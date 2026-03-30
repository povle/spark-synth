#ifndef INSTRUMENT_DX7_H
#define INSTRUMENT_DX7_H

#include "Instrument.h"

class InstrumentDX7 : public Instrument
{
public:
    void init() override;
    void start() override;
    void stop() override;
    void onCustomPot(uint8_t channel, float value) override;
    void sendAdsr() override;
    void onPressedButton(uint8_t button_id) override;

private:
    uint8_t dxPatch = 0;
};

#endif
