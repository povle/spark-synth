#include "InstrumentBLE.h"

#define MIDI_SERVICE_UUID "03b80e5a-ede8-4b33-a751-6ce34ec4c700"
#define MIDI_CHARACTERISTIC_UUID "7772e5db-3868-4112-a1a9-f2669d106bf3"

// BLE Server Callback to track connection state
class BleServerCallbacks : public BLEServerCallbacks
{
    InstrumentBLE *_inst;

public:
    BleServerCallbacks(InstrumentBLE *inst) : _inst(inst) {}
    void onConnect(BLEServer *pServer) override
    {
        _inst->isConnected = true;
        Serial.println("  [BLE] Connected!");
    }
    void onDisconnect(BLEServer *pServer) override
    {
        _inst->isConnected = false;
        Serial.println("  [BLE] Disconnected. Advertising...");
        pServer->startAdvertising();
    }
};

void InstrumentBLE::init()
{
    Serial.println("  [BLE] Initializing BLE Stack...");

    // Set the broadcast name to "Spark Synth"
    BLEDevice::init("Spark Synth");

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new BleServerCallbacks(this));

    BLEService *pService = pServer->createService(BLEUUID(MIDI_SERVICE_UUID));
    pCharacteristic = pService->createCharacteristic(
        BLEUUID(MIDI_CHARACTERISTIC_UUID),
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_NOTIFY |
            BLECharacteristic::PROPERTY_WRITE_NR);

    pCharacteristic->addDescriptor(new BLE2902());
    pService->start();

    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->addServiceUUID(pService->getUUID());
    pAdvertising->start();

    Serial.println("  [BLE] Initialized. Device Name: Spark Synth");
}

void InstrumentBLE::start()
{
    isActive = true;
    Serial.println("  [BLE] Ready. Waiting for connection...");
}

void InstrumentBLE::stop()
{
    isActive = false;
    Serial.println("  [BLE] Stopped.");
}

void InstrumentBLE::update()
{
    // No continuous polling required for BLE
}

void InstrumentBLE::sendMidi(uint8_t status, uint8_t data1, uint8_t data2)
{
    if (!isConnected || !pCharacteristic)
        return;

    // Standard BLE MIDI 5-byte packet structure
    uint8_t midiPacket[5];
    midiPacket[0] = 0x80; // Header (timestamp high)
    midiPacket[1] = 0x80; // Timestamp low
    midiPacket[2] = status;
    midiPacket[3] = data1;
    midiPacket[4] = data2;

    pCharacteristic->setValue(midiPacket, 5);
    pCharacteristic->notify();
}

void InstrumentBLE::noteOn(uint8_t note, float velocity)
{
    if (!isActive)
        return;

    uint8_t vel = (uint8_t)(velocity * 127.0f);
    if (vel == 0)
        vel = 1;

    sendMidi(0x90, note, vel);
}

void InstrumentBLE::noteOff(uint8_t note)
{
    if (!isActive)
        return;

    sendMidi(0x80, note, 0);
}

void InstrumentBLE::updatePot(uint8_t channel, float value)
{
    if (!isActive)
        return;

    // Map your 16 multiplexer pots to standard MIDI CCs
    // (Adjust these CC numbers depending on what parameters you want to control in your DAW)
    uint8_t ccMap[16] = {
        14, 15, 16, 17, // Ch 0-3: Custom
        74, 71,         // Ch 4-5: Cutoff, Res
        0, 0,           // Ch 6-7: Unused
        73, 75,         // Ch 8-9: Attack, Decay
        31, 72,         // Ch 10-11: Sustain, Release
        91, 93,         // Ch 12-13: Reverb, Delay
        0, 7            // Ch 14-15: Unused, Volume
    };

    uint8_t ccNum = ccMap[channel];
    if (ccNum > 0)
    {
        uint8_t ccVal = (uint8_t)(value * 127.0f);
        sendMidi(0xB0, ccNum, ccVal);
    }
}
