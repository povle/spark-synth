#ifndef INSTRUMENT_MOOG_H
#define INSTRUMENT_MOOG_H
#include "../src/Instrument.h"
class InstrumentMoog : public Instrument
{
public:
    void init() override { Serial.println("  [Moog] Stub"); }
    void noteOn(uint8_t note, float velocity) override {}
    void noteOff(uint8_t note) override {}
    void updatePot(uint8_t channel, float value) override {}
};
#endif
