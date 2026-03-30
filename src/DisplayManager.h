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

    // Takes system states and a pointer to the active instrument
    void update(Instrument *activeInst, int octave, float cpuTemp, int batteryPct)
    {
        u8g2.clearBuffer();

        // --- 1. DRAW UNIVERSAL STATUS BAR ---

        // Use a tiny 5x7 font for the top bar
        u8g2.setFont(u8g2_font_5x7_tr);
        uint8_t headerHeight = 10; // Reserve top 10 pixels

        // Left: Octave
        u8g2.setCursor(2, 7);
        u8g2.printf("%.1fC", cpuTemp);


        // Center: CPU Temp
        // Centers text roughly. (128/2) - (width/2)
        u8g2.setCursor(50, 7);
        u8g2.printf("OCT:%+d", octave);

        u8g2.setCursor(105, 7);
        if (batteryPct >= 0)
        {
            u8g2.printf("%d%%", batteryPct);
        }
        else
        {
            u8g2.print("USB");
        }

        // Draw separator line
        u8g2.drawHLine(0, headerHeight, 128);

        // --- 2. DELEGATE TO INSTRUMENT ---
        if (activeInst)
        {
            // Pass the u8g2 object and tell it to start drawing at Y=12
            activeInst->drawUI(u8g2, headerHeight + 2);
        }

        u8g2.sendBuffer();
    }
};
