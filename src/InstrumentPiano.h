#ifndef INSTRUMENT_PIANO_H
#define INSTRUMENT_PIANO_H

#include "Instrument.h"

class InstrumentPiano : public Instrument
{
public:
    InstrumentPiano()
    {
        _instrumentName = "Piano";
        _instrumentShortName = "Piano";
    }
    void init() override;
    void start() override;
    void stop() override;
};

#endif
