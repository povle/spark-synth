#ifndef SYSTEM_H
#define SYSTEM_H

#include <Arduino.h>
#include "Hardware.h"
#include "DisplayManager.h"
#include "Controls.h"
#include "Instrument.h"
#include "InstrumentPiano.h"
#include "InstrumentDX7.h"
#include "InstrumentJuno.h"
#include "InstrumentBLE.h"
#include "InstrumentSampler.h"

#define NUM_INSTRUMENTS 5

class SystemClass
{
public:
    void begin();
    void update();
    void switchInstrument(uint8_t index);
    void listInstruments();
    void handleSerialCommand();
    void updateScreen();
    void inputTask();
    bool isBleActive() { return currentInstrument == 4; }

    uint8_t getCurrentInstrument() { return currentInstrument; }

    volatile bool useBackgroundTask = true;

private:
    uint8_t currentInstrument = 0;
    uint8_t activeNotes[NUM_ROWS][NUM_COLS] = {0};
    int octaveShift = 0;
    DisplayManager display;

    Instrument *instruments[NUM_INSTRUMENTS] = {
        new InstrumentDX7(),
        new InstrumentJuno(),
        new InstrumentSampler(),
        new InstrumentPiano(),
        new InstrumentBLE() // !!update isBleActive after moving this!!
    };
};

extern SystemClass System;

#endif
