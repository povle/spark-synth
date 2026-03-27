#include "Controls.h"

ControlsClass Controls;

void ControlsClass::begin()
{
    Serial.println("-> Initializing Controls...");

    for (uint8_t i = 0; i < NUM_POTS; i++)
    {
        // Select the mux channel
        for (uint8_t pin = 0; pin < 4; pin++)
        {
            digitalWrite(muxPins[pin], (i >> pin) & 0x01);
        }
        delayMicroseconds(15);

        // Pre-load the filter with the initial raw value
        float initialVal = analogRead(MUX_SIG) / 4095.0f;
        smoothedValues[i] = initialVal;
        lastSentValues[i] = -1.0f; // Force first read to trigger
    }
    Serial.println("   Controls Ready\n");
}

bool ControlsClass::readPot(uint8_t channel, float &value)
{
    if (channel >= NUM_POTS)
        return false;

    // 1. Select Mux Channel
    for (uint8_t i = 0; i < 4; i++)
    {
        digitalWrite(muxPins[i], (channel >> i) & 0x01);
    }
    delayMicroseconds(15); // Allow analog signal to settle

    // 2. Read Raw ADC and normalize
    int rawVal = analogRead(MUX_SIG);
    float normVal = rawVal / 4095.0f;

    // 3. Apply Exponential Moving Average (EMA)
    // The multiplier (0.15f) is the smoothing factor.
    // Lower (e.g., 0.05f) = smoother but more lag.
    // Higher (e.g., 0.50f) = faster but more jitter.
    smoothedValues[channel] = (0.16f * normVal) + (0.84f * smoothedValues[channel]);

    // 4. Hysteresis (Deadband threshold)
    // 0.002f gives you ~500 smooth steps of resolution.
    // If it still jitters when resting, increase this to 0.004f.
    if (abs(smoothedValues[channel] - lastSentValues[channel]) > 0.005f)
    {
        lastSentValues[channel] = smoothedValues[channel];
        value = lastSentValues[channel];
        return true;
    }

    return false;
}
