#ifndef INSTRUMENT_JUNO_H
#define INSTRUMENT_JUNO_H

#include "Instrument.h"

class InstrumentJuno : public Instrument
{
public:
    void init() override;
    void start() override;
    void stop() override;
    void onCustomPot(uint8_t channel, float value) override;
    void onPressedButton(uint8_t button_id) override;

private:
    uint8_t junoPatch = 0;
};

#endif
