#ifndef CONTROLS_H
#define CONTROLS_H

#include <Arduino.h>
#include "Hardware.h"

#define NUM_POTS 16

// --- Dynamic Threshold Constants ---
#define POT_LOCKED_THRESHOLD 0.02f  // ~2% turn required to break the lock
#define POT_ACTIVE_THRESHOLD 0.005f // Fine resolution once awake
#define POT_LOCK_TIMEOUT_MS 10000    // Milliseconds of inactivity before locking again

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

    // Call this when loading a patch or switching instruments
    void lockAllPots();

private:
    float smoothedValues[NUM_POTS];
    float lastSentValues[NUM_POTS];
    unsigned long lastMoveTime[NUM_POTS]; // Tracks when the pot was last turned

    uint8_t muxPins[4] = {MUX_S0, MUX_S1, MUX_S2, MUX_S3};
};

extern ControlsClass Controls;

#endif
