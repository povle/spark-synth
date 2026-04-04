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
    needsUIRedraw = true;
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

    // Send note event with gain applied to velocity
    amy_event e = amy_default_event();
    e.synth = getSynthChannel();
    e.midi_note = note;
    e.velocity = velocity * _sample_gain; // Apply amplification
    // NO preset/patch/wave here - set in setupSynthVoices only
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

void InstrumentSampler::onCustomPot(uint8_t channel, float value)
{
    // Knob 0: Amplification (0.0x to 5.0x)
    if (channel == 0)
    {
        _sample_gain = 0.0f + (value * 5.0f);

        amy_event e = amy_default_event();
        e.synth = getSynthChannel();
        e.amp_coefs[0] = _sample_gain;
        amy_add_event(&e);

        Serial.printf("  [Sampler] Gain: %.2fx\n", _sample_gain);
    }

    // Knob 1: Trim start - set flag for debounced reload
    if (channel == 1)
    {
        _trim_start = (uint32_t)(value * 0.9f * (float)original_length);
        _trim_knob_last_change = millis();
        _trim_pending_reload = true; // ← Set flag, don't reload yet
        Serial.printf("  [Sampler] Trim start: %.2fs (pending)\n",
                      (float)_trim_start / SAMPLE_RATE);
    }

    // Knob 2: Trim end - set flag for debounced reload
    if (channel == 2)
    {
        _trim_end = (uint32_t)((0.1f + value * 0.9f) * (float)original_length);
        _trim_knob_last_change = millis();
        _trim_pending_reload = true; // ← Set flag, don't reload yet
        Serial.printf("  [Sampler] Trim end: %.2fs (pending)\n",
                      (float)_trim_end / SAMPLE_RATE);
    }

    // Existing pots 4-11 (filter/ADSR)
    if (channel < 4)
    {
        params.custom[channel] = value;
    }
    needsUIRedraw = true;
}

void InstrumentSampler::onPressedButton(uint8_t button_id)
{
    if (!isActive)
        return;

    if (button_id == 2 && !isRecording)
    {
        startRecording();
        needsUIRedraw = true;
    }
}

void InstrumentSampler::onReleasedButton(uint8_t button_id)
{
    if (!isActive)
        return;

    if (button_id == 2 && isRecording)
    {
        Serial.println("  [Sampler] Stopping...");
        isRecording = false;
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

    checkTrimReload();
}

void InstrumentSampler::drawUI(U8G2 &u8g2, uint8_t y_offset)
{
    if (isRecording)
    {
        u8g2.setFont(u8g2_font_callite24_tr);
        u8g2.setCursor(0, y_offset + 17);
        u8g2.print("REC");
        int progress = (sample_index * 128) / MAX_SAMPLE_SAMPLES;
        if (progress > 128)
            progress = 128;
        u8g2.drawFrame(0, y_offset + 20, 128, 10);
        u8g2.drawBox(2, y_offset + 22, progress, 6);
        u8g2.setFont(u8g2_font_7x13_tr);
        u8g2.setCursor(0, y_offset + 45);
        u8g2.printf("%.1fs", (float)sample_index / SAMPLE_RATE);
    }
    else if (original_length == 0)
    {
        const int CIRCLES_FIRST_X = 32;
        const int CIRCLES_LAST_X = 128 - 32;
        const int CIRCLES_SPACING_X = (CIRCLES_LAST_X - CIRCLES_FIRST_X) / 3;
        const int CIRCLES_Y = y_offset + 16;

        int circle_x = CIRCLES_FIRST_X;
        for (int i = 0; i < 4; i++) {
            circle_x = CIRCLES_FIRST_X + CIRCLES_SPACING_X * i;
            if (i == 2) {
                u8g2.drawDisc(circle_x, CIRCLES_Y, 4);
                u8g2.setFont(u8g2_font_streamline_hand_signs_t);
                u8g2.setCursor(circle_x-5, CIRCLES_Y + 25);
                u8g2.print("0");
            }
            else {
                u8g2.drawCircle(circle_x, CIRCLES_Y, 4);
            }
        }

    }
    else
    {
        // Waveform area: 128px wide, 40px tall, centered
        const int WAVE_X = 0;
        const int WAVE_Y = y_offset + 2;
        const int WAVE_W = 128;
        const int WAVE_H = 40;
        const int WAVE_CENTER = WAVE_Y + WAVE_H / 2;

        // Draw zero baseline
        u8g2.drawHLine(WAVE_X, WAVE_CENTER, WAVE_W);

        // Draw symmetric waveform using max absolute value per pixel
        if (original_length > 0 && _record_buffer != nullptr)
        {

            // Calculate visible region (trimmed portion)
            uint32_t vis_start = _trim_start;
            uint32_t vis_end = _trim_end;
            if (vis_end <= vis_start)
                vis_end = vis_start + 441; // Min 10ms
            if (vis_end > original_length)
                vis_end = original_length;
            uint32_t vis_len = vis_end - vis_start;

            for (int x = 0; x < WAVE_W; x++)
            {
                // Map pixel column to sample range in VISIBLE (trimmed) region
                uint32_t s_start = vis_start + (x * vis_len) / WAVE_W;
                uint32_t s_end = vis_start + ((x + 1) * vis_len) / WAVE_W;

                if (s_end > vis_end)
                    s_end = vis_end;
                if (s_end <= s_start)
                    s_end = s_start + 1;
                if (s_start >= original_length)
                    s_start = original_length - 1;
                if (s_end > original_length)
                    s_end = original_length;

                // Find MAX ABSOLUTE value in this window, apply gain
                int16_t max_abs = 0;
                for (uint32_t s = s_start; s < s_end; s++)
                {
                    if (s >= original_length)
                        break;
                    int32_t val = (int32_t)_record_buffer[s] * (int32_t)(_sample_gain * 256);
                    val = val >> 8; // Fixed-point gain apply
                    int16_t scaled = (int16_t)val;
                    int16_t abs_val = (scaled < 0) ? -scaled : scaled;
                    if (abs_val > max_abs)
                        max_abs = abs_val;
                }

                // Convert max_abs to pixel height (symmetric around center)
                const float SCALE = (WAVE_H / 2.0f - 1) / 32768.0f;
                int half_height = (int)(max_abs * SCALE);
                if (half_height < 1 && max_abs > 0)
                    half_height = 1;

                // Draw symmetric vertical bar
                int y_top = WAVE_CENTER - half_height;
                int y_bottom = WAVE_CENTER + half_height;

                if (y_top < WAVE_Y)
                    y_top = WAVE_Y;
                if (y_bottom > WAVE_Y + WAVE_H - 1)
                    y_bottom = WAVE_Y + WAVE_H - 1;

                if (y_bottom >= y_top)
                {
                    u8g2.drawVLine(WAVE_X + x, y_top, y_bottom - y_top + 1);
                }
            }

            u8g2.setFont(u8g2_font_5x7_tr);
            u8g2.setCursor(2, WAVE_Y + WAVE_H + 2);
            u8g2.printf("%.1fs", (float)vis_start / SAMPLE_RATE);
            u8g2.setCursor(90, WAVE_Y + WAVE_H + 2);
            u8g2.printf("%.1fs", (float)vis_end / SAMPLE_RATE);
        }
    }
}
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

    liveUI = true;

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
    liveUI = false;

    if (sample_length > 0)
    {
        // Store original length before trimming
        original_length = sample_length;

        // Set initial trim to full sample (minus button click)
        const uint32_t CLICK_TRIM = SAMPLE_RATE / 10; // 0.1s
        _trim_start = (original_length > CLICK_TRIM * 2) ? CLICK_TRIM : 0;
        _trim_end = (original_length > CLICK_TRIM) ? original_length - CLICK_TRIM : original_length;

        Serial.printf("  [Sampler] Trimming clicks: %lu -> %lu samples\n",
                      original_length, _trim_end - _trim_start);

        // Load initial trimmed sample
        int16_t *amy_buf = pcm_load(
            _amy_preset_num,
            _trim_end - _trim_start, // Initial trimmed length
            SAMPLE_RATE,
            1,
            60,
            0, 0);

        if (amy_buf != nullptr)
        {
            Serial.println("  [Sampler] pcm_load OK");
            memcpy(amy_buf, _record_buffer + _trim_start, (_trim_end - _trim_start) * sizeof(int16_t));

            sample_length = _trim_end - _trim_start; // Update for UI

            Serial.printf("  [DBG] Loaded %lu samples (preset %d)\n", sample_length, _amy_preset_num);

            const int16_t *check = pcm_get_sample_ram_for_preset(_amy_preset_num, nullptr);
            if (check == nullptr)
            {
                Serial.println("  [Sampler] ERROR: Preset not registered!");
                return;
            }

            setupSynthVoices();
            Serial.printf("  [Sampler] ✓ Ready! Adjust gain/loop with knobs 0-2\n");
        }
        else
        {
            Serial.println("  [Sampler] ERROR: pcm_load returned NULL!");
        }
    }
    else
    {
        Serial.println("  [Sampler] Cancelled (0 samples)");
    }
    needsUIRedraw = true;
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

// Re-load sample with new trim/loop points - simple and reliable
void InstrumentSampler::reloadTrimmedSample()
{
    if (sample_length == 0 || _record_buffer == nullptr)
        return;

    // Ensure valid trim range
    if (_trim_end <= _trim_start)
    {
        _trim_end = _trim_start + 441; // Minimum 10ms
    }
    if (_trim_end > original_length)
    {
        _trim_end = original_length;
    }

    uint32_t trimmed_len = _trim_end - _trim_start;
    if (trimmed_len < 100)
        return; // Too short

    Serial.printf("  [Sampler] Re-loading: %lu-%lu (%lu samples)\n",
                  _trim_start, _trim_end, trimmed_len);

    // Re-load preset with trimmed length and loop points
    int16_t *amy_buf = pcm_load(
        _amy_preset_num,
        trimmed_len, // ← Trimmed length
        SAMPLE_RATE,
        1,              // mono
        60,             // base MIDI note
        0,              // loop start = 0 (relative to trimmed sample)
        trimmed_len - 1 // loop end = full trimmed sample
    );

    if (amy_buf != nullptr)
    {
        // Copy ONLY the trimmed portion
        memcpy(amy_buf, _record_buffer + _trim_start, trimmed_len * sizeof(int16_t));

        // Update displayed length
        sample_length = trimmed_len;

        // Re-configure synth (no RESET_AMY - preserves preset)
        amy_event e = amy_default_event();
        e.synth = getSynthChannel();
        e.patch_number = 1024; // Custom patch base
        e.num_voices = 8;
        amy_add_event(&e);

        for (int i = 0; i < 20; i++)
            amy_update();

        Serial.printf("  [Sampler] ✓ Re-loaded %.2fs sample\n",
                      (float)trimmed_len / SAMPLE_RATE);
    }
    else
    {
        Serial.println("  [Sampler] ERROR: pcm_load failed on re-load!");
    }
}

void InstrumentSampler::checkTrimReload()
{
    if (!_trim_pending_reload)
        return;

    // Only reload if debounce timer has expired
    if (millis() - _trim_knob_last_change >= TRIM_DEBOUNCE_MS)
    {
        _trim_pending_reload = false;
        reloadTrimmedSample();
    }
}
