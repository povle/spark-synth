#include "InstrumentSine.h"

void InstrumentSine::init()
{
    Serial.println("  [Sine] Initialized");
}

void InstrumentSine::start()
{
    isActive = true;
    Serial.println("  [Sine] Ready");
}

void InstrumentSine::stop()
{
    amy_event e = amy_default_event();
    e.synth = 1;
    e.velocity = 0.0f;
    amy_add_event(&e);
    isActive = false;
}

void InstrumentSine::noteOn(uint8_t note, float velocity)
{
    if (!isActive)
        return;

    amy_event e = amy_default_event();
    e.synth = 1;
    e.midi_note = note;
    e.velocity = velocity;
    amy_add_event(&e);
}

void InstrumentSine::noteOff(uint8_t note)
{
    if (!isActive)
        return;

    amy_event e = amy_default_event();
    e.synth = 1;
    e.midi_note = note;
    e.velocity = 0.0f;
    amy_add_event(&e);
}

void InstrumentSine::updatePot(uint8_t channel, float value)
{
    if (channel < 4)
    {
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
