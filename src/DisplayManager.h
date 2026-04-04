#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "Instrument.h"

class DisplayManager
{
public:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

    DisplayManager() : u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL_PIN, OLED_SDA_PIN) {}

    void begin()
    {
        u8g2.begin();
        Serial.println("Display OK");
        // Initialize to force first redraw
        _lastOctave = -999;
        _lastBatteryV = -1.0f;
        _lastInstName = nullptr;
        _lastCurrentInstrument = 255;
        _lastMenuCursorRow = 255;
        _lastMenuCursorCol = 255;
    }

    void update(Instrument *activeInst, Instrument *instruments[],
                uint8_t numInstruments, uint8_t currentInstrument,
                bool menuActive, uint8_t menuCursorRow, uint8_t menuCursorCol,
                int octave, float batteryV)
    {
        const char* instName = activeInst ? activeInst->getName() : "MENU";

        // Check if redraw is needed (includes instrument flag check)
        if (!_needsRedraw(activeInst, octave, batteryV, instName, currentInstrument,
                         menuActive, menuCursorRow, menuCursorCol)) {
            return;  // Nothing changed - skip redraw entirely
        }

        // === REDRAW ===
        u8g2.clearBuffer();
        _drawHeader(octave, batteryV, instName);

        if (menuActive) {
            drawInstrumentMenu(instruments, numInstruments, currentInstrument,
                               menuCursorRow, menuCursorCol);
        } else if (activeInst) {
            activeInst->drawUI(u8g2, 12);  // headerHeight + 2
            activeInst->needsUIRedraw = false;  // Reset flag AFTER drawing
        }

        u8g2.sendBuffer();

        // Update state for next frame
        _lastOctave = octave;
        _lastBatteryV = batteryV;
        _lastInstName = instName;
        _lastCurrentInstrument = currentInstrument;
        _lastMenuCursorRow = menuCursorRow;
        _lastMenuCursorCol = menuCursorCol;
    }

private:
    int _lastOctave = -999;
    float _lastBatteryV = -1.0f;
    const char* _lastInstName = nullptr;
    uint8_t _lastCurrentInstrument = 255;
    uint8_t _lastMenuCursorRow = 255;
    uint8_t _lastMenuCursorCol = 255;

    bool _needsRedraw(Instrument* activeInst, int octave, float batteryV,
                      const char* instName, uint8_t currentInstrument,
                      bool menuActive, uint8_t menuCursorRow, uint8_t menuCursorCol) {
        float batteryRounded = roundf(batteryV * 100.0f) / 100.0f;
        float lastBatteryRounded = roundf(_lastBatteryV * 100.0f) / 100.0f;

        if (octave != _lastOctave) return true;
        if (batteryRounded != lastBatteryRounded) return true;
        if (instName != _lastInstName) return true;
        if (currentInstrument != _lastCurrentInstrument) return true;

        // Menu cursor changes (only when menu is active)
        if (menuActive) {
            if (menuCursorRow != _lastMenuCursorRow) return true;
            if (menuCursorCol != _lastMenuCursorCol) return true;
            return true;  // Menu just opened/closed
        }

        // Instrument UI flag - checked internally
        if (activeInst && activeInst->needsUIRedraw) return true;
        if (activeInst && activeInst->liveUI) return true;

        return false;
    }

    // Draw header bar
    void _drawHeader(int octave, float batteryV, const char* instName) {
        u8g2.setFont(u8g2_font_5x7_tr);
        u8g2.setCursor(2, 7);
        u8g2.printf("OCT:%+d", octave);

        int screenWidth = u8g2.getDisplayWidth();
        int textWidth = u8g2.getStrWidth(instName);
        int startX = (screenWidth - textWidth) / 2;

        u8g2.setFont(u8g2_font_resoledbold_tr);
        u8g2.setCursor(startX, 7);
        u8g2.print(instName);

        u8g2.setFont(u8g2_font_5x7_tr);
        u8g2.setCursor(100, 7);
        u8g2.printf("%.2fV", batteryV);

        u8g2.drawHLine(0, 10, 128);
    }

    // Grid menu drawing
    void drawInstrumentMenu(Instrument *instruments[], uint8_t numInstruments,
                            uint8_t currentInstrument, uint8_t cursorRow, uint8_t cursorCol)
    {
        const uint8_t CELL_W = 42;
        const uint8_t CELL_H = 23;
        const uint8_t MENU_Y = 12;

        u8g2.setFont(u8g2_font_6x10_tr);

        for (uint8_t row = 0; row < 2; row++) {
            for (uint8_t col = 0; col < 3; col++) {
                uint8_t index = row * 3 + col;
                uint8_t x = col * CELL_W;
                uint8_t y = MENU_Y + row * CELL_H;

                bool isCursor = (row == cursorRow && col == cursorCol);

                if (isCursor) {
                    u8g2.setDrawColor(1);
                    u8g2.drawBox(x + 1, y + 1, CELL_W - 2, CELL_H - 2);
                    u8g2.setDrawColor(0);
                } else {
                    u8g2.setDrawColor(1);
                    u8g2.drawFrame(x, y, CELL_W, CELL_H);
                }

                const char* name = (index < numInstruments) ? instruments[index]->getName() : "---";
                int textW = u8g2.getStrWidth(name);
                u8g2.setCursor(x + (CELL_W - textW) / 2, y + (CELL_H + 7) / 2);
                u8g2.print(name);
                u8g2.setDrawColor(1);
            }
        }
    }
};
