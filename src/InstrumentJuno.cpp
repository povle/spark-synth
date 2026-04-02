#include "InstrumentJuno.h"
#include "juno_patches.h"
#include "Controls.h"

InstrumentJuno::InstrumentJuno()
{
    _instrumentName = "JUNO";
}

void InstrumentJuno::init()
{
    Serial.println("  [Juno] Initialized");
}

void InstrumentJuno::start()
{
    isActive = true;

    // 1. Let AMY configure the base patch
    amy_event e = amy_default_event();
    e.synth = 1;
    e.num_voices = 8;
    e.volume = 4.0f;
    e.patch_number = patch;
    amy_add_event(&e);

    Controls.lockAllPots();

    // 2. Load the state into our local struct
    state.loadFromSysex(patch);

    // 3. WAKE UP THE OSCILLATORS
    updateOscDuty(JUNO_OSC_PWM);
    updateOscAmps(JUNO_OSC_PWM);
    updateOscAmps(JUNO_OSC_SAW);
    updateOscAmps(JUNO_OSC_SUB);
    updateOscAmps(JUNO_OSC_NOISE);

    Serial.printf("  [Juno] Ready (patch %d)\n", patch);
}
void InstrumentJuno::stop()
{
    amy_event e = amy_default_event();
    e.synth = 1;
    e.velocity = 0.0f;
    amy_add_event(&e);
    isActive = false;
}

void InstrumentJuno::updatePot(uint8_t channel, float value)
{
    if (!isActive)
        return;

    switch (channel)
    {
    case 0: // Duty Cycle
        state.dco_pwm = value;
        updateOscDuty(JUNO_OSC_PWM);
        break;

    case 1: // Saw Level (Continuous)
        state.saw_level = value;
        updateOscAmps(JUNO_OSC_SAW);
        break;

    case 2: // Sub Level
        state.dco_sub = value;
        updateOscAmps(JUNO_OSC_SUB);
        break;

    case 15: // Noise Level
        state.dco_noise = value;
        updateOscAmps(JUNO_OSC_NOISE);
        break;

    case 3:
        state.hpf = (uint8_t)(value * 3.9f);
        updateHpf();
        break; // HPF

    case 4:
        state.vcf_freq = value;
        updateVcf();
        break;
    case 5:
        state.vcf_res = value;
        updateVcf();
        break;

    case 6:
        state.lfo_rate = value;
        updateLfo();
        break;
    case 7:
        state.vcf_lfo = value;
        updateVcf();
        break; // VCF LFO Mod depth

    case 8:
        state.env_a = value;
        updateAdsr();
        break;
    case 9:
        state.env_d = value;
        updateAdsr();
        break;
    case 10:
        state.env_s = value;
        updateAdsr();
        break;
    case 11:
        state.env_r = value;
        updateAdsr();
        break;
    }
}

void InstrumentJuno::updateOscAmps(int osc)
{
    amy_event e = amy_default_event();
    e.synth = 1;
    e.osc = osc;

    float target_amp = 0.005f; // Minimum baseline to keep osc awake

    switch (osc)
    {
    case JUNO_OSC_PWM:
        if (state.dco_pwm <= 0.01f)
        {
            target_amp = 0.005f; // Knob is zero, mute it.
        }
        else
        {
            target_amp = fmax(state.vca_level, 0.005f); // Knob is up, follow VCA.
        }
        break;

    case JUNO_OSC_SAW:
        target_amp = fmax(state.saw_level * state.vca_level, 0.005f);
        break;

    case JUNO_OSC_SUB:
        target_amp = fmax(state.dco_sub * state.vca_level, 0.005f);
        break;

    case JUNO_OSC_NOISE:
        target_amp = fmax(state.dco_noise * state.vca_level, 0.005f);
        break;
    }

    e.amp_coefs[0] = 0.0f;
    e.amp_coefs[1] = 0.0f;
    e.amp_coefs[2] = target_amp;

    e.amp_coefs[3] = state.vca_gate ? 0.0f : 1.0f;
    e.amp_coefs[4] = state.vca_gate ? 1.0f : 0.0f;
    e.amp_coefs[5] = 0.0f;
    e.amp_coefs[6] = 0.0f;

    amy_add_event(&e);
}

void InstrumentJuno::updateOscDuty(int osc)
{
    amy_event e = amy_default_event();
    e.synth = 1;
    e.osc = osc;

    if (state.dco_pwm <= 0.01f)
    {
        // Knob is off. Set to neutral square wave to wait silently.
        e.duty_coefs[0] = 0.5f;
        e.duty_coefs[5] = 0.0f;
    }
    else
    {
        // Remap the active part of the knob (0.02 - 1.0) back to 0.0 - 1.0
        float active_pwm = (state.dco_pwm - 0.02f) / 0.98f;
        if (active_pwm < 0.0f)
            active_pwm = 0.0f;

        if (state.pwm_manual)
        {
            // Juno manual PWM sweeps from 50% (Square) down to ~5% (Narrow)
            e.duty_coefs[0] = 0.5f - (0.45f * active_pwm);
            e.duty_coefs[5] = 0.0f;
        }
        else
        {
            // LFO mode: base is 50%, knob controls depth
            e.duty_coefs[0] = 0.5f;
            e.duty_coefs[5] = 0.5f * active_pwm;
        }
    }

    amy_add_event(&e);
}

void InstrumentJuno::updateVcf()
{
    amy_event e = amy_default_event();
    e.synth = 1;
    e.osc = JUNO_OSC_PWM; // VCF is usually on the root osc
    e.filter_freq_coefs[0] = 13.0f * powf(2.0f, 0.0938f * (state.vcf_freq * 127.0f));
    e.resonance = 0.7f * powf(2.0f, 4.0f * state.vcf_res);
    e.filter_freq_coefs[5] = 1.25f * state.vcf_lfo; // LFO modulation
    amy_add_event(&e);
}

void InstrumentJuno::updateAdsr()
{
    // Calculate times in ms using the Juno math curves, enforcing a 1ms minimum
    uint16_t a_ms = (uint16_t)fmax(6.0f + 8.0f * (state.env_a * 127.0f), 1.0f);
    uint16_t d_ms = (uint16_t)fmax(80.0f * powf(2.0f, 0.085f * (state.env_d * 127.0f)) - 80.0f, 1.0f);
    uint16_t r_ms = (uint16_t)fmax(70.0f * powf(2.0f, 0.066f * (state.env_r * 127.0f)) - 70.0f, 1.0f);

    amy_event e = amy_default_event();
    e.synth = 1;

    // Pair 0: Attack
    e.eg0_times[0] = a_ms;
    e.eg0_values[0] = 1.0f;

    // Pair 1: Decay to Sustain
    e.eg0_times[1] = d_ms;
    e.eg0_values[1] = state.env_s;

    // Pair 2: Release to 0.0
    e.eg0_times[2] = r_ms;
    e.eg0_values[2] = 0.0f;

    // Apply this envelope to all the sounding oscillators in the Juno patch
    e.osc = JUNO_OSC_PWM;
    amy_add_event(&e);
    e.osc = JUNO_OSC_SAW;
    amy_add_event(&e);
    e.osc = JUNO_OSC_SUB;
    amy_add_event(&e);
    e.osc = JUNO_OSC_NOISE;
    amy_add_event(&e);
}

void InstrumentJuno::updateLfo()
{
    amy_event e = amy_default_event();
    e.synth = 1;
    e.osc = JUNO_OSC_LFO;

    // Calculate LFO frequency
    e.freq_coefs[0] = fmax(0.6f * powf(2.0f, 0.04f * (state.lfo_rate * 127.0f)) - 0.1f, 0.001f);

    // Calculate LFO Delay time in ms
    uint16_t delay_ms = (uint16_t)fmax(18.0f * powf(2.0f, 0.066f * (state.lfo_delay_time * 127.0f)) - 13.0f, 1.0f);

    // Set up the LFO fade-in envelope using the C arrays
    e.eg0_times[0] = delay_ms;
    e.eg0_values[0] = 1.0f;

    e.eg0_times[1] = 10000;
    e.eg0_values[1] = 0.0f;

    amy_add_event(&e);
}

void InstrumentJuno::updateHpf()
{
    amy_event e = amy_default_event();
    e.synth = 1;
    if (state.hpf == 0)
    {
        e.eq_l = 7;
        e.eq_m = -3;
        e.eq_h = -3;
    }
    else if (state.hpf == 1)
    {
        e.eq_l = 0;
        e.eq_m = 0;
        e.eq_h = 0;
    }
    else if (state.hpf == 2)
    {
        e.eq_l = -8;
        e.eq_m = 0;
        e.eq_h = 0;
    }
    else
    {
        e.eq_l = -15;
        e.eq_m = 8;
        e.eq_h = 8;
    }
    amy_add_event(&e);
}


void InstrumentJuno::onPressedButton(uint8_t button_id)
{
    if (button_id == 2 && patch > 0) {
        patch--;
    } else if (button_id == 3 && patch < 127) {
        patch++;
    } else {
        return;
    }
    start();
}
