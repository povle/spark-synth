#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "Instrument.h" // Ensure this matches your base class filename

class InstrumentBLE : public Instrument
{
public:
    void init() override;
    void start() override;
    void stop() override;
    void update() override;

    void noteOn(uint8_t note, float velocity) override;
    void noteOff(uint8_t note) override;
    void updatePot(uint8_t channel, float value) override;

    // New handlers
    void updateJoystick(float x, float y) override;
    void onPressedButton(uint8_t button_id) override;

    bool isConnected = false;

private:
    BLEServer *pServer = nullptr;
    BLECharacteristic *pCharacteristic = nullptr;
    void sendMidi(uint8_t status, uint8_t data1, uint8_t data2);

    // Track joystick state to prevent redundant BLE packets
    float _lastJoyX = -100.0f;
    float _lastJoyY = -100.0f;
};
