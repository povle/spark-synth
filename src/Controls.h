#ifndef CONTROLS_H
#define CONTROLS_H

#include <Arduino.h>
#include "Hardware.h"

#define NUM_POTS 16
#define POT_THRESHOLD 0.02f

class ControlsClass
{
public:
    void begin();
    bool readPot(uint8_t channel, float &value);
    void applyToHardware(uint8_t channel, float value);

private:
    float smoothedValues[NUM_POTS];
    float lastSentValues[NUM_POTS];
    uint8_t muxPins[4] = {MUX_S0, MUX_S1, MUX_S2, MUX_S3};
};

extern ControlsClass Controls;

#endif
