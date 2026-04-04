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

    bool instrumentMenuActive = false;
    uint8_t menuCursorRow = 0; // 0 or 1
    uint8_t menuCursorCol = 0; // 0, 1, or 2
    unsigned long menuLongPressStart = 0;
    static constexpr unsigned long MENU_LONG_PRESS_MS = 800; // 0.8s to open menu

    // Helper to get instrument index from grid position
    uint8_t gridPosToIndex(uint8_t row, uint8_t col)
    {
        return row * 3 + col; // 3 columns per row
    }

    // Helper to get grid position from instrument index
    void indexToGridPos(uint8_t index, uint8_t &row, uint8_t &col)
    {
        row = index / 3;
        col = index % 3;
    }
};

extern SystemClass System;

#endif
