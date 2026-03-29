#ifndef SYSTEM_H
#define SYSTEM_H

#include <Arduino.h>
#include "Hardware.h"
#include "Controls.h"
#include "Instrument.h"
#include "InstrumentSine.h"
#include "InstrumentDX7.h"
#include "InstrumentJuno.h"
#include "InstrumentBLE.h"

#define NUM_INSTRUMENTS 4

class SystemClass
{
public:
    void begin();
    void update();
    void switchInstrument(uint8_t index);
    void listInstruments();
    void handleSerialCommand();
    void updateScreen();

    uint8_t getCurrentInstrument() { return currentInstrument; }

private:
    uint8_t currentInstrument = 0;
    uint8_t activeNotes[NUM_ROWS][NUM_COLS] = {0};
    int octaveShift = 0;

    Instrument *instruments[NUM_INSTRUMENTS] = {
        new InstrumentDX7(),
        new InstrumentJuno(),
        new InstrumentSine(),
        new InstrumentBLE()};

    const char *instrumentNames[NUM_INSTRUMENTS] = {
        "DX-7",
        "Juno",
        "Sine Wave",
        "BLE MIDI"};
};

extern SystemClass System;

#endif
