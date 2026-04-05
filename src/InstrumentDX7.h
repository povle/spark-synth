#ifndef INSTRUMENT_DX7_H
#define INSTRUMENT_DX7_H

#include "Instrument.h"

class InstrumentDX7 : public Instrument
{
public:
    InstrumentDX7()
    {
        _instrumentName = "DX7";
        _instrumentShortName = "DX7";
    }
    void init() override;
    void start() override;
    void stop() override;
    void onCustomPot(uint8_t channel, float value) override;
    void sendAdsr() override;
    void onPressedButton(uint8_t button_id) override;
    void drawUI(U8G2 &u8g2, uint8_t y_offset) override;

    private : uint8_t patch = 0;
};

#endif
