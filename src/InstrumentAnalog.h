#ifndef INSTRUMENT_ANALOG_H
#define INSTRUMENT_ANALOG_H

#include "Instrument.h"

class InstrumentAnalog : public Instrument
{
public:
    InstrumentAnalog();

    void init() override;
    void start() override;
    void stop() override;
    void onCustomPot(uint8_t channel, float value) override;
    void configNoise() override;
    void configLfo() override;
    void updateJoystickY(float y) override;

private:
    uint8_t _osc1_wave = SINE; // Default: SAW_DOWN
    uint8_t _osc2_wave = SINE;

    float _osc2_detune = 0.0f;
    float _osc_balance = 0.5f; // 0=osc1, 0.5=both, 1=osc2
    float _modWheel = 0.0f;

    // AMY oscillator indices
    static constexpr uint8_t OSC_1 = 0;
    static constexpr uint8_t OSC_2 = 1;
    static constexpr uint8_t OSC_NOISE = 2;
    static constexpr uint8_t OSC_LFO_FILTER = 3;
    static constexpr uint8_t OSC_LFO_PITCH = 4;

    void setupSynthVoices();
    void initFromPots();
    void updateOsc1Wave(uint8_t wave);
    void updateOsc2Wave(uint8_t wave);
    void updateOscDetune();
    void sendAdsr() override;
    void updateOscBalance();
    void applyPitchBend();
};

#endif
