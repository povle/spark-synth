#include "InstrumentAnalog.h"
#include "Controls.h"
#include <cmath>

InstrumentAnalog::InstrumentAnalog()
{
    _instrumentName = "Analog";
    _instrumentShortName = "ANLG";
}

void InstrumentAnalog::init()
{
    Serial.println("  [Analog] Initializing...");
    // initFromPots();
    Serial.println("  [Analog] Init complete\n");
}


void InstrumentAnalog::start()
{
    isActive = true;
    setupSynthVoices();
    Serial.println("  [Analog] Ready");
}

void InstrumentAnalog::stop()
{
    amy_event e = amy_default_event();
    e.synth = getSynthChannel();
    e.velocity = 0.0f;
    amy_add_event(&e);
    isActive = false;
}

void InstrumentAnalog::setupSynthVoices()
{
    amy_event e = amy_default_event();
    e.reset_osc = RESET_PATCH;
    e.patch_number = 1024;
    amy_add_event(&e);

    // Osc 1
    e = amy_default_event();
    e.osc = OSC_1;
    e.patch_number = 1024;
    e.wave = _osc1_wave;
    e.amp_coefs[COEF_CONST] = 0.5f;
    e.chained_osc = OSC_2;
    e.mod_source = OSC_LFO_FILTER;
    amy_add_event(&e);

    // Osc 2
    e = amy_default_event();
    e.osc = OSC_2;
    e.patch_number = 1024;
    e.wave = _osc2_wave;
    e.amp_coefs[COEF_CONST] = 0.5f;
    e.chained_osc = OSC_NOISE;
    e.mod_source = OSC_LFO_FILTER;
    amy_add_event(&e);

    // Noise
    e = amy_default_event();
    e.osc = OSC_NOISE;
    e.patch_number = 1024;
    e.wave = NOISE;
    e.amp_coefs[COEF_CONST] = params.noise * 0.5f;
    e.chained_osc = OSC_LFO_FILTER;
    amy_add_event(&e);

    // LFO Filter
    e = amy_default_event();
    e.osc = OSC_LFO_FILTER;
    e.patch_number = 1024;
    e.wave = SINE;
    e.freq_coefs[COEF_CONST] = params.lfo_freq * 10.0f;
    e.amp_coefs[COEF_CONST] = params.lfo_amp;
    e.freq_coefs[COEF_NOTE] = 0;
    e.amp_coefs[COEF_NOTE] = 0;
    // e.chained_osc = OSC_LFO_PITCH;
    amy_add_event(&e);

    e = amy_default_event();
    e.patch_number = 1024;
    e.filter_freq_coefs[COEF_MOD] = 1.0f;
    amy_add_event(&e);

    // // LFO Pitch
    // e = amy_default_event();
    // e.osc = OSC_LFO_PITCH;
    // e.patch_number = 1024;
    // e.wave = SINE;
    // e.freq_coefs[COEF_CONST] = params.lfo_freq * 10.0f + 0.5f;
    // e.amp_coefs[COEF_CONST] = _modWheel * params.lfo_amp;
    // amy_add_event(&e);

    // Synth config
    e = amy_default_event();
    e.synth = getSynthChannel();
    e.patch_number = 1024;
    e.num_voices = 8;
    amy_add_event(&e);

    applyDefaultEQ(4.0f);

    // Apply initial settings
    updateOscDetune();
    updateOscBalance();
    configNoise();
    configLfo();

    Serial.printf("  [Analog] Synth configured (synth=%d)\n", getSynthChannel());
}

void InstrumentAnalog::onCustomPot(uint8_t channel, float value)
{
    // Pot 0: Osc 1 wave (0-4)
    if (channel == 0)
    {
        _osc1_wave = (uint8_t)(value * 4.99f);
        if (_osc1_wave > 4)
            _osc1_wave = 4;
        updateOsc1Wave(_osc1_wave);
        return;
    }

    // Pot 1: Osc 2 wave
    if (channel == 1)
    {
        _osc2_wave = (uint8_t)(value * 4.99f);
        if (_osc2_wave > 4)
            _osc2_wave = 4;
        updateOsc2Wave(_osc2_wave);
        return;
    }

    // Pot 2: Detune
    if (channel == 2)
    {
        _osc2_detune = value;
        updateOscDetune();
        return;
    }

    // Pot 3: Balance
    if (channel == 3)
    {
        _osc_balance = value;
        updateOscBalance();
        return;
    }
}

void InstrumentAnalog::updateOsc1Wave(uint8_t wave)
{
    amy_event e = amy_default_event();
    e.synth = getSynthChannel();
    e.osc = OSC_1;
    e.wave = wave;
    amy_add_event(&e);
}

void InstrumentAnalog::updateOsc2Wave(uint8_t wave)
{
    amy_event e = amy_default_event();
    e.synth = getSynthChannel();
    e.osc = OSC_2;
    e.wave = wave;
    amy_add_event(&e);
}

void InstrumentAnalog::updateOscDetune()
{
    float detune;
    const float DETUNE_DEADZONE = 0.03;
    if (_osc2_detune > 0.5 + DETUNE_DEADZONE)
        detune = std::pow(2, (_osc2_detune-DETUNE_DEADZONE )*2.0f - 1.0f);
    else if (_osc2_detune < 0.5 - DETUNE_DEADZONE)
        detune = std::pow(2, (_osc2_detune + DETUNE_DEADZONE) * 2.0f - 1.0f);
    else
        detune = 0;

    amy_event e = amy_default_event();
    e.synth = getSynthChannel();
    e.osc = OSC_2;
    e.freq_coefs[COEF_CONST] = 440.0f * detune;
    amy_add_event(&e);
}

void InstrumentAnalog::updateOscBalance()
{
    float amp1, amp2;
    if (_osc_balance < 0.5f)
    {
        amp1 = 1.0f;
        amp2 = _osc_balance * 2.0f;
    }
    else
    {
        amp1 = (1.0f - _osc_balance) * 2.0f;
        amp2 = 1.0f;
    }

    amy_event e = amy_default_event();
    e.synth = getSynthChannel();
    e.osc = OSC_1;
    e.amp_coefs[COEF_CONST] = amp1 * 0.5f;
    amy_add_event(&e);

    e = amy_default_event();
    e.synth = getSynthChannel();
    e.osc = OSC_2;
    e.amp_coefs[COEF_CONST] = amp2 * 0.5f;
    amy_add_event(&e);
}

void InstrumentAnalog::configNoise()
{
    amy_event e = amy_default_event();
    e.synth = getSynthChannel();
    e.osc = OSC_NOISE;
    e.amp_coefs[COEF_CONST] = params.noise * 0.5f;
    amy_add_event(&e);
}

void InstrumentAnalog::configLfo()
{
    // LFO → Filter
    amy_event e = amy_default_event();
    e.synth = getSynthChannel();
    e.osc = OSC_LFO_FILTER;
    e.freq_coefs[COEF_CONST] = params.lfo_freq * 20.0f;
    e.amp_coefs[COEF_CONST] = params.lfo_amp * 2.0f + 0.001f;
    // e.amp_coefs[COEF_NOTE] = 0;
    amy_add_event(&e);

    // // LFO → Pitch (scaled by mod wheel)
    // e = amy_default_event();
    // e.synth = getSynthChannel();
    // e.osc = OSC_LFO_PITCH;
    // e.freq_coefs[COEF_CONST] = params.lfo_freq * 10.0f + 0.5f;
    // e.amp_coefs[COEF_CONST] = _modWheel * params.lfo_amp;
    // amy_add_event(&e);
}

void InstrumentAnalog::updateJoystickY(float y)
{
    y -= 0.5;
    y *= 2.0f;
    const float MOD_DEADZONE = 0.1f;
    _modWheel = (y > MOD_DEADZONE) ? (y - MOD_DEADZONE) / (1.0f - MOD_DEADZONE) : 0.0f;
    // configLfo();
}
