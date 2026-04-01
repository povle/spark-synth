#ifndef INSTRUMENT_JUNO_H
#define INSTRUMENT_JUNO_H

#include "Instrument.h"
#include "juno_patches.h"

// Internal oscillator map for Juno patches in AMY
#define JUNO_OSC_PWM 0
#define JUNO_OSC_LFO 1
#define JUNO_OSC_SAW 2
#define JUNO_OSC_SUB 3
#define JUNO_OSC_NOISE 4

struct JunoState
{
    float lfo_rate, lfo_delay_time, dco_lfo, dco_pwm, dco_noise;
    float vcf_freq, vcf_res, vcf_env, vcf_lfo, vcf_kbd, vca_level;
    float env_a, env_d, env_s, env_r, dco_sub;

    // Original binary switches
    bool stop_16, stop_8, stop_4, pulse_en;
    bool pwm_manual, vcf_neg, vca_gate, cheap_filter;
    uint8_t chorus, hpf;

    // NEW: Continuous analog override for Sawtooth
    float saw_level;

    void loadFromSysex(int patchIndex)
    {
        uint8_t sysex[18];
        for (int i = 0; i < 18; i++)
        {
            sysex[i] = pgm_read_byte(&juno_sysex[patchIndex][i]);
        }

        // Continuous sliders (0-127)
        lfo_rate = sysex[0] / 127.0f;
        lfo_delay_time = sysex[1] / 127.0f;
        dco_lfo = sysex[2] / 127.0f;
        dco_pwm = sysex[3] / 127.0f;
        dco_noise = sysex[4] / 127.0f;
        vcf_freq = sysex[5] / 127.0f;
        vcf_res = sysex[6] / 127.0f;
        vcf_env = sysex[7] / 127.0f;
        vcf_lfo = sysex[8] / 127.0f;
        vcf_kbd = sysex[9] / 127.0f;
        vca_level = sysex[10] / 127.0f;
        env_a = sysex[11] / 127.0f;
        env_d = sysex[12] / 127.0f;
        env_s = sysex[13] / 127.0f;
        env_r = sysex[14] / 127.0f;
        dco_sub = sysex[15] / 127.0f;

        // Byte 16: Switches
        stop_16 = (sysex[16] & (1 << 0)) > 0;
        stop_8 = (sysex[16] & (1 << 1)) > 0;
        stop_4 = (sysex[16] & (1 << 2)) > 0;
        pulse_en = (sysex[16] & (1 << 3)) > 0;

        // Convert binary SAW to continuous float
        bool saw_binary = (sysex[16] & (1 << 4)) > 0;
        saw_level = saw_binary ? 1.0f : 0.0f;

        // Chorus mapping
        uint8_t cho_raw = (sysex[16] >> 5) & 0x03;
        const uint8_t cho_map[] = {2, 0, 1, 0};
        chorus = cho_map[cho_raw];

        // Byte 17: Switches
        pwm_manual = (sysex[17] & (1 << 0)) > 0;
        vcf_neg = (sysex[17] & (1 << 1)) > 0;
        vca_gate = (sysex[17] & (1 << 2)) > 0;

        // HPF mapping
        uint8_t hpf_raw = (sysex[17] >> 3) & 0x03;
        hpf = 3 - hpf_raw; // Invert to match standard Juno 0-3

        cheap_filter = (sysex[17] & (1 << 5)) > 0;
    }
};

class InstrumentJuno : public Instrument
{
public:
    InstrumentJuno();
    void init() override;
    void start() override;
    void stop() override;
    void updatePot(uint8_t channel, float value) override;
    void onPressedButton(uint8_t button_id) override;
    void drawUI(U8G2 &u8g2, uint8_t y_offset) override;

private:
    uint8_t patch = 0;
    JunoState state;

    void loadPatchState(uint8_t patchIdx);
    void updateOscAmps(int osc);
    void updateOscDuty(int osc);
    void updateVcf();
    void updateAdsr();
    void updateLfo();
    void updateHpf();

    // Math helpers from juno.py
    float calc_vcf_freq(float val);
    float calc_adsr_time(float val, float base, float factor);
};

#endif
