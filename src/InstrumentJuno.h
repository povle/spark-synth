#ifndef INSTRUMENT_JUNO_H
#define INSTRUMENT_JUNO_H

#include "Instrument.h"

class InstrumentJuno : public Instrument
{
public:
    void init() override;
    void start() override;
    void stop() override;
    void noteOn(uint8_t note, float velocity) override;
    void noteOff(uint8_t note) override;
    void updatePot(uint8_t channel, float value) override;

private:
    uint8_t junoPatch = 1;
};

#endif
