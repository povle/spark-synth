#include "Hardware.h"

HardwareClass Hardware;

void HardwareClass::begin()
{
    Serial.println("-> Initializing Hardware...");
    Serial.printf("   Free Heap: %lu bytes\n", ESP.getFreeHeap());
    Serial.printf("   CPU Cores: %d\n", ESP.getChipCores());

    // === AMY Config (from AMY_MIDI_Synth.ino + AMY_USB_Host_MIDI) ===
    amy_config_t cfg = amy_default_config();
    cfg.audio = AMY_AUDIO_IS_I2S;
    cfg.i2s_bclk = I2S_BCK_PIN;
    cfg.i2s_lrc = I2S_LRC_PIN;
    cfg.i2s_dout = I2S_DIN_PIN;
    cfg.features.startup_bleep = 1;
    cfg.features.default_synths = 1; // Creates synth 1, 2, 10
    cfg.features.reverb = 0;         // Disabled - saves CPU/memory
    cfg.features.echo = 0;           // Disabled - saves CPU/memory
    cfg.features.chorus = 0;
    cfg.max_oscs = 128;
    cfg.max_voices = 32;
    cfg.max_synths = 16;
    cfg.midi = AMY_MIDI_IS_NONE;

    // === CRITICAL: Enable Multicore (from AMY config docs) ===
    cfg.platform.multicore = 1;   // ← Use both ESP32-S3 cores!
    cfg.platform.multithread = 1; // ← Use RTOS multithreading

    amy_start(cfg);
    Serial.println("   AMY OK (listen for bleep!)");
    Serial.printf("   Free Heap after AMY: %lu bytes\n", ESP.getFreeHeap());

    // === RESET_AMY + Voice Alloc ONCE (from test_polyphony) ===
    Serial.println("   Resetting AMY and allocating 12 voices to synth 1...");

    amy_event e = amy_default_event();
    e.reset_osc = RESET_AMY;
    amy_add_event(&e);

    // e = amy_default_event();
    // e.synth = 1;
    // e.num_voices = 12;
    // e.patch_number = 1;
    // amy_add_event(&e);

    for (int i = 0; i < 100; i++)
    {
        amy_update();
    }

    Serial.println("   Voices allocated\n");

    // === MCP23017 ===
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(400000);
    if (mcp.begin_I2C(0x20, &Wire))
    {
        for (uint8_t r = 0; r < NUM_ROWS; r++)
        {
            mcp.pinMode(rowPins[r], OUTPUT);
            mcp.digitalWrite(rowPins[r], HIGH);
        }
        for (uint8_t c = 0; c < NUM_COLS; c++)
        {
            mcp.pinMode(colPins[c], INPUT_PULLUP);
        }
        Serial.println("   MCP23017 OK");
    }

    // === MUX ===
    pinMode(MUX_S0, OUTPUT);
    pinMode(MUX_S1, OUTPUT);
    pinMode(MUX_S2, OUTPUT);
    pinMode(MUX_S3, OUTPUT);
    pinMode(MUX_SIG, INPUT);
    Serial.println("   MUX OK");

    initNoteMap();
    Serial.println("   Hardware Ready\n");
}

void HardwareClass::audioPump()
{
    amy_update(); // AMY handles I2S internally
}

void HardwareClass::initNoteMap()
{
    for (uint8_t r = 0; r < NUM_ROWS; r++)
    {
        for (uint8_t c = 0; c < NUM_COLS; c++)
        {
            noteMap[r][c] = 60;
        }
    }

    noteMap[0][7] = 57;
    noteMap[0][6] = 58;
    noteMap[0][5] = 73;
    noteMap[0][4] = 67;
    noteMap[0][3] = 77;
    noteMap[1][7] = 59;
    noteMap[1][6] = 61;
    noteMap[1][5] = 75;
    noteMap[1][4] = 69;
    noteMap[1][3] = 79;
    noteMap[2][7] = 60;
    noteMap[2][6] = 63;
    noteMap[2][5] = 78;
    noteMap[2][4] = 71;
    noteMap[2][3] = 81;
    noteMap[3][7] = 62;
    noteMap[3][6] = 66;
    noteMap[3][5] = 80;
    noteMap[3][4] = 72;
    noteMap[3][3] = 83;
    noteMap[4][7] = 64;
    noteMap[4][6] = 68;
    noteMap[4][5] = 82;
    noteMap[4][4] = 74;
    noteMap[4][3] = 84;
    noteMap[5][7] = 65;
    noteMap[5][6] = 70;
    noteMap[5][5] = 0;
    noteMap[5][4] = 76;
    noteMap[5][3] = 0;
}

void HardwareClass::scanKeyboard()
{
    for (uint8_t r = 0; r < NUM_ROWS; r++)
    {
        mcp.digitalWrite(rowPins[r], LOW);
        for (uint8_t c = 0; c < NUM_COLS; c++)
        {
            bool isPressed = (mcp.digitalRead(colPins[c]) == LOW);
            if (isPressed != keyState[r][c])
            {
                keyState[r][c] = isPressed;

                // Calculate next position in the ring buffer
                uint8_t nextHead = (queueHead + 1) % QUEUE_SIZE;

                // If queue is not full, push the event
                if (nextHead != queueTail)
                {
                    keyQueue[queueHead] = {r, c, isPressed};
                    queueHead = nextHead;
                }
            }
        }
        mcp.digitalWrite(rowPins[r], HIGH);
    }
}

bool HardwareClass::getNextKeyEvent(uint8_t &row, uint8_t &col, bool &pressed)
{
    // If head == tail, the queue is empty
    if (queueHead == queueTail)
        return false;

    // Pop the oldest event
    row = keyQueue[queueTail].row;
    col = keyQueue[queueTail].col;
    pressed = keyQueue[queueTail].pressed;

    queueTail = (queueTail + 1) % QUEUE_SIZE;
    return true;
}

uint8_t HardwareClass::getMidiNote(uint8_t row, uint8_t col)
{
    if (row >= NUM_ROWS || col >= NUM_COLS)
        return 60;
    return noteMap[row][col];
}
