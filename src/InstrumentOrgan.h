#ifndef INSTRUMENT_ORGAN_H
#define INSTRUMENT_ORGAN_H
#include "../src/Instrument.h"
class InstrumentOrgan : public Instrument
{
public:
    void init() override { Serial.println("  [Organ] Stub"); }
    void noteOn(uint8_t note, float velocity) override {}
    void noteOff(uint8_t note) override {}
    void updatePot(uint8_t channel, float value) override {}
};
#endif
