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

void ControlsClass::readJoystick(float &x, float &y)
{
    // --- CALIBRATION CONSTANTS ---
    const int X_MIN = 167;
    const int X_MAX = 4011;
    const int X_CENTER = 2089; // Adjust if your physical center rests slightly differently

    const int Y_MIN = 147;
    const int Y_MAX = 4095;

    const int DEADZONE = 150;

    // 1. Read Raw Hardware
    int rawX = analogRead(JOY_PIN_X);
    int rawY = analogRead(JOY_PIN_Y);

    // 2. Calculate X (-1.0 to 1.0) with Deadzone & Asymmetric Center
    float normX = 0.0f;
    if (rawX > (X_CENTER + DEADZONE))
    {
        // Map right side
        normX = (float)(rawX - (X_CENTER + DEADZONE)) / (float)(X_MAX - (X_CENTER + DEADZONE));
    }
    else if (rawX < (X_CENTER - DEADZONE))
    {
        // Map left side (yields a negative number down to -1.0)
        normX = (float)(rawX - (X_CENTER - DEADZONE)) / (float)((X_CENTER - DEADZONE) - X_MIN);
    }

    // Constrain X to prevent math errors if pushed harder than calibration
    if (normX > 1.0f)
        normX = 1.0f;
    if (normX < -1.0f)
        normX = -1.0f;

    // 3. Calculate Y (0.0 to 1.0) and Reverse it
    float normY = (float)(rawY - Y_MIN) / (float)(Y_MAX - Y_MIN);
    normY = 1.0f - normY; // Reverses the axis

    // Constrain Y
    if (normY > 1.0f)
        normY = 1.0f;
    if (normY < 0.0f)
        normY = 0.0f;

    // 4. Output the cleaned values
    x = normX;
    y = normY;
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
