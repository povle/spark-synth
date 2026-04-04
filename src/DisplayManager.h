#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "Instrument.h"

class DisplayManager
{
public:
    // Assuming a 128x64 I2C OLED on Wire
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

    DisplayManager() : u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL_PIN, OLED_SDA_PIN) {}
    void begin()
    {
        // Wire1.begin(, );
        // Wire1.setClock(400000);
        u8g2.begin();
        Serial.println("Display OK");
    }

    void update(Instrument *activeInst, Instrument *instruments[],
                                uint8_t numInstruments, uint8_t currentInstrument,
                                bool menuActive, uint8_t menuCursorRow, uint8_t menuCursorCol,
                                int octave, float batteryV)
    {

        u8g2.clearBuffer();

        // --- 1. DRAW UNIVERSAL STATUS BAR ---
        u8g2.setFont(u8g2_font_5x7_tr);
        uint8_t headerHeight = 10;

        u8g2.setCursor(2, 7);
        u8g2.printf("OCT:%+d", octave);

        int screenWidth = u8g2.getDisplayWidth();
        const char *instName = activeInst ? activeInst->getName() : "MENU";
        int textWidth = u8g2.getStrWidth(instName);
        int startX = (screenWidth - textWidth) / 2;

        u8g2.setFont(u8g2_font_resoledbold_tr);
        u8g2.setCursor(startX, 7);
        u8g2.print(instName);

        u8g2.setFont(u8g2_font_5x7_tr);
        u8g2.setCursor(100, 7);
        u8g2.printf("%.2fV", batteryV);

        u8g2.drawHLine(0, headerHeight, 128);

        // --- 2. DRAW CONTENT: Menu or Instrument UI ---
        if (menuActive)
        {
            // Draw instrument selection grid
            drawInstrumentMenu(instruments, numInstruments, currentInstrument,
                               menuCursorRow, menuCursorCol);
        }
        else if (activeInst)
        {
            // Delegate to instrument's custom UI
            activeInst->drawUI(u8g2, headerHeight + 2);
        }

        u8g2.sendBuffer();
    }

    // Add this method to DisplayManager class:
    void drawInstrumentMenu(Instrument *instruments[], uint8_t numInstruments,
                            uint8_t currentInstrument, uint8_t cursorRow, uint8_t cursorCol)
    {

        // Grid layout: 3 cols × 2 rows, each cell 42px wide × 23px tall
        const uint8_t CELL_W = 42; // 128 / 3 = 42.67, use 42
        const uint8_t CELL_H = 23;
        const uint8_t MENU_Y = 12; // Start below header

        u8g2.setFont(u8g2_font_6x10_tr); // Fits nicely in 23px tall cell

        for (uint8_t row = 0; row < 2; row++)
        {
            for (uint8_t col = 0; col < 3; col++)
            {
                uint8_t index = row * 3 + col;
                uint8_t x = col * CELL_W;
                uint8_t y = MENU_Y + row * CELL_H;

                // ONLY highlight cursor position (not active instrument)
                bool isCursor = (row == cursorRow && col == cursorCol);

                if (isCursor)
                {
                    // Cursor: white fill, black text
                    u8g2.setDrawColor(1);
                    u8g2.drawBox(x + 1, y + 1, CELL_W - 2, CELL_H - 2);
                    u8g2.setDrawColor(0); // Black text
                }
                else
                {
                    // Normal cell: just border
                    u8g2.setDrawColor(1);
                    u8g2.drawFrame(x, y, CELL_W, CELL_H);
                    // Text stays black (default draw color 1)
                }

                // Get instrument name
                const char *name = (index < numInstruments) ? instruments[index]->getName() : "---";

                // Center text
                int textW = u8g2.getStrWidth(name);
                int textX = x + (CELL_W - textW) / 2;
                int textY = y + (CELL_H + 7) / 2;

                u8g2.setCursor(textX, textY);
                u8g2.print(name);

                // Restore draw color
                u8g2.setDrawColor(1);
            }
        }
    }
};
