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
    cfg.features.reverb = 1;
    cfg.features.echo = 1;
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

    for (int i = 0; i < 100; i++)
    {
        amy_update();
    }

    Serial.println("   Voices allocated\n");

    Wire1.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    Wire1.setClock(400000);
    if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);
        display.display();
        Serial.println("   OLED OK");
    }

    // === MCP23017 ===
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(400000);
    // Wire.setClock(100000);
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
        for (uint8_t c = 0; c < 5; c++)
        {
            mcp.pinMode(buttonPins[c], INPUT_PULLUP);
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
    // Initialize all to Middle C (60) as a fallback
    for (uint8_t r = 0; r < NUM_ROWS; r++)
    {
        for (uint8_t c = 0; c < NUM_COLS; c++)
        {
            noteMap[r][c] = 60;
        }
    }

    // Mapping logic:
    // Old Col 7 (PB7) -> New Col 4
    // Old Col 6 (PB6) -> New Col 3
    // Old Col 5 (PB5) -> New Col 2
    // Old Col 4 (PB4) -> New Col 1
    // Old Col 3 (PB3) -> New Col 0

    // Row 0
    noteMap[0][4] = 57;
    noteMap[0][3] = 58;
    noteMap[0][2] = 73;
    noteMap[0][1] = 67;
    noteMap[0][0] = 77;

    // Row 1
    noteMap[1][4] = 59;
    noteMap[1][3] = 61;
    noteMap[1][2] = 75;
    noteMap[1][1] = 69;
    noteMap[1][0] = 79;

    // Row 2
    noteMap[2][4] = 60;
    noteMap[2][3] = 63;
    noteMap[2][2] = 78;
    noteMap[2][1] = 71;
    noteMap[2][0] = 81;

    // Row 3
    noteMap[3][4] = 62;
    noteMap[3][3] = 66;
    noteMap[3][2] = 80;
    noteMap[3][1] = 72;
    noteMap[3][0] = 83;

    // Row 4
    noteMap[4][4] = 64;
    noteMap[4][3] = 68;
    noteMap[4][2] = 82;
    noteMap[4][1] = 74;
    noteMap[4][0] = 84;

    // Row 5
    noteMap[5][4] = 65;
    noteMap[5][3] = 70;
    noteMap[5][2] = 0; // Unmapped/Dead key
    noteMap[5][1] = 76;
    noteMap[5][0] = 0; // Unmapped/Dead key
}
void HardwareClass::scanKeyboard()
{
    uint16_t allPins = 0;

    // --- 1. SCAN MATRIX ---
    for (uint8_t r = 0; r < NUM_ROWS; r++)
    {
        // Drive current row LOW
        mcp.digitalWrite(rowPins[r], LOW);

        // Take a snapshot of all 16 pins on the MCP23017 (Takes ~0.1ms)
        allPins = mcp.readGPIOAB();

        for (uint8_t c = 0; c < NUM_COLS; c++)
        {
            // Extract the specific column bit from the snapshot
            // If the bit is 0, the key is pulled LOW (pressed)
            bool isPressed = ((allPins & (1 << colPins[c])) == 0);

            if (isPressed != keyState[r][c])
            {
                keyState[r][c] = isPressed;

                // Push to the Event Queue
                uint8_t nextHead = (queueHead + 1) % QUEUE_SIZE;
                if (nextHead != queueTail)
                {
                    keyQueue[queueHead] = {r, c, isPressed};
                    queueHead = nextHead;
                }
            }
        }

        // Drive row back HIGH before moving to the next one
        mcp.digitalWrite(rowPins[r], HIGH);
    }

    // --- 2. SCAN THE 5 BUTTONS ---
    // Since the buttons are wired directly to GND (independent of the rows),
    // their state was accurately captured in 'allPins' during the last row scan.
    // We can just reuse that exact same snapshot to read them, saving I2C time!
    for (uint8_t b = 0; b < 5; b++)
    {
        // Extract the specific button bit from the snapshot
        bool isPressed = ((allPins & (1 << buttonPins[b])) == 0);

        if (isPressed && !buttonState[b])
        {
            buttonJustPressed[b] = true; // Mark as just fired
        }
        buttonState[b] = isPressed;
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

bool HardwareClass::isButtonPressed(uint8_t index)
{
    if (index >= 5)
        return false;
    return buttonState[index];
}

bool HardwareClass::wasButtonJustPressed(uint8_t index)
{
    if (index >= 5)
        return false;
    if (buttonJustPressed[index])
    {
        buttonJustPressed[index] = false; // Clear flag after reading
        return true;
    }
    return false;
}

uint8_t HardwareClass::getMidiNote(uint8_t row, uint8_t col)
{
    if (row >= NUM_ROWS || col >= NUM_COLS)
        return 60;
    return noteMap[row][col];
}
