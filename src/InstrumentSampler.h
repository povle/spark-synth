#ifndef INSTRUMENT_SAMPLER_H
#define INSTRUMENT_SAMPLER_H

#include "Instrument.h"
#include <ESP_I2S.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern "C"
{
    extern int16_t *pcm_load(uint16_t patch_number, uint32_t length, uint32_t samplerate, uint8_t channels, uint8_t midinote, uint32_t loopstart, uint32_t loopend);
    extern const uint16_t pcm_samples;
}

#define MAX_SAMPLE_SAMPLES (44100 * 4)
#define SAMPLE_RATE 44100

// ICS-43434 PDM Mic Pins
#define MIC_BCK 41
#define MIC_WS 42 // Now used for setPins()
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
    void updatePot(uint8_t channel, float value) override;
    void onPressedButton(uint8_t button_id) override;
    void update() override;
    void drawUI(U8G2 &u8g2, uint8_t y_offset) override;

private:
    I2SClass mic_i2s;
    int16_t *_record_buffer = nullptr;

    volatile bool isRecording = false;
    volatile uint32_t sample_index = 0;
    uint32_t sample_length = 0;
    TaskHandle_t _recordingTaskHandle = nullptr;

    // Flag: recording task sets this when done
    volatile bool recordingFinished = false;

    uint16_t _amy_preset_num = 0;

    static void recordingTaskWrapper(void *arg);
    void startRecording();
    void finishRecording(); // Called from main thread after task finishes
    void setupSynthVoices();
    void sendAllParams();
};

#endif
