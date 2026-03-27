#ifndef HARDWARE_H
#define HARDWARE_H

#include <Arduino.h>
#include <Wire.h>
#include "ESP_I2S.h"
#include <Adafruit_MCP23X17.h>

extern "C"
{
#include <amy.h>
}

#define I2S_BCK_PIN 6
#define I2S_LRC_PIN 4
#define I2S_DIN_PIN 5
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9
#define MUX_S0 13
#define MUX_S1 14
#define MUX_S2 15
#define MUX_S3 16
#define MUX_SIG 17

#define NUM_ROWS 6
#define NUM_COLS 8

struct KeyEvent
{
    uint8_t row;
    uint8_t col;
    bool pressed;
};

class HardwareClass
{
public:
    void scanKeyboard(); // No longer needs to return bool
    bool getNextKeyEvent(uint8_t &row, uint8_t &col, bool &pressed);
    void begin();
    void audioPump();
    uint8_t getMidiNote(uint8_t row, uint8_t col);


private:
    Adafruit_MCP23X17 mcp;
    const uint8_t rowPins[NUM_ROWS] = {0, 1, 2, 3, 4, 5};
    const uint8_t colPins[NUM_COLS] = {8, 9, 10, 11, 12, 13, 14, 15};
    bool keyState[NUM_ROWS][NUM_COLS] = {false};

    static const uint8_t QUEUE_SIZE = 32; // Big enough for a keyboard mash
    KeyEvent keyQueue[QUEUE_SIZE];
    uint8_t queueHead = 0;
    uint8_t queueTail = 0;

    uint8_t noteMap[NUM_ROWS][NUM_COLS];
    void initNoteMap();
};

extern HardwareClass Hardware;

#endif
