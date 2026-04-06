#ifndef INSTRUMENT_SAMPLER_H
#define INSTRUMENT_SAMPLER_H

#include "Instrument.h"
#include <ESP_I2S.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern "C"
{
    extern int16_t *pcm_load(uint16_t preset_number, uint32_t length, uint32_t samplerate, uint8_t channels, uint8_t midinote, uint32_t loopstart, uint32_t loopend);
    extern const int16_t *pcm_get_sample_ram_for_preset(uint16_t preset_number, uint32_t *length);
    extern const uint16_t pcm_samples;
}

#define SAMPLE_RATE 44100
#define MAX_SAMPLE_SAMPLES (44100 * 5)

#define MIC_BCK 41
#define MIC_WS 42
#define MIC_DIN 40

class InstrumentSampler : public Instrument
{
public:
    InstrumentSampler();

    void init() override;
    void start() override;
    void stop() override;
    void noteOn(uint8_t note, float velocity) override;
    void noteOff(uint8_t note) override;
    void onCustomPot(uint8_t channel, float value) override;
    void onPressedButton(uint8_t button_id) override;
    void onReleasedButton(uint8_t button_id) override;
    void update() override;
    void drawUI(U8G2 &u8g2, uint8_t y_offset) override;
    uint8_t getSynthChannel() override { return 3; }

private:
    I2SClass mic_i2s;
    int16_t *_record_buffer = nullptr; // Full original recording

    volatile bool isRecording = false;
    volatile uint32_t sample_index = 0;
    uint32_t sample_length = 0;   // Current trimmed length
    uint32_t original_length = 0; // Original full recording length
    TaskHandle_t _recordingTaskHandle = nullptr;
    volatile bool recordingFinished = false;

    uint16_t _amy_preset_num = 0;

    // Dynamic controls
    float _sample_gain = 3.5f;
    uint32_t _trim_start = 0; // Start of trimmed region (samples)
    uint32_t _trim_end = 0;   // End of trimmed region (samples)

    // Debounce for trim knobs (reload only after knob stops)
    uint32_t _trim_knob_last_change = 0;              // millis() timestamp
    bool _trim_pending_reload = false;                // Flag: reload needed after debounce
    static constexpr uint32_t TRIM_DEBOUNCE_MS = 150; // Wait 150ms after last change

    void reloadTrimmedSample();
    void checkTrimReload();

    static void recordingTaskWrapper(void *arg);
    void startRecording();
    void finishRecording();
    void setupSynthVoices();
    void sendAllParams();
};

#endif
