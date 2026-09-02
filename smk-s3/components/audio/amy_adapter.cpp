#include "amy_adapter.h"
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdio>
#include <cstring>

extern "C" {
#include "amy.h"
void amy_platform_init() {}
void amy_platform_deinit() {}
}

namespace smk {

static const char* TAG = "AMY_ADAPTER";

AmyAdapter::AmyAdapter() {}

AmyAdapter::~AmyAdapter() {
    amy_stop();
}

bool AmyAdapter::begin(uint32_t sample_rate_hz) {
    ESP_LOGI(TAG, "Initializing AMY Synth Engine (Dedicated Audio Core 1 + SRAM Internal Cache)");
    
    amy_config_t config = amy_default_config();
    config.audio = AMY_AUDIO_IS_NONE;
    config.midi = AMY_MIDI_IS_NONE;
    config.platform.multicore = 0;  // Dedicated Core 1 audio task, keeping Core 0 100% free for USB MIDI
    config.platform.multithread = 0;
    config.features.default_synths = 1;
    config.features.reverb = 1;
    config.features.chorus = 1;
    config.features.echo = 1;
    config.features.startup_bleep = 0;
    config.ram_caps_events = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT; // High-speed SRAM for delta event queue
    config.ram_caps_synth = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;  // High-speed SRAM for voice & osc states
    config.ram_caps_block = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;  // High-speed SRAM for rendering block buffers
    config.ram_caps_fbl = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;    // High-speed SRAM for bus mixing buffers
    config.ram_caps_delay = MALLOC_CAP_SPIRAM;    // Reverb / Chorus / Echo in PSRAM
    config.ram_caps_sample = MALLOC_CAP_SPIRAM;   // PCM drum samples in PSRAM
    config.ram_caps_sysex = MALLOC_CAP_SPIRAM;
    config.overload_threshold = 0.95f;
    config.overload_ms = 500;
    config.max_oscs = 200;
    
    active_voices_.store(0);
    std::memset(active_notes_, 0, sizeof(active_notes_));

    amy_start(config);

    // Release unused upstream synth 2 (DX7 ch2 placeholder) to maximize clean oscillator headroom for Synth 1 & GM Drums
    amy_event e = amy_default_event();
    e.synth = 2;
    e.num_voices = 0;
    amy_add_event(&e);

    return true;
}

void AmyAdapter::noteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    channel &= 0x0F;
    note &= 0x7F;
    if (velocity > 0) {
        if (active_notes_[channel][note] < 255) {
            active_notes_[channel][note]++;
        }
        active_voices_++;
        amy_event e = amy_default_event();
        // Synth 10 is GM Drums (channel 9); all other channels route to main synth (synth 1)
        e.synth = (channel == 9) ? 10 : 1;
        e.midi_note = (float)note;
        e.velocity = (float)velocity / 127.0f;
        amy_add_event(&e);
    } else {
        noteOff(channel, note);
    }
}

void AmyAdapter::noteOff(uint8_t channel, uint8_t note) {
    channel &= 0x0F;
    note &= 0x7F;
    if (active_notes_[channel][note] > 0) {
        active_notes_[channel][note]--;
        if (active_voices_ > 0) active_voices_--;
    }
    amy_event e = amy_default_event();
    e.synth = (channel == 9) ? 10 : 1;
    e.midi_note = (float)note;
    e.velocity = 0.0f;
    amy_add_event(&e);
}

void AmyAdapter::pitchBend(uint8_t channel, int16_t value) {
    amy_event e = amy_default_event();
    e.synth = (channel == 9) ? 10 : 1;
    // MIDI Pitch Bend standard: 0..16383 (center 8192). Range: +/- 2 semitones
    e.pitch_bend = ((float)(value - 8192)) / (6.0f * 8192.0f);
    amy_add_event(&e);
}

void AmyAdapter::controlChange(uint8_t channel, uint8_t controller, uint8_t value) {
    uint8_t target_synth = (channel == 9) ? 10 : 1;
    if (controller == 64) { // Sustain Pedal
        amy_event e = amy_default_event();
        e.synth = target_synth;
        e.pedal = value;
        amy_add_event(&e);
    } else if (controller == 120 || controller == 123) { // All Sound Off / All Notes Off
        allNotesOff();
    } else {
        uint8_t msg[3] = { (uint8_t)(0xB0 | (channel & 0x0F)), controller, value };
        convert_midi_bytes_to_messages(msg, 3, 0);
    }
}

void AmyAdapter::allNotesOff() {
    std::memset(active_notes_, 0, sizeof(active_notes_));
    active_voices_.store(0);
    all_notes_off();
}

void AmyAdapter::panic() {
    allNotesOff();
    amy_reset_oscs();
}

static inline int16_t applySoftClip(int32_t sample) {
    constexpr int32_t kLinearThresh = 24576; // ~75% of full scale
    constexpr int32_t kMaxVal = 32760;
    if (sample > kLinearThresh) {
        int32_t diff = sample - kLinearThresh;
        int32_t headroom = kMaxVal - kLinearThresh;
        return (int16_t)(kLinearThresh + (diff * headroom) / (diff + headroom));
    } else if (sample < -kLinearThresh) {
        int32_t diff = -sample - kLinearThresh;
        int32_t headroom = kMaxVal - kLinearThresh;
        return (int16_t)(-kLinearThresh - (diff * headroom) / (diff + headroom));
    }
    return (int16_t)sample;
}

void AmyAdapter::setMasterGain(float gain) {
    if (gain < 0.0f) gain = 0.0f;
    if (gain > 2.0f) gain = 2.0f;
    master_gain_.store(gain, std::memory_order_relaxed);
}

int16_t* AmyAdapter::render() {
    amy_execute_deltas();
    amy_render(0, AMY_OSCS, 0);
    int16_t* buf = amy_fill_buffer();
    if (buf) {
        bool limiter = soft_limiter_enabled_.load(std::memory_order_relaxed);
        float gain = master_gain_.load(std::memory_order_relaxed);
        bool apply_gain = (gain != 1.0f);
        size_t total_samples = AMY_BLOCK_SIZE * AMY_NCHANS;

        if (limiter || apply_gain) {
            for (size_t i = 0; i < total_samples; ++i) {
                int32_t s = buf[i];
                if (apply_gain) {
                    s = (int32_t)((float)s * gain);
                }
                if (limiter) {
                    s = applySoftClip(s);
                } else {
                    if (s > 32767) s = 32767;
                    if (s < -32768) s = -32768;
                }
                buf[i] = (int16_t)s;
            }
        }

        // Fast copy for oscilloscope UI (no zero-crossing in real-time audio thread)
        size_t copy_samples = (total_samples < kRawScopeBufferSize)
                              ? total_samples : kRawScopeBufferSize;
        std::memcpy(scope_buffer_, buf, copy_samples * sizeof(int16_t));
    }
    return buf;
}

void AmyAdapter::getScopeSamples(int16_t* dest, size_t max_count, size_t* out_count) const {
    if (!dest || max_count == 0) return;
    size_t count = (max_count < kScopeBufferSize) ? max_count : kScopeBufferSize;
    
    // Find zero-crossing on-demand within UI task to stabilize waveform
    size_t start_offset = 0;
    size_t total_frames = kRawScopeBufferSize / 2;
    for (size_t i = 0; i < 32 && (i + count) < total_frames; ++i) {
        int16_t s0 = scope_buffer_[i * 2];
        int16_t s1 = scope_buffer_[(i + 1) * 2];
        if (s0 <= 0 && s1 > 0) {
            start_offset = i;
            break;
        }
    }
    
    for (size_t i = 0; i < count; ++i) {
        size_t idx = (start_offset + i) * 2;
        if (idx + 1 < kRawScopeBufferSize) {
            int32_t mixed = (int32_t)scope_buffer_[idx] + (int32_t)scope_buffer_[idx + 1];
            dest[i] = (int16_t)(mixed / 2);
        } else {
            dest[i] = 0;
        }
    }
    if (out_count) {
        *out_count = count;
    }
}

uint16_t AmyAdapter::blockSize() const {
    return AMY_BLOCK_SIZE;
}

float AmyAdapter::renderLoad() const {
    return amy_get_render_load();
}

uint32_t AmyAdapter::activeVoices() const {
    return active_voices_.load();
}

void AmyAdapter::setFilter(uint8_t synth_id, float cutoff_hz, float resonance) {
    amy_event e = amy_default_event();
    e.synth = (synth_id == 0) ? 1 : synth_id;
    e.filter_freq_coefs[COEF_CONST] = cutoff_hz;
    e.resonance = resonance;
    amy_add_event(&e);
}

void AmyAdapter::setOscillatorWaveform(uint8_t synth_id, uint8_t wave_type) {
    amy_event e = amy_default_event();
    e.synth = (synth_id == 0) ? 1 : synth_id;
    e.wave = wave_type;
    amy_add_event(&e);
}

void AmyAdapter::setEnvelope(uint8_t synth_id, float attack_ms, float decay_ms, float sustain_level, float release_ms) {
    amy_event e = amy_default_event();
    e.synth = (synth_id == 0) ? 1 : synth_id;
    e.amp_coefs[COEF_CONST] = 1.0f;
    e.amp_coefs[COEF_EG0] = 1.0f;
    e.bp_is_set[0] = 1;
    e.eg0_times[0] = (uint32_t)(attack_ms > 1.0f ? attack_ms : 1.0f);
    e.eg0_values[0] = 1.0f;
    e.eg0_times[1] = (uint32_t)(decay_ms > 1.0f ? decay_ms : 1.0f);
    e.eg0_values[1] = sustain_level;
    e.eg0_times[2] = (uint32_t)(release_ms > 1.0f ? release_ms : 1.0f);
    e.eg0_values[2] = 0.0f;
    amy_add_event(&e);
}

void AmyAdapter::loadPreset(uint8_t synth_id, uint16_t preset_id, uint8_t num_voices) {
    allNotesOff();
    amy_event e = amy_default_event();
    e.synth = (synth_id == 0) ? 1 : synth_id;
    e.patch_number = preset_id;
    e.num_voices = (num_voices > 0) ? num_voices : 8;
    amy_add_event(&e);
}

void AmyAdapter::sendAmyMessage(const char* message) {
    if (message) amy_play_message((char*)message);
}

void AmyAdapter::setFmModIndex(uint8_t synth_id, float mod_index) {
    amy_event e = amy_default_event();
    e.synth = (synth_id == 0) ? 1 : synth_id;
    e.ratio = mod_index;
    amy_add_event(&e);
}

void AmyAdapter::setFmFeedback(uint8_t synth_id, float feedback) {
    amy_event e = amy_default_event();
    e.synth = (synth_id == 0) ? 1 : synth_id;
    e.feedback = feedback;
    amy_add_event(&e);
}

void AmyAdapter::setFmRatio(uint8_t synth_id, float ratio) {
    amy_event e = amy_default_event();
    e.synth = (synth_id == 0) ? 1 : synth_id;
    e.ratio = ratio;
    amy_add_event(&e);
}

void AmyAdapter::setFmAlgorithm(uint8_t synth_id, uint8_t algo_id) {
    amy_event e = amy_default_event();
    e.synth = (synth_id == 0) ? 1 : synth_id;
    e.algorithm = algo_id;
    amy_add_event(&e);
}

void AmyAdapter::setChorus(float depth, float rate, float level) {
    amy_event e = amy_default_event();
    e.chorus_level = level;
    e.chorus_depth = depth;
    e.chorus_lfo_freq = rate;
    amy_add_event(&e);
}

void AmyAdapter::setReverb(float room_size, float damp, float mix) {
    amy_event e = amy_default_event();
    e.reverb_level = mix;
    e.reverb_liveness = room_size;
    e.reverb_damping = damp;
    amy_add_event(&e);
}

void AmyAdapter::setDelay(float delay_ms, float feedback, float mix) {
    amy_event e = amy_default_event();
    e.echo_level = mix;
    e.echo_delay_ms = delay_ms;
    e.echo_feedback = feedback;
    amy_add_event(&e);
}

void AmyAdapter::setSendLevels(uint8_t synth_id, float reverb_send, float chorus_send, float echo_send) {
    // Optional per-synth send levels
}

} // namespace smk
