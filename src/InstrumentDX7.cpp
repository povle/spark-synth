#include "InstrumentDX7.h"

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
    e.num_voices = 12;
    e.patch_number = dxPatch;
    amy_add_event(&e);

    Serial.printf("  [DX-7] Ready (patch %d)\n", dxPatch);
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
    if (channel == 0)
    {
        // LATCH FIX: Only send to AMY if the patch number actually changed
        uint8_t newPatch = 128 + (uint8_t)(value * 127);
        if (newPatch != dxPatch)
        {
            dxPatch = newPatch;
            amy_event e = amy_default_event();
            e.synth = 1;
            e.patch_number = dxPatch;
            amy_add_event(&e);
            Serial.printf("  [DX-7] Patch %d\n", dxPatch);
        }
    }
    params.custom[channel] = value;
    return;
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
