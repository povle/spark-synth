#pragma once
#include <Arduino.h>
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
    float custom[4] = {0.5f, 0.5f, 0.5f, 0.5f};
};

class Instrument
{
public:
    SynthParams params;
    bool isActive = false;

    virtual ~Instrument() {}

    virtual void init() {}

    virtual void start()
    {
        isActive = true;
        sendAllParams();
    }

    virtual void stop()
    {
        isActive = false;
    }

    virtual void update() {}

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

    // Standardized Potentiometer Router
    virtual void updatePot(uint8_t channel, float value)
    {
        Serial.printf("Knob %d moved to %f\n", channel, value);
        bool updateAdsr = false;
        bool updateFilter = false;

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

        // ADSR (8-11)
        case 8:
            params.attack = value * 2.0f;
            updateAdsr = true;
            break;
        case 9:
            params.decay = value * 2.0f;
            updateAdsr = true;
            break;
        case 10:
            params.sustain = value;
            updateAdsr = true;
            break;
        case 11:
            params.release = value * 5.0f;
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
            config_echo(params.delay_amp, params.delay_freq, params.delay_amp * 7.0f * params.delay_freq, params.delay_amp * 0.8f, 0.0f);
            break;
        case 14:
            if (value < 0.05) {
                value = 0;
            };
            params.delay_amp = value;
            config_echo(params.delay_amp, params.delay_freq, params.delay_amp * 7.0f * params.delay_freq, params.delay_amp * 0.8f, 0.0f);
            break;
        }

        // Only send updates to AMY if the instrument is currently active on screen
        if (isActive)
        {
            if (updateAdsr)
                sendAdsr();
            if (updateFilter)
                sendFilter();
        }
    }

protected:
    virtual uint8_t getSynthChannel() { return 1; }
    virtual void onCustomPot(uint8_t channel, float value) {}

    // THE PROPER ADSR ARRAY LOGIC
    // Virtual so subclasses (like Organ) can override if they don't use e.synth
    virtual void sendAdsr()
    {
        amy_event e = amy_default_event();
        e.synth = getSynthChannel();

        // Convert seconds to milliseconds (ensure minimum of 1ms to prevent audio glitches)
        uint16_t a_ms = (uint16_t)fmax(params.attack * 1000.0f, 1.0f);
        uint16_t d_ms = (uint16_t)fmax(params.decay * 1000.0f, 1.0f);
        uint16_t r_ms = (uint16_t)fmax(params.release * 1000.0f, 1.0f);
        Serial.printf("Updating ADSR: %d %d %f %d\n", a_ms, d_ms, params.sustain, r_ms);

        // Pair 0: Delta time to reach Attack Peak (1.0)
        e.eg0_times[0] = a_ms;
        e.eg0_values[0] = 1.0f;

        // Pair 1: Delta time to drop to Sustain Level
        e.eg0_times[1] = d_ms;
        e.eg0_values[1] = params.sustain;

        // Pair 2: Time to drop to 0.0 (AMY automatically waits for Note Off for the last pair)
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
};
