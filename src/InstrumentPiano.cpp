#include "InstrumentPiano.h"

void InstrumentPiano::init()
{
    Serial.println("  [Piano] Initialized");
}

void InstrumentPiano::start()
{
    isActive = true;

    // Allocate voices dynamically when the instrument loads
    amy_event e = amy_default_event();
    e.synth = 1;
    e.num_voices = 12;
    e.patch_number = 256;
    amy_add_event(&e);

    Serial.printf("  [Piano] Ready\n");
}

void InstrumentPiano::stop()
{
    amy_event e = amy_default_event();
    e.synth = 1;
    e.velocity = 0.0f;
    amy_add_event(&e);
    isActive = false;
}
