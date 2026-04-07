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
        lastSentValues[i] = initialVal; // Sync to physical position
        lastMoveTime[i] = 0;            // Start locked
    }
    Serial.println("   Controls Ready\n");
}

void ControlsClass::lockAllPots()
{
    for (uint8_t i = 0; i < NUM_POTS; i++)
    {
        lastMoveTime[i] = 0; // Force timeout

        // Sync the baseline to the current physical position so the user
        // has to move it 5% away from where it currently sits to wake it up.
        lastSentValues[i] = smoothedValues[i];
    }
}

void ControlsClass::readJoystick(float &x, float &y)
{
    // --- CALIBRATION CONSTANTS ---
    const int X_MIN = 167;
    const int X_MAX = 4011;
    const int X_CENTER = 2089;

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
        normX = (float)(rawX - (X_CENTER + DEADZONE)) / (float)(X_MAX - (X_CENTER + DEADZONE));
    }
    else if (rawX < (X_CENTER - DEADZONE))
    {
        normX = (float)(rawX - (X_CENTER - DEADZONE)) / (float)((X_CENTER - DEADZONE) - X_MIN);
    }

    if (normX > 1.0f)
        normX = 1.0f;
    if (normX < -1.0f)
        normX = -1.0f;

    // 3. Calculate Y (0.0 to 1.0) and Reverse it
    float normY = (float)(rawY - Y_MIN) / (float)(Y_MAX - Y_MIN);
    normY = 1.0f - normY;

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
    delayMicroseconds(15);

    // 2. Read Raw ADC and normalize
    int rawVal = analogRead(MUX_SIG);
    float normVal = rawVal / 4095.0f;

    // 3. Apply Exponential Moving Average (EMA)
    smoothedValues[channel] = (0.16f * normVal) + (0.84f * smoothedValues[channel]);

    // 4. Dynamic Threshold Logic
    float currentThreshold = POT_LOCKED_THRESHOLD;

    // If the pot was moved recently, use the fine active threshold
    if (millis() - lastMoveTime[channel] < POT_LOCK_TIMEOUT_MS)
    {
        currentThreshold = POT_ACTIVE_THRESHOLD;
    }

    // 5. Check if movement exceeds the active threshold
    if (abs(smoothedValues[channel] - lastSentValues[channel]) > currentThreshold)
    {
        lastSentValues[channel] = smoothedValues[channel];
        lastMoveTime[channel] = millis(); // Reset the lock timer

        value = lastSentValues[channel];
        return true;
    }

    return false;
}

float ControlsClass::getPotValue(uint8_t channel)
{
    if (channel >= NUM_POTS)
        return 0.5f;

    // Return the smoothed value directly (no threshold check)
    // This is safe for init/patch loading where we want current position
    return smoothedValues[channel];
}
