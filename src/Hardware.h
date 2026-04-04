#pragma once

#ifndef HARDWARE_H
#define HARDWARE_H

#include <Arduino.h>
#include <Wire.h>
#include "ESP_I2S.h"
#include <Adafruit_MCP23X17.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

extern "C"
{
#include <amy.h>
}

#define I2S_LRC_PIN 4
#define I2S_DIN_PIN 5
#define I2S_BCK_PIN 6

#define BATTERY_PIN 7
#define BATTERY_CALIBRATION_FACTOR 3.38f

#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9

#define OLED_SDA_PIN 10
#define OLED_SCL_PIN 11

#define MUX_S0 13
#define MUX_S1 14
#define MUX_S2 15
#define MUX_S3 16
#define MUX_SIG 17

#define NUM_ROWS 6
#define NUM_COLS 5

#define DISABLE_SERIAL_OUTPUT 1

#if DISABLE_SERIAL_OUTPUT
class NullSerial
{
public:
    template <typename... T>
    void begin(T... args) {}
    template <typename... T>
    void print(T... args) {}
    template <typename... T>
    void println(T... args) {}
    template <typename... T>
    void printf(T... args) {}

    // Stops handleSerialCommand() from freezing
    int available() { return 0; }
    int read() { return -1; }

    // Stops while(!Serial) from freezing
    operator bool() { return true; }
};

static NullSerial DummySerial;
#define Serial DummySerial
#endif

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

    // Button API
    bool isButtonPressed(uint8_t index);
    bool wasButtonJustPressed(uint8_t index);
    bool wasButtonJustReleased(uint8_t index);

    void updateBattery();
    int getBatteryPercentage();
    float getBatteryVoltage();

private:
    Adafruit_MCP23X17 mcp;

    uint8_t rowPins[NUM_ROWS] = {0, 1, 2, 3, 4, 5};   // PA0-PA5
    uint8_t colPins[NUM_COLS] = {11, 12, 13, 14, 15}; // PB3-PB7
    uint8_t noteMap[NUM_ROWS][NUM_COLS];

    // The 5 Buttons: PA6(6), PA7(7), PB0(8), PB1(9), PB2(10)
    uint8_t buttonPins[5] = {6, 10, 9, 7, 8};
    bool buttonState[5] = {false};
    bool buttonJustPressed[5] = {false};
    bool buttonJustReleased[5] = {false};

    bool keyState[NUM_ROWS][NUM_COLS] = {false};

    static const uint8_t QUEUE_SIZE = 32;
    KeyEvent keyQueue[QUEUE_SIZE];
    uint8_t queueHead = 0;
    uint8_t queueTail = 0;

    void initNoteMap();

    unsigned long _lastBatteryRead = 0;
    float _smoothedBatteryVolts = -1.0f; // -1 indicates it hasn't been read yet
};

extern HardwareClass Hardware;

#endif
