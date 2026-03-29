#ifndef CONTROLS_H
#define CONTROLS_H

#include <Arduino.h>
#include "Hardware.h"

#define NUM_POTS 16
#define POT_THRESHOLD 0.02f
#define JOY_PIN_X 2
#define JOY_PIN_Y 1
#define JOY_CENTER 2048
#define JOY_DEADZONE 150

class ControlsClass
{
public:
    void begin();
    bool readPot(uint8_t channel, float &value);
    void applyToHardware(uint8_t channel, float value);
    void readJoystick(float &x, float &y);

private:
    float smoothedValues[NUM_POTS];
    float lastSentValues[NUM_POTS];
    uint8_t muxPins[4] = {MUX_S0, MUX_S1, MUX_S2, MUX_S3};
};

extern ControlsClass Controls;

#endif
