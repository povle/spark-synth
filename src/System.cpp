#include "System.h"

SystemClass System;

void SystemClass::begin()
{
    Serial.begin(115200);
    while (!Serial)
        ;
    delay(1000);

    Serial.println("\n========================================");
    Serial.println("  ESP32-S3 AMY Synth (Dual Core)");
    Serial.println("========================================\n");

    Serial.printf("Chip Model: %s\n", ESP.getChipModel());
    Serial.printf("CPU Cores: %d\n", ESP.getChipCores());
    Serial.printf("CPU Frequency: %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("Free Heap: %lu bytes\n", ESP.getFreeHeap());
    Serial.println();

    Hardware.begin();
    Controls.begin();

    Serial.println("Initializing instruments...");
    for (uint8_t i = 0; i < NUM_INSTRUMENTS; i++)
    {
        Serial.printf("  [%d] %s\n", i, instrumentNames[i]);
        instruments[i]->init();
    }
    Serial.println();

    switchInstrument(0);

    Serial.println("========================================");
    Serial.println("  READY! Commands: inst, list, test");
    Serial.println("========================================\n");
}

void SystemClass::update()
{
    Hardware.audioPump();
    handleSerialCommand();

    // 1. Scan the hardware to populate the queue
    Hardware.scanKeyboard();

    // 2. Process ALL pending key events in the queue
    uint8_t row, col;
    bool pressed;

    while (Hardware.getNextKeyEvent(row, col, pressed))
    {
        uint8_t baseNote = Hardware.getMidiNote(row, col);

        if (baseNote > 0)
        {
            if (pressed)
            {
                // Calculate the note with the CURRENT octave shift
                uint8_t actualNote = baseNote + (octaveShift * 12);

                // Save this specific note to the physical key's slot
                activeNotes[row][col] = actualNote;

                instruments[currentInstrument]->noteOn(actualNote, 0.8f);
            }
            else
            {
                // Retrieve the EXACT note that was playing on this physical key
                uint8_t actualNote = activeNotes[row][col];

                if (actualNote > 0)
                {
                    instruments[currentInstrument]->noteOff(actualNote);

                    // Clear the slot
                    activeNotes[row][col] = 0;
                }
            }
        }
    }

    // 2. PROCESS BUTTONS
    // Example: Button 0 = Octave Down, Button 1 = Octave Up
    if (Hardware.wasButtonJustPressed(0))
    {
        octaveShift--;
        Serial.printf("Octave: %d\n", octaveShift);
    }
    if (Hardware.wasButtonJustPressed(1))
    {
        octaveShift++;
        Serial.printf("Octave: %d\n", octaveShift);
    }

    // Button 4 (Assuming this is the Joystick Switch) -> Long press to change instrument
    static unsigned long joyPressTime = 0;
    if (Hardware.wasButtonJustPressed(4))
    {
        joyPressTime = millis();
    }
    // If it's currently held down for more than 1 second
    if (Hardware.isButtonPressed(4) && joyPressTime > 0 && (millis() - joyPressTime > 1000))
    {
        joyPressTime = 0; // Reset so it doesn't fire repeatedly
        uint8_t nextInst = (currentInstrument + 1) % NUM_INSTRUMENTS;
        switchInstrument(nextInst);
    }
    // Clear the timer if released early
    if (!Hardware.isButtonPressed(4))
    {
        joyPressTime = 0;
    }

    // 3. Process Potentiometers
    for (uint8_t i = 0; i < 16; i++)
    {
        float value;
        if (Controls.readPot(i, value))
        {
            instruments[currentInstrument]->updatePot(i, value);
        }
    }

    // 4. PROCESS JOYSTICK
    float jX, jY;
    Controls.readJoystick(jX, jY);

    // Send Pitch Bend (jX ranges -1.0 to 1.0)
    // Convert to AMY ratio (e.g., +/- 2 semitones)
    float bendRatio = powf(2.0f, (jX * 2.0f) / 12.0f);

    amy_event e = amy_default_event();
    e.synth = 1; // Send to active synth
    e.pitch_bend = bendRatio;
    amy_add_event(&e);

    // 5. UPDATE OLED (Limit to 10 FPS so it doesn't choke the ESP32)
    static unsigned long lastScreenUpdate = 0;
    if (millis() - lastScreenUpdate > 100)
    {
        lastScreenUpdate = millis();
        updateScreen();
        // Serial.printf("jX = %f jY = %f\n", jX, jY);
    }

    instruments[currentInstrument]->update();
}

void SystemClass::updateScreen()
{
    Hardware.display.clearDisplay();

    Hardware.display.setCursor(0, 0);
    Hardware.display.setTextSize(2);
    Hardware.display.println(instrumentNames[currentInstrument]);

    Hardware.display.setCursor(0, 24);
    Hardware.display.setTextSize(1);
    Hardware.display.printf("Octave: %+d\n", octaveShift);

    Hardware.display.display();
}

void SystemClass::switchInstrument(uint8_t index)
{
    if (index >= NUM_INSTRUMENTS)
        return;

    instruments[currentInstrument]->stop();

    amy_reset_oscs();

    currentInstrument = index;
    instruments[currentInstrument]->start();

    Serial.printf("\n✅ Switched to: %s\n\n", instrumentNames[index]);
}

void SystemClass::listInstruments()
{
    Serial.println("\nInstruments:");
    for (uint8_t i = 0; i < NUM_INSTRUMENTS; i++)
    {
        Serial.printf("  [%d] %s%s\n", i, instrumentNames[i],
                      (i == currentInstrument) ? " ←" : "");
    }
    Serial.println();
}

void SystemClass::handleSerialCommand()
{
    static String cmd = "";
    while (Serial.available())
    {
        char c = Serial.read();
        if (c == '\n' || c == '\r')
        {
            cmd.trim();

            if (cmd == "test")
            {
                Serial.println("Test chord (C-E-G)...");
                amy_event e = amy_default_event();
                e.synth = 1;
                e.midi_note = 60;
                e.velocity = 0.8f;
                amy_add_event(&e);
                e.midi_note = 64;
                amy_add_event(&e);
                e.midi_note = 67;
                amy_add_event(&e);
            }
            else if (cmd == "list")
                listInstruments();
            else if (cmd == "heap")
            {
                Serial.printf("\nFree Heap: %lu bytes\n\n", ESP.getFreeHeap());
            }
            else if (cmd.startsWith("inst "))
            {
                uint8_t idx = cmd.substring(5).toInt();
                switchInstrument(idx);
            }
            cmd = "";
        }
        else
        {
            cmd += c;
        }
    }
}
