#include "System.h"

SystemClass System;

void SystemClass::begin()
{
    Serial.begin(115200);
    while (!Serial)
        ;
    delay(2000);

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
        uint8_t note = Hardware.getMidiNote(row, col);

        if (note > 0)
        {
            if (pressed)
            {
                instruments[currentInstrument]->noteOn(note, 0.8f);
            }
            else
            {
                instruments[currentInstrument]->noteOff(note);
            }
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

    instruments[currentInstrument]->update();
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
