#include "InstrumentJuno.h"

void InstrumentJuno::init()
{
    Serial.println("  [Juno] Initialized (Patches 1-127)");
}

void InstrumentJuno::start()
{
    isActive = true;

    // Allocate voices dynamically when the instrument loads
    amy_event e = amy_default_event();
    e.synth = 1;
    e.num_voices = 6;
    e.patch_number = junoPatch;
    amy_add_event(&e);

    Serial.printf("  [Juno] Ready (patch %d)\n", junoPatch);
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

void InstrumentJuno::onPressedButton(uint8_t button_id)
{
    if (button_id == 2)
    {
        junoPatch--;
    }
    else if (button_id == 3)
    {
        junoPatch++;
    }
    else
    {
        return;
    }
    amy_event e = amy_default_event();
    e.synth = 1;
    e.patch_number = junoPatch + 1;
    amy_add_event(&e);
    Serial.printf("  [Juno] Patch %d\n", junoPatch);
}
