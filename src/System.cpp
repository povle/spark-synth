#include "System.h"

SystemClass System;

void SystemClass::begin()
{
    Serial.begin(115200);
    while (!Serial)
        ;

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
    display.begin();

    Serial.println("Initializing instruments...");
    for (uint8_t i = 0; i < NUM_INSTRUMENTS; i++)
    {
        instruments[i]->init();
        Serial.printf("  [%d] %s\n", i, instruments[i]->getName());
    }
    Serial.println();

    switchInstrument(0);
}


void SystemClass::update()
{
    // 2. PROCESS BUTTONS
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

    for (uint8_t i = 2; i <=3; i++)
    {
        if (Hardware.wasButtonJustPressed(i))
        {
            instruments[currentInstrument]->onPressedButton(i);
        }
        else if (Hardware.wasButtonJustReleased(i))
        {
            instruments[currentInstrument]->onReleasedButton(i);
        }
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

    // === INSTRUMENT MENU HANDLING (FINAL FIX) ===


    static bool menuButtonWasPressed = false; // Track button state within menu
    static bool menuReadyForSelect = false;   // True after release

    // Check for long press to OPEN menu
    if (!instrumentMenuActive && Hardware.isButtonPressed(4))
    {
        if (menuLongPressStart == 0)
        {
            menuLongPressStart = millis();
        }
        else if (millis() - menuLongPressStart >= MENU_LONG_PRESS_MS)
        {
            // Menu opening
            instrumentMenuActive = true;
            menuLongPressStart = 0;
            menuReadyForSelect = false;
            menuButtonWasPressed = true; // Button is currently held
            indexToGridPos(currentInstrument, menuCursorRow, menuCursorCol);
            Serial.println("  [Menu] OPEN");
        }
    }
    else if (!Hardware.isButtonPressed(4))
    {
        menuLongPressStart = 0;

        // Track release after menu open
        if (instrumentMenuActive && !menuReadyForSelect)
        {
            menuReadyForSelect = true;
            menuButtonWasPressed = false; // Button now released
            Serial.println("  [Menu] Released - ready for select");
        }
    }

    if (instrumentMenuActive)
    {
        // === MENU MODE ===

        // Joystick navigation
        float jX, jY;
        Controls.readJoystick(jX, jY);

        const float DEADZONE_X = 0.2f;
        const float DEADZONE_Y = 0.15f;
        static unsigned long lastNavTime = 0;

        if (millis() - lastNavTime >= 200)
        {
            bool moved = false;

            // Y-axis: 0.0 to 1.0, center 0.5
            if (jY > (0.5f + DEADZONE_Y) && menuCursorRow > 0)
            {
                menuCursorRow = 0;
                moved = true;
            }
            else if (jY < (0.5f - DEADZONE_Y) && menuCursorRow < 1)
            {
                menuCursorRow = 1;
                moved = true;
            }

            // X-axis: -1.0 to 1.0, center 0.0
            if (jX < -DEADZONE_X && menuCursorCol > 0)
            {
                menuCursorCol--;
                moved = true;
            }
            else if (jX > DEADZONE_X && menuCursorCol < 2)
            {
                menuCursorCol++;
                moved = true;
            }

            if (moved)
                lastNavTime = millis();
        }

        // === SELECT: Track button state locally (NOT wasButtonJustPressed!) ===
        bool buttonIsPressed = Hardware.isButtonPressed(4);

        // Detect FRESH press: was released, now pressed again
        if (menuReadyForSelect && !menuButtonWasPressed && buttonIsPressed)
        {
            uint8_t newIndex = gridPosToIndex(menuCursorRow, menuCursorCol);
            if (newIndex < NUM_INSTRUMENTS)
            {
                Serial.printf("  [System] Selected instrument %d\n", newIndex);
                switchInstrument(newIndex);
            }
            instrumentMenuActive = false;
            menuReadyForSelect = false;
            menuButtonWasPressed = false;
            Serial.println("  [Menu] CLOSE (selected)");
        }

        // Update state for next loop
        menuButtonWasPressed = buttonIsPressed;
    }
    else
    {
        // === NORMAL MODE ===
        float jX, jY;
        Controls.readJoystick(jX, jY);
        instruments[currentInstrument]->updateJoystick(jX, jY);
    }

    // 5. UPDATE OLED (Limit to 10 FPS so it doesn't choke the ESP32)
    static unsigned long lastScreenUpdate = 0;
    if (millis() - lastScreenUpdate > 100)
    {
        lastScreenUpdate = millis();
        updateScreen();
    }

    Hardware.updateBattery();

    instruments[currentInstrument]->update();
}

void SystemClass::inputTask()
{
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
                uint8_t actualNote = baseNote + ((octaveShift - 1) * 12); // constant offset by 1

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
}

void SystemClass::updateScreen()
{
    display.update(
        instruments[currentInstrument], // activeInst
        instruments,                    // all instruments array
        NUM_INSTRUMENTS,                // count
        currentInstrument,              // current index
        instrumentMenuActive,           // menu flag
        menuCursorRow, menuCursorCol,   // cursor position
        octaveShift,                    // octave
        Hardware.getBatteryVoltage()    // battery voltage
    );
}

void SystemClass::switchInstrument(uint8_t index)
{
    if (index >= NUM_INSTRUMENTS)
        return;

    instruments[currentInstrument]->stop();

    amy_reset_oscs();

    currentInstrument = index;
    instruments[currentInstrument]->start();

    Serial.printf("\n✅ Switched to: %s\n\n", instruments[currentInstrument]->getName());
}

void SystemClass::listInstruments()
{
    Serial.println("\nInstruments:");
    for (uint8_t i = 0; i < NUM_INSTRUMENTS; i++)
    {
        Serial.printf("  [%d] %s%s\n", i, instruments[currentInstrument]->getName(),
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
