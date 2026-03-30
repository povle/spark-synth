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

#define NUM_INSTRUMENTS 3

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
    DisplayManager display;

    Instrument *instruments[NUM_INSTRUMENTS] = {
        new InstrumentDX7(),
        new InstrumentJuno(),
        // new InstrumentPiano(),
        new InstrumentBLE()};

    const char *instrumentNames[NUM_INSTRUMENTS] = {
        "DX-7",
        "Juno",
        // "Piano",
        "BLE MIDI"};
};

extern SystemClass System;

#endif
