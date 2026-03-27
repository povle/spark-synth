#ifndef INSTRUMENT_DX7_H
#define INSTRUMENT_DX7_H

#include "Instrument.h"

class InstrumentDX7 : public Instrument
{
public:
    void init() override;
    void start() override;
    void stop() override;
    void noteOn(uint8_t note, float velocity) override;
    void noteOff(uint8_t note) override;
    void onCustomPot(uint8_t channel, float value) override;
    void sendAdsr() override;

    private : uint8_t dxPatch = 128;
};

#endif
