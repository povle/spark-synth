#include "InstrumentSampler.h"

InstrumentSampler::InstrumentSampler()
{
    _instrumentName = "SAMPLER";
}

void InstrumentSampler::init()
{
    Serial.println("  [Sampler] Initializing...");

    // Allocate buffer in PSRAM
    _record_buffer = (int16_t *)heap_caps_malloc(MAX_SAMPLE_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM);

    if (_record_buffer == nullptr)
    {
        Serial.println("  [Sampler] FATAL: PSRAM allocation failed!");
        return;
    }
    Serial.printf("  [Sampler] PSRAM buffer: %lu bytes OK\n", MAX_SAMPLE_SAMPLES * sizeof(int16_t));

    // I2S Mic Configuration (ICS-43434 wired as I2S)
    mic_i2s.setPins(MIC_BCK, MIC_WS, -1, MIC_DIN);

    bool ok = mic_i2s.begin(
        I2S_MODE_STD,
        SAMPLE_RATE, // ← Must match AMY_SAMPLE_RATE!
        I2S_DATA_BIT_WIDTH_32BIT,
        I2S_SLOT_MODE_MONO,
        I2S_STD_SLOT_LEFT);

    if (!ok)
    {
        Serial.println("  [Sampler] ERROR: Mic I2S init failed!");
        return;
    }
    Serial.println("  [Sampler] Mic I2S OK");

    // Test mic
    Serial.println("  [Sampler] Testing mic...");
    int32_t test_buf[32];
    size_t test_read = mic_i2s.readBytes((char *)test_buf, sizeof(test_buf));
    Serial.printf("  [Sampler] Test read: %lu bytes, sample: %ld\n",
                  test_read, test_read > 0 ? test_buf[0] : -1);

    if (test_read == 0)
    {
        Serial.println("  [Sampler] WARNING: Mic not reading!");
    }

    // Get first custom preset number (after ROM presets)
    _amy_preset_num = pcm_samples;
    Serial.printf("  [Sampler] Custom PRESET number: %d (NOT patch!)\n", _amy_preset_num);
    Serial.println("  [Sampler] Init complete\n");
}

void InstrumentSampler::start()
{
    isActive = true;
    setupSynthVoices();

    if (sample_length > 0)
    {
        //
        // sendAllParams();
        Serial.printf("  [Sampler] Ready: %lu samples (%.2fs)\n",
                      sample_length, (float)sample_length / SAMPLE_RATE);
    }
    else
    {
        Serial.println("  [Sampler] Ready. Press Button 2 to record.");
    }
}

void InstrumentSampler::stop()
{
    if (isRecording)
    {
        isRecording = false;
        if (_recordingTaskHandle)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            _recordingTaskHandle = nullptr;
        }
    }

    // Silence synth 3
    amy_event e = amy_default_event();
    e.synth = getSynthChannel();
    e.velocity = 0.0f;
    amy_add_event(&e);

    isActive = false;
}

void InstrumentSampler::noteOn(uint8_t note, float velocity)
{
    if (!isActive || isRecording || sample_length == 0)
        return;

    Serial.printf("  [Sampler] NOTE ON: %d %f\n", note, velocity);

    amy_event e = amy_default_event();
    e.synth = getSynthChannel();
    e.midi_note = note;
    e.velocity = velocity;

    // NO e.preset, NO e.patch_number, NO e.wave!
    amy_add_event(&e);
}

void InstrumentSampler::noteOff(uint8_t note)
{
    if (!isActive || isRecording)
        return;

    amy_event e = amy_default_event();
    e.synth = getSynthChannel();
    e.midi_note = note;
    e.velocity = 0.0f;
    amy_add_event(&e);
}

void InstrumentSampler::updatePot(uint8_t channel, float value)
{
    if (channel < 4)
    {
        params.custom[channel] = value;
        return;
    }

    switch (channel)
    {
    case 4:
        params.cutoff = 50.0f + (value * 8000.0f);
        break;
    case 5:
        params.resonance = 0.5f + (value * 4.5f);
        break;
    case 8:
        params.attack = value * 2.0f;
        break;
    case 9:
        params.decay = value * 2.0f;
        break;
    case 10:
        params.sustain = value;
        break;
    case 11:
        params.release = value * 5.0f;
        break;
    }
    sendAllParams();
}

void InstrumentSampler::onPressedButton(uint8_t button_id)
{
    if (!isActive)
        return;

    if (button_id == 2)
    {
        if (isRecording)
        {
            Serial.println("  [Sampler] Stopping...");
            isRecording = false;
            // finishRecording() called from update()
        }
        else
        {
            startRecording();
        }
    }
}

void InstrumentSampler::update()
{
    // Check if recording finished and process in main thread
    if (recordingFinished && !isRecording)
    {
        recordingFinished = false;
        finishRecording(); // Main thread has full stack
    }
}

void InstrumentSampler::drawUI(U8G2 &u8g2, uint8_t y_offset)
{
    u8g2.setFont(u8g2_font_logisoso16_tr);
    u8g2.setCursor(0, y_offset + 20);

    if (isRecording)
    {
        u8g2.print("REC");
        int progress = (sample_index * 128) / MAX_SAMPLE_SAMPLES;
        if (progress > 128)
            progress = 128;
        u8g2.drawFrame(0, y_offset + 30, 128, 10);
        u8g2.drawBox(2, y_offset + 32, progress, 6);
        u8g2.setFont(u8g2_font_7x13_tr);
        u8g2.setCursor(0, y_offset + 45);
        u8g2.printf("%.1fs", (float)sample_index / SAMPLE_RATE);
    }
    else
    {
        u8g2.print("SAMPLER");
        u8g2.setFont(u8g2_font_7x13_tr);
        u8g2.setCursor(0, y_offset + 35);
        if (sample_length == 0)
        {
            u8g2.print("Empty. BTN2.");
        }
        else
        {
            u8g2.printf("%.2fs", (float)sample_length / SAMPLE_RATE);
        }
    }
}

// === RECORDING TASK - Minimal Work ===

void InstrumentSampler::recordingTaskWrapper(void *arg)
{
    InstrumentSampler *self = (InstrumentSampler *)arg;
    int32_t raw_buffer[128];

    Serial.println("  [Sampler] Recording task STARTED");

    while (self->isRecording && self->sample_index < MAX_SAMPLE_SAMPLES)
    {
        size_t bytes_read = self->mic_i2s.readBytes((char *)raw_buffer, sizeof(raw_buffer));

        if (bytes_read > 0)
        {
            int samples_read = bytes_read / sizeof(int32_t);

            for (int i = 0; i < samples_read; i++)
            {
                int32_t s32 = raw_buffer[i];
                int16_t s16 = (int16_t)(s32 >> 16); // 32-bit to 16-bit conversion

                if (self->sample_index < MAX_SAMPLE_SAMPLES)
                {
                    self->_record_buffer[self->sample_index++] = s16;
                }
            }
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    Serial.printf("  [Sampler] Recording DONE: %lu samples\n", self->sample_index);

    // Signal main thread - DON'T call AMY functions here!
    self->sample_length = self->sample_index;
    self->recordingFinished = true;
    self->_recordingTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

void InstrumentSampler::startRecording()
{
    if (isRecording || _record_buffer == nullptr)
        return;

    Serial.println("  [Sampler] === STARTING RECORDING ===");

    isRecording = true;
    sample_index = 0;
    recordingFinished = false;
    sample_length = 0;

    // Silence synth 3
    amy_event e = amy_default_event();
    e.synth = getSynthChannel();
    e.velocity = 0.0f;
    amy_add_event(&e);

    // 16KB stack for recording task
    BaseType_t result = xTaskCreatePinnedToCore(
        recordingTaskWrapper,
        "SamplerRec",
        16384,
        this,
        10,
        &_recordingTaskHandle,
        1);

    if (result != pdPASS)
    {
        Serial.println("  [Sampler] ERROR: Task create failed!");
        isRecording = false;
    }
}

// === FINISH RECORDING - Main Thread ===

void InstrumentSampler::finishRecording()
{
    Serial.println("  [Sampler] === FINISHING RECORDING ===");
    Serial.printf("  [Sampler] Captured: %lu samples @ %d Hz\n", sample_length, SAMPLE_RATE);

    if (sample_length > 0)
    {
        // === TRIM BUTTON CLICK NOISE ===
        // Remove first and last 0.15 seconds
        const uint32_t TRIM_SAMPLES = SAMPLE_RATE / 7; // 0.15 seconds
        uint32_t trim_start = TRIM_SAMPLES;
        uint32_t trim_end = (sample_length > TRIM_SAMPLES) ? sample_length - TRIM_SAMPLES : sample_length;
        uint32_t trimmed_length = (trim_end > trim_start) ? trim_end - trim_start : 0;

        Serial.printf("  [Sampler] Trimming: %d start + %d end = %d samples removed\n",
                      TRIM_SAMPLES, TRIM_SAMPLES, sample_length - trimmed_length);
        Serial.printf("  [Sampler] Original: %lu samples, Trimmed: %lu samples (%.2fs)\n",
                      sample_length, trimmed_length, (float)trimmed_length / SAMPLE_RATE);

        if (trimmed_length < 100)
        {
            Serial.println("  [Sampler] WARNING: Too short after trim, skipping load");
            return;
        }

        // Verify sample range (on trimmed portion)
        int16_t min_val = 32767, max_val = -32768;
        for (uint32_t i = trim_start; i < trim_end && i < trim_start + 100; i++)
        {
            if (_record_buffer[i] < min_val)
                min_val = _record_buffer[i];
            if (_record_buffer[i] > max_val)
                max_val = _record_buffer[i];
        }
        Serial.printf("  [Sampler] Trimmed sample range: %d to %d\n", min_val, max_val);

        Serial.printf("  [Sampler] Loading into AMY as PRESET %d...\n", _amy_preset_num);

        // Load sample into AMY - use TRIMMED length!
        int16_t *amy_buf = pcm_load(
            _amy_preset_num,
            trimmed_length, // ← Use trimmed length
            SAMPLE_RATE,
            1,   // mono
            60,  // base MIDI note
            0, 0 // no loop
        );

        if (amy_buf != nullptr)
        {
            Serial.println("  [Sampler] pcm_load returned valid pointer");

            // Copy TRIMMED samples to AMY's buffer (skip first/last 0.1s)
            memcpy(amy_buf, _record_buffer + trim_start, trimmed_length * sizeof(int16_t));

            // Verify copy
            Serial.printf("  [DBG] Buffer[%lu]=%d, AMY[0]=%d\n", trim_start, _record_buffer[trim_start], amy_buf[0]);

            // Verify preset is registered
            const int16_t *check = pcm_get_sample_ram_for_preset(_amy_preset_num, nullptr);
            Serial.printf("  [DBG] pcm_get_sample_ram_for_preset(%d) = %p\n",
                          _amy_preset_num, (void *)check);

            if (check == nullptr)
            {
                Serial.println("  [Sampler] ERROR: Preset not registered!");
                return;
            }

            // Update sample_length for display/UI (use trimmed value)
            sample_length = trimmed_length;

            // Configure synth for PCM playback
            setupSynthVoices();

            Serial.printf("  [Sampler] ✓ PRESET %d loaded! Play keys.\n", _amy_preset_num);
        }
        else
        {
            Serial.println("  [Sampler] ERROR: pcm_load returned NULL!");
            Serial.println("  [Sampler] Try reducing MAX_SAMPLE_SAMPLES");
        }
    }
    else
    {
        Serial.println("  [Sampler] Cancelled (0 samples)");
    }
}

void InstrumentSampler::setupSynthVoices()
{
    amy_event e = amy_default_event();
    e.reset_osc = RESET_PATCH;
    e.oscs_per_voice = 1;
    e.patch_number = 1024;
    amy_add_event(&e);

    e = amy_default_event();
    e.osc = 0;
    e.patch_number = 1024;
    e.wave = PCM;
    e.preset = _amy_preset_num;
    amy_add_event(&e);

    e = amy_default_event();
    e.synth = getSynthChannel();
    e.patch_number = 1024;
    e.num_voices = 8;
    e.volume = 6.0f;
    amy_add_event(&e);

    sendAllParams();

    Serial.printf("  [Sampler] PCM synth configured (synth=%d, preset=%d)\n",
                  getSynthChannel(), _amy_preset_num);
}

void InstrumentSampler::sendAllParams()
{
    amy_event e = amy_default_event();
    e.synth = getSynthChannel();
    e.filter_freq_coefs[0] = params.cutoff;
    e.resonance = params.resonance;
    amy_add_event(&e);

    e = amy_default_event();
    e.synth = getSynthChannel();
    e.eg0_times[0] = 0;
    e.eg0_values[0] = 1.0f;
    e.eg0_times[1] = (uint16_t)(params.decay * 1000);
    e.eg0_values[1] = params.sustain;
    e.eg0_times[2] = (uint16_t)(params.release * 1000);
    e.eg0_values[2] = 0.0f;
    amy_add_event(&e);
}
