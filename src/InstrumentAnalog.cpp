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
    Serial.println("  [Analog] Init complete\n");
}


void InstrumentAnalog::start()
{
    isActive = true;
    initFromPots();
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

void InstrumentAnalog::initFromPots()
{
    float v;

    // Pot 0: Osc 1 waveform (0-4)
    v = Controls.getPotValue(0);
    _osc1_wave = (uint8_t)(v * 4.99f);
    if (_osc1_wave > 4)
        _osc1_wave = 4;

    // Pot 1: Osc 2 waveform
    v = Controls.getPotValue(1);
    _osc2_wave = (uint8_t)(v * 4.99f);
    if (_osc2_wave > 4)
        _osc2_wave = 4;

    // Pot 2: Detune (0.0-1.0, center deadzone)
    _osc2_detune = Controls.getPotValue(2);

    // Pot 3: Balance
    _osc_balance = Controls.getPotValue(3);

    // Filter/ADSR via params
    params.cutoff = 50.0f + Controls.getPotValue(4) * 8000.0f;
    params.resonance = 0.5f + Controls.getPotValue(5) * 4.5f;
    params.attack = Controls.getPotValue(8) * 2.0f;
    params.decay = Controls.getPotValue(9) * 2.0f;
    params.sustain = Controls.getPotValue(10);
    params.release = Controls.getPotValue(11) * 5.0f;

    // Noise/LFO params
    params.noise = Controls.getPotValue(6);
    params.lfo_freq = Controls.getPotValue(14);
    params.lfo_amp = Controls.getPotValue(15);

    Serial.println("  [Analog] Initialized from pot positions");
}

void InstrumentAnalog::updateOscDetune()
{
    const float DETUNE_DEADZONE = 0.03f;
    const float HALF_RANGE = 0.5f - DETUNE_DEADZONE; // 0.47

    float octaves;

    if (_osc2_detune < 0.5f - DETUNE_DEADZONE)
    {
        // Lower half: map [0, 0.47] → [-1, 0] octaves
        float t = _osc2_detune / HALF_RANGE; // 0→0, 0.47→1
        octaves = t - 1.0f;                  // 0→-1, 1→0
    }
    else if (_osc2_detune > 0.5f + DETUNE_DEADZONE)
    {
        // Upper half: map [0.53, 1.0] → [0, +1] octaves
        float t = (_osc2_detune - (0.5f + DETUNE_DEADZONE)) / HALF_RANGE;
        octaves = t; // 0→0, 1→+1
    }
    else
    {
        octaves = 0.0f; // Deadzone
    }

    // Convert octaves to frequency multiplier (log2 space)
    float freq_mult = powf(2.0f, octaves); // -1→0.5, 0→1.0, +1→2.0

    amy_event e = amy_default_event();
    e.synth = getSynthChannel();
    e.osc = OSC_2;
    e.freq_coefs[COEF_CONST] = freq_mult * 440.0f;
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
