#include "InstrumentJuno.h"
#include "juno_patches.h"

void InstrumentJuno::init()
{
    Serial.println("  [Juno] Initialized (Patches 0-127)");
}

void InstrumentJuno::start()
{
    isActive = true;

    // Allocate voices dynamically when the instrument loads
    amy_event e = amy_default_event();
    e.synth = 1;
    e.num_voices = 5;
    e.patch_number = patch;
    amy_add_event(&e);

    Serial.printf("  [Juno] Ready (patch %d)\n", patch);
}

void InstrumentJuno::stop()
{
    amy_event e = amy_default_event();
    e.synth = 1;
    e.velocity = 0.0f;
    amy_add_event(&e);
    isActive = false;
}

void InstrumentJuno::onCustomPot(uint8_t channel, float value)
{
    params.custom[channel] = value;
    return;
}

void InstrumentJuno::drawUI(U8G2 &u8g2, uint8_t y_offset)
{
    u8g2.setFont(u8g2_font_tenfatguys_tf);
    // u8g2.setCursor(0, y_offset + 18);
    // u8g2.printf("INT %d", patch + 1);
    u8g2.setCursor(0, y_offset + 32);
    u8g2.print(juno_patch_names[patch]);
}

void InstrumentJuno::onPressedButton(uint8_t button_id)
{
    if (button_id == 2)
    {
        if (patch > 0)
            patch--;
    }
    else if (button_id == 3)
    {
        if (patch < 127)
            patch++;
    }
    else
    {
        return;
    }
    amy_event e = amy_default_event();
    e.synth = 1;
    e.patch_number = patch;
    amy_add_event(&e);
    Serial.printf("  [Juno] Patch %d\n", patch);
}
