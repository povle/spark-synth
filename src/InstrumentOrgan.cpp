#pragma once
#include "Instrument.h"
#include <math.h>

#define NUM_PARTIALS 5
#define MAX_VOICES 6

class OrganSynth : public Instrument
{
public:
    void init() override
    {
        Serial.println("  [Organ] Initialized");
        for (int i = 0; i < MAX_VOICES; i++)
            _voice_notes[i] = -1;
    }

    void start() override
    {
        Instrument::start(); // Sets isActive and triggers parameters
        Serial.println("  [Organ] Ready");
        _updatePartials(params.custom[0]); // Custom 0 is Spectral Shape 'B'
    }

    void noteOn(uint8_t note, float velocity) override
    {
        if (!isActive)
            return;
        int voice = _allocateVoice(note);
        if (voice < 0)
            return;

        amy_event e = amy_default_event();
        e.osc = voice * NUM_PARTIALS;
        e.midi_note = note;
        e.velocity = velocity;
        amy_add_event(&e);
    }

    void noteOff(uint8_t note) override
    {
        if (!isActive)
            return;
        for (int i = 0; i < MAX_VOICES; i++)
        {
            if (_voice_notes[i] == note)
            {
                amy_event e = amy_default_event();
                e.osc = i * NUM_PARTIALS;
                e.velocity = 0.0f;
                amy_add_event(&e);
                _voice_notes[i] = -1; // Free voice
                break;
            }
        }
    }

protected:
    void onCustomPot(uint8_t channel, float value) override
    {
        if (channel == 0)
            _updatePartials(value);
    }

    // Organ overrides ADSR to apply it to all 30 raw oscillators
    void sendAdsr() override
    {
        uint16_t a_ms = (uint16_t)fmax(params.attack * 1000.0f, 1.0f);
        uint16_t d_ms = (uint16_t)fmax(params.decay * 1000.0f, 1.0f);
        uint16_t r_ms = (uint16_t)fmax(params.release * 1000.0f, 1.0f);

        // Apply this envelope to every single partial across all voices
        for (int i = 0; i < (MAX_VOICES * NUM_PARTIALS); i++)
        {
            amy_event e = amy_default_event();
            e.osc = i;

            e.eg0_times[0] = a_ms;
            e.eg0_values[0] = 1.0f;

            e.eg0_times[1] = d_ms;
            e.eg0_values[1] = params.sustain;

            e.eg0_times[2] = r_ms;
            e.eg0_values[2] = 0.0f;

            amy_add_event(&e);
        }
    }

    void sendFilter() override
    {
        for (int i = 0; i < (MAX_VOICES * NUM_PARTIALS); i++)
        {
            amy_event e = amy_default_event();
            e.osc = i;
            e.filter_freq_coefs[0] = params.cutoff;
            e.resonance = params.resonance;
            amy_add_event(&e);
        }
    }

private:
    int _voice_notes[MAX_VOICES];

    int _allocateVoice(int note)
    {
        for (int i = 0; i < MAX_VOICES; i++)
        {
            if (_voice_notes[i] == -1)
            {
                _voice_notes[i] = note;
                return i;
            }
        }
        _voice_notes[0] = note; // Simple steal
        return 0;
    }

    float _getAmplitude(float b, int partial, int num_partials)
    {
        float sign_val = (1.0f - b >= 0.0f) ? 1.0f : -1.0f;
        float a = pow(10.0f - 10.0f * b, 2.0f) * sign_val;
        float x = (float)(partial - 1) / (float)(num_partials - 1);
        if (a >= 0.0f)
            return fmin(1.0f / pow(x + 1.0f, a), 1.0f);
        if (partial == 1)
            return 1.0f;
        return 0.0f;
    }

    void _updatePartials(float b)
    {
        float raw_amps[NUM_PARTIALS];
        float sum = 0.0f;

        for (int i = 1; i <= NUM_PARTIALS; i++)
        {
            raw_amps[i - 1] = _getAmplitude(b, i, NUM_PARTIALS);
            sum += raw_amps[i - 1];
        }
        if (sum == 0.0f)
            sum = 1.0f;

        for (int v = 0; v < MAX_VOICES; v++)
        {
            int base_osc = v * NUM_PARTIALS;

            for (int i = 1; i <= NUM_PARTIALS; i++)
            {
                float final_amp = fmax(raw_amps[i - 1] / sum, 0.0f);

                amy_event e = amy_default_event();
                e.osc = base_osc + (i - 1);
                e.wave = SINE;

                e.freq_coefs[0] = 261.63f * i;
                e.freq_coefs[1] = 1.0f;

                e.amp_coefs[0] = 0.0f;
                e.amp_coefs[1] = 0.0f;
                e.amp_coefs[2] = 0.0f;
                e.amp_coefs[3] = final_amp;

                e.filter_type = FILTER_NONE;

                if (i < NUM_PARTIALS)
                    e.chained_osc = base_osc + i;
                else
                    e.chained_osc = -1;

                amy_add_event(&e);
            }
        }
        sendAllParams(); // Ensure ADSR/Filter match current pot states
    }
};
