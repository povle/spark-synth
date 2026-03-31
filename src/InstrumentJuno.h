#ifndef INSTRUMENT_JUNO_H
#define INSTRUMENT_JUNO_H

#include "Instrument.h"

class InstrumentJuno : public Instrument
{
public:
    InstrumentJuno()
    {
        _instrumentName = "JUNO";
    }
    void init() override;
    void start() override;
    void stop() override;
    void onCustomPot(uint8_t channel, float value) override;
    void onPressedButton(uint8_t button_id) override;
    void drawUI(U8G2 &u8g2, uint8_t y_offset) override;

private:
    uint8_t patch = 0;
};

#endif
