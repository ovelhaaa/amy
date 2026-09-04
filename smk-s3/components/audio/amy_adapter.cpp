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
    ESP_LOGI(TAG, "Initializing AMY Synth Engine (Dedicated Audio Core 1 + Dual Bus Architecture)");
    
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
    config.max_oscs = 120; // Aligned with app_config.h kMaxOscillators (120 oscs)
    
    active_voices_.store(0);
    std::memset(active_notes_, 0, sizeof(active_notes_));

    amy_start(config);

    // Release unused upstream synth 2 (DX7 ch2 placeholder) and synth 0 (bleep placeholder)
    amy_event e = amy_default_event();
    e.synth = 2;
    e.num_voices = 0;
    amy_add_event(&e);

    e = amy_default_event();
    e.synth = 0;
    e.num_voices = 0;
    amy_add_event(&e);

    // Configure Synth 1 (Main Synth): Bus 0 with 4ms voice-stealing micro-fade
    amy_event e1 = amy_default_event();
    e1.synth = 1;
    e1.bus = 0;
    e1.synth_delay_ms = 4;
    amy_add_event(&e1);

    // Configure Synth 10 (GM Drums): Bus 1 with punchy dry mix
    amy_event e10 = amy_default_event();
    e10.synth = 10;
    e10.bus = 1;
    amy_add_event(&e10);

    // Master bus volume headroom
    amy_event ev = amy_default_event();
    ev.volume[0] = 0.85f; // Synth bus
    ev.volume[1] = 0.90f; // Drum bus
    amy_add_event(&ev);

    // Set Bus 1 (Drum Bus) tailored 3-band EQ and zero FX sends (keeps kick/snare punchy and dry)
    amy_event drum_eq = amy_default_event();
    drum_eq.bus = 1;
    drum_eq.eq_l = 2.5f;   // +2.5dB solid low end for kick
    drum_eq.eq_m = 0.8f;   // Clean mid scoop
    drum_eq.eq_h = 1.8f;   // +1.8dB crisp top end for snare/hi-hat
    drum_eq.reverb_level = 0.0f;
    drum_eq.chorus_level = 0.0f;
    drum_eq.echo_level = 0.0f;
    amy_add_event(&drum_eq);

    return true;
}

void AmyAdapter::noteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    channel &= 0x0F;
    note &= 0x7F;
    if (velocity > 0) {
        if (active_notes_[channel][note] < 255) {
            active_notes_[channel][note]++;
        }

        // Synth 10 is GM Drums (channel 9)
        if (channel == 9) {
            active_voices_++;
            amy_event e = amy_default_event();
            e.synth = 10;
            e.midi_note = (float)note;
            e.velocity = (float)velocity / 127.0f;
            amy_add_event(&e);
            return;
        }

        // Main Synth (Synth 1)
        if (mono_mode_) {
            // Remove note if already in stack (Last-Note Priority)
            int found = -1;
            for (int i = 0; i < mono_stack_size_; ++i) {
                if (mono_stack_[i] == note) { found = i; break; }
            }
            if (found >= 0) {
                for (int i = found; i < mono_stack_size_ - 1; ++i) {
                    mono_stack_[i] = mono_stack_[i + 1];
                }
                mono_stack_size_--;
            }
            if (mono_stack_size_ < kMonoStackCap) {
                mono_stack_[mono_stack_size_++] = note;
            }

            active_voices_.store(1, std::memory_order_relaxed);

            amy_event e = amy_default_event();
            e.synth = 1;
            e.midi_note = (float)note;
            if (mono_stack_size_ == 1) {
                // Initial note onset: trigger attack envelope
                e.velocity = (float)velocity / 127.0f;
            } else {
                // Legato glide: do not set velocity so envelope doesn't re-trigger or cut
            }
            amy_add_event(&e);
        } else {
            // Polyphonic Mode
            active_voices_++;
            amy_event e = amy_default_event();
            e.synth = 1;
            e.midi_note = (float)note;
            e.velocity = (float)velocity / 127.0f;
            amy_add_event(&e);
        }
    } else {
        noteOff(channel, note);
    }
}

void AmyAdapter::noteOff(uint8_t channel, uint8_t note) {
    channel &= 0x0F;
    note &= 0x7F;
    if (active_notes_[channel][note] > 0) {
        active_notes_[channel][note]--;
    }

    if (channel == 9) {
        if (active_voices_ > 0) active_voices_--;
        amy_event e = amy_default_event();
        e.synth = 10;
        e.midi_note = (float)note;
        e.velocity = 0.0f;
        amy_add_event(&e);
        return;
    }

    // Main Synth (Synth 1)
    if (mono_mode_) {
        int found = -1;
        for (int i = 0; i < mono_stack_size_; ++i) {
            if (mono_stack_[i] == note) { found = i; break; }
        }
        if (found < 0) return; // Note not in stack

        bool was_top = (found == mono_stack_size_ - 1);

        for (int i = found; i < mono_stack_size_ - 1; ++i) {
            mono_stack_[i] = mono_stack_[i + 1];
        }
        mono_stack_size_--;

        if (mono_stack_size_ == 0) {
            // All keys released: trigger voice release
            active_voices_.store(0, std::memory_order_relaxed);
            amy_event e = amy_default_event();
            e.synth = 1;
            e.midi_note = (float)note;
            e.velocity = 0.0f;
            amy_add_event(&e);
        } else if (was_top) {
            // Active note was released, but older note(s) still held down:
            // Glide smoothly back to previous held note without re-triggering attack envelope
            uint8_t prev_note = mono_stack_[mono_stack_size_ - 1];
            amy_event e = amy_default_event();
            e.synth = 1;
            e.midi_note = (float)prev_note;
            amy_add_event(&e);
        }
        // If was not top, the currently sounding note continues playing uninterrupted
    } else {
        // Polyphonic Mode
        if (active_voices_ > 0) active_voices_--;
        amy_event e = amy_default_event();
        e.synth = 1;
        e.midi_note = (float)note;
        e.velocity = 0.0f;
        amy_add_event(&e);
    }
}

void AmyAdapter::pitchBend(uint8_t channel, int16_t value) {
    if (channel == 9) return; // Drum channel does not respond to pitch bend
    amy_event e = amy_default_event();
    e.synth = 1;
    // MidiParser passes value centered at 0 (-8192..+8191). Range: +/- 2 semitones (+/- 1/6 octave)
    e.pitch_bend = ((float)value) / (6.0f * 8192.0f);
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
    mono_stack_size_ = 0;

    // Release sustain pedal for Synth 1 and Synth 10
    amy_event ep = amy_default_event();
    ep.synth = 1;
    ep.pedal = 0;
    amy_add_event(&ep);
    ep.synth = 10;
    amy_add_event(&ep);

    // Release all active instrument voices in AMY
    amy_event e = amy_default_event();
    e.synth = 1;
    e.velocity = 0.0f;
    amy_add_event(&e);

    e.synth = 10;
    amy_add_event(&e);

    // Immediately mute any sounding voices
    all_notes_off();
}

void AmyAdapter::panic() {
    allNotesOff();
    // Center pitch bend back to 0 on both main synth and drums
    pitchBend(0, 0);
    pitchBend(9, 0);
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
        float gain = master_gain_.load(std::memory_order_relaxed);
        size_t total_samples = AMY_BLOCK_SIZE * AMY_NCHANS;

        // Apply fast fixed-point master gain only if non-unity
        // (Soft-clipping is handled natively by AMY's lookup table in internal DRAM)
        if (gain != 1.0f) {
            int32_t q8_gain = static_cast<int32_t>(gain * 256.0f);
            for (size_t i = 0; i < total_samples; ++i) {
                int32_t s = (static_cast<int32_t>(buf[i]) * q8_gain) >> 8;
                if (s > 32767) s = 32767;
                else if (s < -32768) s = -32768;
                buf[i] = static_cast<int16_t>(s);
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
    e.filter_freq_coefs[COEF_VEL] = 1.5f; // Dynamic velocity tracking: harder velocity opens filter
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

void AmyAdapter::setPortamento(uint8_t synth_id, uint16_t portamento_ms) {
    amy_event e = amy_default_event();
    e.synth = (synth_id == 0) ? 1 : synth_id;
    e.portamento_ms = portamento_ms;
    amy_add_event(&e);
}

void AmyAdapter::loadPreset(uint8_t synth_id, uint16_t preset_id, uint8_t num_voices) {
    allNotesOff();
    mono_mode_ = (num_voices == 1);
    mono_stack_size_ = 0;
    amy_event e = amy_default_event();
    e.synth = (synth_id == 0) ? 1 : synth_id;
    e.patch_number = preset_id;
    e.num_voices = (num_voices > 0) ? num_voices : 8;
    e.bus = 0; // Synth bus
    e.synth_delay_ms = 4; // 4ms smooth micro-fade on voice stealing
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
    e.bus = 0; // Target Synth bus 0 only
    e.chorus_level = level;
    e.chorus_depth = depth;
    e.chorus_lfo_freq = rate;
    amy_add_event(&e);
}

void AmyAdapter::setReverb(float room_size, float damp, float mix) {
    amy_event e = amy_default_event();
    e.bus = 0; // Target Synth bus 0 only
    e.reverb_level = mix;
    e.reverb_liveness = room_size;
    e.reverb_damping = damp;
    amy_add_event(&e);
}

void AmyAdapter::setDelay(float delay_ms, float feedback, float mix) {
    amy_event e = amy_default_event();
    e.bus = 0; // Target Synth bus 0 only
    e.echo_level = mix;
    e.echo_delay_ms = delay_ms;
    e.echo_feedback = feedback;
    amy_add_event(&e);
}

void AmyAdapter::setSendLevels(uint8_t synth_id, float reverb_send, float chorus_send, float echo_send) {
    // Optional per-synth send levels
}

} // namespace smk
