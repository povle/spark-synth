#ifndef INSTRUMENT_SINE_H
#define INSTRUMENT_SINE_H

#include "Instrument.h"

class InstrumentSine : public Instrument
{
public:
    void init() override;
    void start() override;
    void stop() override;
    void noteOn(uint8_t note, float velocity) override;
    void noteOff(uint8_t note) override;
    void updatePot(uint8_t channel, float value) override;
};

#endif
