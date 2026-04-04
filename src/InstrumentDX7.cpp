#include "InstrumentDX7.h"
#include "dx7_patches.h"
#include "dx7_algos.h"
#include <U8g2lib.h>
#include <vector>
#include <utility>
#include <algorithm>
#include <cmath>

void textCenteredAtX(U8G2 &u8g2, int centerX, int y, const char *text)
{
    int textWidth = u8g2.getStrWidth(text);
    int startX = centerX - (textWidth / 2);
    u8g2.drawStr(startX, y, text);
};

void draw_algo(U8G2 &u8g2,
               const std::vector<std::pair<int, int>> &top_connections,
               const std::vector<std::pair<int, int>> &bottom_connections,
               const std::vector<int> &carriers,
               int bottom_offset)
{
    const int base_y = 63;
    char buffer[2];

    // 1. Draw Operators
    for (int op = 0; op < 6; op++)
    {
        int x_0 = 2 + op * 21;
        int w = 16;
        int h = 16;
        int y_0 = base_y - bottom_offset - h;

        u8g2.setDrawColor(1);

        itoa(op+1, buffer, 10);

        textCenteredAtX(u8g2, x_0+8, y_0 + 13, buffer);

        // Check if 'op' is in the carriers vector
        bool is_carrier = (std::find(carriers.begin(), carriers.end(), op) != carriers.end());

        if (is_carrier)
        {
            u8g2.setDrawColor(2); // XOR mode
            u8g2.drawBox(x_0, y_0, w, h);
        }
        else
        {
            u8g2.drawFrame(x_0, y_0, w, h);
        }
    }

    u8g2.setDrawColor(1); // Restore normal color

    // 2. Draw Top Connections
    for (const auto &conn : top_connections)
    {
        int a = conn.first;
        int b = conn.second;

        int x_a = 2 + a * 21;
        int y_hline = base_y - bottom_offset - 16 - 4;

        u8g2.drawVLine(x_a + 8, y_hline, 3);

        if (a != b)
        {
            int x_b = 2 + b * 21;
            u8g2.drawVLine(x_b + 8, y_hline, 3);

            int start_x = std::min(x_a, x_b);
            int width = std::abs(x_b - x_a) + 1;
            u8g2.drawHLine(start_x + 8, y_hline, width);
        }
        else
        {
            // Feedback loop
            u8g2.drawHLine(x_a + 17, base_y - bottom_offset - 8, 2);
            u8g2.drawVLine(x_a + 18, y_hline, 12);
            u8g2.drawHLine(x_a + 8, y_hline, 11);
        }
    }

    // 3. Draw Bottom Connections
    for (const auto &conn : bottom_connections)
    {
        int a = conn.first;
        int b = conn.second;

        int x_a = 2 + a * 21;
        int y_hline = base_y - bottom_offset + 3;

        u8g2.drawVLine(x_a + 8, y_hline - 2, 3);

        if (a != b)
        {
            int x_b = 2 + b * 21;
            u8g2.drawVLine(x_b + 8, y_hline - 2, 3);

            int start_x = std::min(x_a, x_b);
            int width = std::abs(x_b - x_a) + 1;
            u8g2.drawHLine(start_x + 8, y_hline, width);
        }
        else
        {
            // Feedback loop
            u8g2.drawHLine(x_a + 17, base_y - bottom_offset - 8, 2);
            u8g2.drawVLine(x_a + 18, y_hline - 12, 12);
            u8g2.drawHLine(x_a + 8, y_hline, 11);
        }
    }
}

void InstrumentDX7::init()
{
    Serial.println("  [DX-7] Initialized (Patches 128-255)");
}

void InstrumentDX7::start()
{
    isActive = true;

    // Allocate voices dynamically when the instrument loads
    amy_event e = amy_default_event();
    e.synth = 1;
    e.num_voices = 8;
    e.volume = 4.0f;
    e.patch_number = patch + 128;
    amy_add_event(&e);
    needsUIRedraw = true;

    Serial.printf("  [DX-7] Ready (patch %d)\n", patch);
}

void InstrumentDX7::stop()
{
    amy_event e = amy_default_event();
    e.synth = 1;
    e.velocity = 0.0f;
    amy_add_event(&e);
    isActive = false;
}

void InstrumentDX7::onCustomPot(uint8_t channel, float value)
{
    params.custom[channel] = value;
    return;
}

void InstrumentDX7::drawUI(U8G2 &u8g2, uint8_t y_offset)
{
    u8g2.setFont(u8g2_font_spleen12x24_mu);
    u8g2.setCursor(0, y_offset + 18);
    u8g2.printf("%s", dx7_patches[patch].name);

    uint8_t algoNum = pgm_read_byte(&dx7_patches[patch].algo);
    const DX7Algo &currentAlgo = dx7_algorithms[algoNum-1];

    u8g2.setFont(u8g2_font_tenfatguys_tf);
    draw_algo(u8g2, currentAlgo.top, currentAlgo.bottom, currentAlgo.carriers, 9);
}

void InstrumentDX7::onPressedButton(uint8_t button_id)
{
    if (button_id == 2) {
        if (patch > 0)
            patch--;
    }
    else if (button_id == 3) {
        if (patch < 127)
            patch++;
    }
    else {
        return;
    }
    amy_event e = amy_default_event();
    e.synth = 1;
    e.patch_number = patch + 128;
    amy_add_event(&e);
    Serial.printf("  [DX-7] Patch %d\n", patch);
    needsUIRedraw = true;
}

void InstrumentDX7::sendAdsr()
{
    amy_event e = amy_default_event();
    e.synth = getSynthChannel();

    // Convert seconds to milliseconds (ensure minimum of 1ms to prevent audio glitches)
    uint16_t a_ms = (uint16_t)fmax(params.attack * 1000.0f, 1.0f);
    uint16_t d_ms = (uint16_t)fmax(params.decay * 1000.0f, 1.0f);
    uint16_t r_ms = (uint16_t)fmax(params.release * 1000.0f, 1.0f);
    Serial.printf("Updating ADSR: %d %d %f %d\n", a_ms, d_ms, params.sustain, r_ms);

    // Pair 0: Delta time to reach Attack Peak (1.0)
    e.eg1_times[0] = a_ms;
    e.eg1_values[0] = 1.0f;

    // Pair 1: Delta time to drop to Sustain Level
    e.eg1_times[1] = d_ms;
    e.eg1_values[1] = params.sustain;

    // Pair 2: Time to drop to 0.0 (AMY automatically waits for Note Off for the last pair)
    e.eg1_times[2] = r_ms;
    e.eg1_values[2] = 0.0f;

    // e.amp_coefs[0] = 0.0f; // Default is 0,0,1,1 (i.e. the amplitude comes from the note velocity multiplied by by Envelope Generator 0.)
    // e.amp_coefs[0] = 0.0f;
    // e.amp_coefs[1] = 1.0f;
    // e.amp_coefs[1] = 1.0f;

    // e.freq_coefs[0] = 0.0f; // Default is 0,1,0,0,0,0,1 (from note pitch plus pitch_bend)
    // e.freq_coefs[1] = 1.0f;
    // e.freq_coefs[2] = 0.0f;
    // e.freq_coefs[3] = 0.0f;
    // e.freq_coefs[4] = 0.0f;
    // e.freq_coefs[5] = 1.0f;

    amy_add_event(&e);
}
