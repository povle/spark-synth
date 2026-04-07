#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
extern "C"
{
#include <amy.h>
}

// Struct to hold all synth parameters so they can be saved/loaded easily later
struct SynthParams
{
    float cutoff = 2000.0f;
    float resonance = 1.0f;
    float attack = 0.05f; // Seconds
    float decay = 0.2f;   // Seconds
    float sustain = 0.5f; // Level 0.0-1.0
    float release = 0.5f; // Seconds
    float reverb = 0.0f;
    float delay_amp = 0.0f;
    float delay_freq = 0.0f;
    float noise = 0.0f;
    float lfo_amp = 0.0f;
    float lfo_freq = 0.0f;
    float custom[4] = {0.5f, 0.5f, 0.5f, 0.5f};
};

class Instrument
{
public:
    SynthParams params;
    bool isActive = false;
    bool needsUIRedraw = false;
    bool liveUI = false; // same thing but as needsUIRedraw but doesn't get reset by DisplayManager. currently just for sampler to avoid resetting needsUIRedraw from a thread.

    virtual ~Instrument() {}

    virtual void init() {}

    virtual void start()
    {
        isActive = true;
        sendAllParams();
        applyDefaultEQ();
    }

    virtual void stop()
    {
        isActive = false;
    }

    virtual void update() {}

    virtual void drawUI(U8G2 &u8g2, uint8_t y_offset) {}

    // Default Note On/Off (Uses AMY's synth allocator)
    virtual void noteOn(uint8_t note, float velocity)
    {
        if (!isActive)
            return;
        amy_event e = amy_default_event();
        e.synth = getSynthChannel();
        e.midi_note = note;
        e.velocity = velocity;
        amy_add_event(&e);
    }

    virtual void noteOff(uint8_t note)
    {
        if (!isActive)
            return;
        amy_event e = amy_default_event();
        e.synth = getSynthChannel();
        e.midi_note = note;
        e.velocity = 0.0f;
        amy_add_event(&e);
    }

    virtual void onPressedButton(uint8_t button_id) {}

    virtual void onReleasedButton(uint8_t button_id) {}

    virtual void updateJoystick(float x, float y) {
        if (!isActive)
            return;
        float bendRatio = powf(2.0f, (x * 2.0f) / 12.0f);

        amy_event e = amy_default_event();
        e.synth = getSynthChannel();
        e.pitch_bend = bendRatio;
        amy_add_event(&e);

        updateJoystickY(y);
    }

    virtual void updateJoystickY(float y) {};

    const char *getName()
    {
        return _instrumentName;
    }

    const char *getShortName()
    {
        return _instrumentShortName;
    }

    void applyDefaultEQ(float volume)
    {
        amy_event e = amy_default_event();
        e.synth = 1;
        e.volume = volume;
        e.eq_l = 4.0f;  // Boost lows slightly
        e.eq_m = -3.0f; // Cut the harsh mid-range "honk"
        e.eq_h = -6.0f; // Severely cut the high-frequency fizz
        // TODO: we should detect if the 3.5 cable is connected and disable the EQ when it is.
        amy_add_event(&e);
    }

    void applyDefaultEQ()
    {
        amy_event e = amy_default_event();
        e.synth = 1;
        e.volume = 4.0f;
        e.eq_l = 4.0f;  // Boost lows slightly
        e.eq_m = -3.0f; // Cut the harsh mid-range "honk"
        e.eq_h = -6.0f; // Severely cut the high-frequency fizz
        // TODO: we should detect if the 3.5 cable is connected and disable the EQ when it is.
        amy_add_event(&e);
    }

    // Standardized Potentiometer Router
    virtual void updatePot(uint8_t channel, float value)
    {
        Serial.printf("Knob %d moved to %f\n", channel, value);
        bool updateAdsr = false;
        bool updateFilter = false;
        bool updateLfo = false;
        bool updateNoise = false;

        switch (channel)
        {
        // Custom Knobs (0-3)
        case 0:
        case 1:
        case 2:
        case 3:
            params.custom[channel] = value;
            onCustomPot(channel, value);
            break;

        // Filter (4-5)
        case 4:
            params.cutoff = 50.0f + (value * 8000.0f);
            updateFilter = true;
            break;
        case 5:
            params.resonance = 0.5f + (value * 4.5f);
            updateFilter = true;
            break;

        // LFO (6-7)
        case 6:
            params.lfo_freq = value;
            updateLfo = true;
            break;
        case 7:
            params.lfo_amp = value;
            updateLfo = true;
            break;

        // ADSR (8-11)
        case 8:
            params.attack = value;
            updateAdsr = true;
            break;
        case 9:
            params.decay = value;
            updateAdsr = true;
            break;
        case 10:
            params.sustain = value;
            updateAdsr = true;
            break;
        case 11:
            params.release = value;
            updateAdsr = true;
            break;

        // FX (12-13) - Sent directly to global FX engine
        case 12:
            if (value < 0.05) {
                value = 0;
            };
             params.reverb = value;

            config_reverb(value * 2.0f, 0.85f, 0.5f, 3000.0f);
            break;
        case 13:
            if (value < 0.05) {
                value = 0;
            };

            params.delay_freq = value * 2000.0f;
            config_echo(params.delay_amp, params.delay_freq, 3000.0f, params.delay_amp * 0.8f, 0.0f);
            break;
        case 14:
            if (value < 0.05) {
                value = 0;
            };
            params.delay_amp = value;
            config_echo(params.delay_amp, params.delay_freq, 3000.0f, params.delay_amp * 0.8f, 0.0f);
            break;
        case 15:
            params.noise = value;
            updateNoise = true;
            break;
        }

        // Only send updates to AMY if the instrument is currently active on screen
        if (isActive)
        {
            if (updateAdsr)
                sendAdsr();
            if (updateFilter)
                sendFilter();
            if (updateNoise)
                configNoise();
            if (updateLfo)
                configLfo();
        }
    }

protected:
    const char *_instrumentName = "Base";
    const char *_instrumentShortName = "Base";

    virtual uint8_t getSynthChannel() { return 1; }
    virtual void onCustomPot(uint8_t channel, float value) {}

    virtual void configNoise()
    {
        // if noise is not used might as well use it for volume
        amy_event e = amy_default_event();
        e.synth = getSynthChannel();
        e.volume = params.noise * 7.0f;
        amy_add_event(&e);
    }
    virtual void configLfo() {}

    virtual void sendAdsr()
    {
        amy_event e = amy_default_event();
        e.synth = getSynthChannel();

        // Calculate times in ms using the Juno math curves, enforcing a 1ms minimum
        uint16_t a_ms = (uint16_t)fmax(6.0f + 8.0f * (params.attack * 127.0f), 1.0f);
        uint16_t d_ms = (uint16_t)fmax(80.0f * powf(2.0f, 0.085f * (params.decay * 127.0f)) - 80.0f, 1.0f);
        uint16_t r_ms = (uint16_t)fmax(70.0f * powf(2.0f, 0.066f * (params.release * 127.0f)) - 70.0f, 1.0f);

        // Pair 0: Attack
        e.eg0_times[0] = a_ms;
        e.eg0_values[0] = 1.0f;

        // Pair 1: Decay to Sustain
        e.eg0_times[1] = d_ms;
        e.eg0_values[1] = params.sustain;

        // Pair 2: Release to 0.0
        e.eg0_times[2] = r_ms;
        e.eg0_values[2] = 0.0f;

        amy_add_event(&e);
    }

    virtual void sendFilter()
    {
        amy_event e = amy_default_event();
        e.synth = getSynthChannel();
        e.filter_freq_coefs[0] = params.cutoff;
        e.resonance = params.resonance;
        e.filter_type = 1; // Filter type: 0 = none (default.) 1 = lowpass, 2 = bandpass, 3 = highpass, 4 = double-order lowpass.
        amy_add_event(&e);
    }

    void sendAllParams()
    {
        sendAdsr();
        sendFilter();
    }

    void drawWrappedPatchName(U8G2 &u8g2, uint8_t x, uint8_t y, const char *flashPtr)
    {
        char buffer[48];
        // Copy from PROGMEM to local RAM buffer
        strncpy_P(buffer, flashPtr, 47);
        buffer[47] = '\0';

        // Find the delimiter
        char *separator = strchr(buffer, '\n');

        if (separator != NULL)
        {
            // We found a split!
            *separator = '\0';           // Terminate line 1 at the pipe
            char *line2 = separator + 1; // Line 2 starts after the pipe

            u8g2.drawStr(x, y, buffer);     // Draw first line
            u8g2.drawStr(x, y + 18, line2);
        }
        else
        {
            // No split needed
            u8g2.drawStr(x, y, buffer);
        }
    }
};
