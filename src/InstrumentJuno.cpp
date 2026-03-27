#include "InstrumentJuno.h"

void InstrumentJuno::init()
{
    Serial.println("  [Juno] Initialized (Patches 1-127)");
}

void InstrumentJuno::start()
{
    isActive = true;
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

void InstrumentJuno::noteOn(uint8_t note, float velocity)
{
    if (!isActive)
        return;

    amy_event e = amy_default_event();
    e.synth = 1;
    e.midi_note = note;
    e.velocity = velocity;
    amy_add_event(&e);
}

void InstrumentJuno::noteOff(uint8_t note)
{
    if (!isActive)
        return;

    amy_event e = amy_default_event();
    e.synth = 1;
    e.midi_note = note;
    e.velocity = 0.0f;
    amy_add_event(&e);
}

void InstrumentJuno::updatePot(uint8_t channel, float value)
{
    if (channel < 4)
    {
        if (channel == 0)
        {
            // LATCH FIX: Only send to AMY if the patch number actually changed
            uint8_t newPatch = 0 + (uint8_t)(value * 127);
            if (newPatch != junoPatch)
            {
                junoPatch = newPatch;
                amy_event e = amy_default_event();
                e.synth = 1;
                e.patch_number = junoPatch;
                amy_add_event(&e);
                Serial.printf("  [Juno] Patch %d\n", junoPatch);
            }
        }
        params.custom[channel] = value;
        return;
    }

    switch (channel)
    {
    case 4:
        params.cutoff = 50.0f + (value * 8000.0f);
        break;
    case 5:
        params.resonance = 0.5f + (value * 4.5f);
        break;
    case 8:
        params.attack = value * 2.0f;
        break;
    case 9:
        params.decay = value * 2.0f;
        break;
    case 10:
        params.sustain = value;
        break;
    case 11:
        params.release = value * 5.0f;
        break;
    }
}
