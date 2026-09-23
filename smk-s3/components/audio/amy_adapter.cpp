#include "amy_adapter.h"
#include "synth_config.h"
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

extern "C" {
#include "amy.h"
// Same optional CC hook installed by amy_default_synths().
void juno_filter_midi_handler(uint8_t* bytes, uint16_t len, uint8_t is_sysex);
void amy_platform_init() {}
void amy_platform_deinit() {}
}

namespace smk {

static const char* TAG = "AMY_ADAPTER";
static AmyAdapter* s_active_amy_adapter = nullptr;

extern "C" {
    struct FmAlgorithm { uint8_t ops[MAX_ALGO_OPS]; };
    extern const struct FmAlgorithm algorithms[33];
}

static constexpr uint8_t FM_OUT_BUS_ONE = 1 << 0;
static constexpr uint8_t FM_OUT_BUS_TWO = 1 << 1;

// NOTE: Saturation drive currently operates on Synth Bus 0 post-FX as a master bus drive.
extern "C" void smk_bus_postprocess_hook(uint8_t bus, SAMPLE *buf, uint16_t len) {
    if (bus == 0 && s_active_amy_adapter) {
        float drive = s_active_amy_adapter->drive();
        if (drive > 0.001f) {
            float k = 1.0f + drive * 3.5f;
            float comp = 1.0f / (1.0f + 0.45f * drive);
            size_t total_samples = len * AMY_NCHANS;
            for (size_t i = 0; i < total_samples; ++i) {
                float x = S2F(buf[i]);
                float driven = x * k;
                float y = (driven / (1.0f + 0.5f * std::fabs(driven))) * comp;
                buf[i] = F2S(y);
            }
        }
    }
}

// Boot-only check: AMY can register a voice even when its osc allocation failed.
// Verify each voice's ownership, not just the instrument's reported voice count.
static bool completeInstrumentAllocation(uint8_t synth_id, uint8_t expected_voices) {
    uint16_t voices[MAX_VOICES_PER_INSTRUMENT];
    const int count = instrument_get_num_voices(synth_id, voices);
    const int oscs_per_voice = instrument_get_oscs_per_voice(synth_id);
    if (count != expected_voices || oscs_per_voice <= 0) {
        ESP_LOGE(TAG, "Synth %u incomplete: voices=%d expected=%u oscs/voice=%d",
                 synth_id, count, expected_voices, oscs_per_voice);
        return false;
    }
    for (int v = 0; v < count; ++v) {
        int allocated = 0;
        for (uint16_t osc = 0; osc < AMY_OSCS; ++osc) {
            if (osc_to_voice[osc] == voices[v]) ++allocated;
        }
        if (allocated != oscs_per_voice) {
            ESP_LOGE(TAG, "Synth %u voice %u incomplete: oscs=%d expected=%d",
                     synth_id, voices[v], allocated, oscs_per_voice);
            return false;
        }
    }
    ESP_LOGI(TAG, "Synth %u ready: voices=%d oscs/voice=%d allocated=%d pool=%u",
             synth_id, count, oscs_per_voice, count * oscs_per_voice, AMY_OSCS);
    return true;
}

void AmyAdapter::endEngine() {
    if (s_active_amy_adapter == this) {
        s_active_amy_adapter = nullptr;
    }
    amy_stop();
}

bool AmyAdapter::beginEngine(uint32_t sample_rate_hz) {
    ESP_LOGI(TAG, "Initializing AMY Synth Engine (Dedicated Audio Core 1 + Dual Bus Architecture)");
    s_active_amy_adapter = this;
    
    amy_config_t config = amy_default_config();
    config.audio = AMY_AUDIO_IS_NONE;
    config.midi = AMY_MIDI_IS_NONE;
    config.platform.multicore = 0;  // Dedicated Core 1 audio task, keeping Core 0 100% free for USB MIDI
    config.platform.multithread = 0;
    // The upstream demo also creates a bleeper and six DX7 voices. With 120
    // oscs those fragment the pool before the last Juno voice can be allocated.
    config.features.default_synths = 0;
    config.amy_external_midi_input_hook = juno_filter_midi_handler;
    config.amy_external_bus_postprocess_hook = smk_bus_postprocess_hook;
    config.features.reverb = 1;
    config.features.chorus = 1;
    config.features.echo = 1;
    config.features.startup_bleep = 0;
    config.ram_caps_events = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT; // Oscillator state and normal events stay internal
    config.ram_caps_synth = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;  // High-speed SRAM for voice & osc states
    config.ram_caps_block = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;  // High-speed SRAM for rendering block buffers
    config.ram_caps_fbl = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;    // High-speed SRAM for bus mixing buffers
    config.ram_caps_delay = MALLOC_CAP_SPIRAM;    // Reverb / Chorus / Echo in PSRAM
    config.ram_caps_sample = MALLOC_CAP_SPIRAM;   // PCM drum samples in PSRAM
    config.ram_caps_sysex = MALLOC_CAP_SPIRAM;
    config.overload_threshold = 0.95f;
    config.overload_ms = 500;
    config.max_oscs = smk::config::kMaxOscillators;
    
    active_voices_.store(0);
    std::memset(active_notes_, 0, sizeof(active_notes_));

    amy_start(config);

    // Allocate the large, persistent drum block before the replaceable main
    // voices. Patch 258 supplies its one-voice container, flags and GM mappings.
    amy_event e10 = amy_default_event();
    e10.synth = 10;
    e10.patch_number = smk::config::kDrumPatchNumber;
    e10.bus = 1;
    amy_add_event(&e10);
    if (!completeInstrumentAllocation(10, 1)) return false;

    // Configure Synth 1 (Main Synth): Bus 0 with 4ms voice-stealing micro-fade
    amy_event e1 = amy_default_event();
    e1.synth = 1;
    e1.patch_number = smk::config::kDefaultPatchNumber;
    e1.num_voices = smk::config::kDefaultVoiceCount;
    e1.bus = 0;
    e1.synth_delay_ms = 4;
    amy_add_event(&e1);
    if (!completeInstrumentAllocation(1, smk::config::kDefaultVoiceCount)) return false;

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

void AmyAdapter::executeNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
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
        executeNoteOff(channel, note);
    }
}

void AmyAdapter::executeNoteOff(uint8_t channel, uint8_t note) {
    channel &= 0x0F;
    note &= 0x7F;
    if (active_notes_[channel][note] > 0) {
        active_notes_[channel][note]--;
    } else {
        return; // Note was already released or cleared by Panic / AllNotesOff
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

void AmyAdapter::executePitchBend(uint8_t channel, int16_t value) {
    if (channel == 9) return; // Drum channel does not respond to pitch bend
    amy_event e = amy_default_event();
    e.synth = 1;
    // MidiParser passes value centered at 0 (-8192..+8191). Range: +/- 2 semitones (+/- 1/6 octave)
    e.pitch_bend = ((float)value) / (6.0f * 8192.0f);
    amy_add_event(&e);
}

void AmyAdapter::executeControlChange(uint8_t channel, uint8_t controller, uint8_t value) {
    uint8_t target_synth = (channel == 9) ? 10 : 1;
    if (controller == 64) { // Sustain Pedal
        amy_event e = amy_default_event();
        e.synth = target_synth;
        e.pedal = value;
        amy_add_event(&e);
    } else if (controller == 120 || controller == 123) { // All Sound Off / All Notes Off
        executeAllNotesOff();
    } else {
        uint8_t msg[3] = { (uint8_t)(0xB0 | (channel & 0x0F)), controller, value };
        convert_midi_bytes_to_messages(msg, 3, 0);
    }
}

void AmyAdapter::executeAllNotesOff() {
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

void AmyAdapter::executePanic() {
    // Exclusive owner: cancel future deltas as well as currently sounding
    // voices. Otherwise an internally scheduled Note On can revive after Panic.
    amy_deltas_reset();
    executeAllNotesOff();
    // Center pitch bend back to 0 on both main synth and drums
    executePitchBend(0, 0);
    executePitchBend(9, 0);
}

void AmyAdapter::setMasterGain(float gain) {
    if (gain < 0.0f) gain = 0.0f;
    if (gain > 2.0f) gain = 2.0f;
    master_gain_.store(gain, std::memory_order_relaxed);
}

int16_t* AmyAdapter::renderEngine() {
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
        for (size_t i = 0; i < copy_samples; ++i) {
            scope_buffer_[i].store(buf[i], std::memory_order_relaxed);
        }
    }
    render_load_snapshot_.store(amy_get_render_load(), std::memory_order_relaxed);
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
    return render_load_snapshot_.load(std::memory_order_relaxed);
}

uint32_t AmyAdapter::activeVoices() const {
    return active_voices_.load();
}

void AmyAdapter::executeFilter(uint8_t synth_id, float cutoff_hz, float resonance,
                               float env_amount, float key_tracking, float vel_tracking, uint8_t filter_type) {
    amy_event e = amy_default_event();
    e.synth = (synth_id == 0) ? 1 : synth_id;
    e.filter_freq_coefs[COEF_CONST] = cutoff_hz;
    e.filter_freq_coefs[COEF_NOTE] = key_tracking;
    e.filter_freq_coefs[COEF_EG0] = env_amount;
    e.filter_freq_coefs[COEF_VEL] = vel_tracking;
    e.resonance = resonance;
    if (filter_type != 0xFF) {
        e.filter_type = filter_type;
    }
    amy_add_event(&e);
}

void AmyAdapter::executeWaveform(uint8_t synth_id, uint8_t wave_type) {
    amy_event e = amy_default_event();
    e.synth = (synth_id == 0) ? 1 : synth_id;
    e.wave = wave_type;
    amy_add_event(&e);
}

void AmyAdapter::executeEnvelope(uint8_t synth_id, float attack_ms, float decay_ms, float sustain_level, float release_ms) {
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

void AmyAdapter::executePortamento(uint8_t synth_id, uint16_t portamento_ms) {
    amy_event e = amy_default_event();
    e.synth = (synth_id == 0) ? 1 : synth_id;
    e.portamento_ms = portamento_ms;
    amy_add_event(&e);
}

void AmyAdapter::executePreset(uint8_t synth_id, uint16_t preset_id, uint8_t num_voices) {
    executeAllNotesOff();
    mono_mode_ = (num_voices == 1);
    mono_stack_size_ = 0;
    amy_event e = amy_default_event();
    e.synth = (synth_id == 0) ? 1 : synth_id;
    e.patch_number = preset_id;
    e.num_voices = (num_voices > 0) ? num_voices : 8;
    e.bus = 0; // Synth bus
    e.synth_delay_ms = 4; // 4ms smooth micro-fade on voice stealing
    amy_add_event(&e);
    captureFmBaseState(e.synth);
}

void AmyAdapter::captureFmBaseState(uint8_t synth_id) {
    uint8_t target = (synth_id == 0) ? 1 : synth_id;
    if (target != 1) return;
    fm_snapshot_valid_ = false;
    uint16_t voices[MAX_VOICES_PER_INSTRUMENT];
    int num_v = instrument_get_num_voices(target, voices);
    if (num_v <= 0) return;

    for (int v = 0; v < num_v && v < (int)kMaxVoicesSnapshot; ++v) {
        uint16_t base = voice_to_base_osc[voices[v]];
        if (!AMY_IS_SET(base)) continue;
        if (synth[base]->wave == ALGO) {
            uint8_t algo_id = synth[base]->algorithm;
            if (algo_id < 1 || algo_id > 32) algo_id = 1;
            const auto& algo = algorithms[algo_id];
            for (uint8_t op = 0; op < MAX_ALGO_OPS && op < (int)kMaxOpsSnapshot; ++op) {
                auto& snap = fm_base_ops_[v][op];
                snap.valid = false;
                snap.is_modulator = (algo.ops[op] & (FM_OUT_BUS_ONE | FM_OUT_BUS_TWO)) != 0;
                uint16_t op_osc = synth[base]->algo_source[op];
                if (AMY_IS_SET(op_osc)) {
                    snap.base_level = synth[op_osc]->amp_coefs[COEF_CONST];
                    snap.base_logratio = synth[op_osc]->logratio;
                    snap.base_logfreq = synth[op_osc]->logfreq_coefs[COEF_CONST];
                    snap.valid = true;
                }
            }
            fm_snapshot_valid_ = true;
        } else if (AMY_IS_SET(synth[base]->mod_source)) {
            uint16_t mod_osc = synth[base]->mod_source;
            auto& snap = fm_base_mod_source_[v];
            snap.valid = true;
            snap.is_modulator = true;
            snap.base_level = synth[mod_osc]->amp_coefs[COEF_CONST];
            snap.base_logratio = synth[mod_osc]->logratio;
            snap.base_logfreq = synth[mod_osc]->logfreq_coefs[COEF_CONST];
            fm_snapshot_valid_ = true;
        }
    }
}

void AmyAdapter::executeMessage(const char* message) {
    if (message) amy_play_message((char*)message);
}

void AmyAdapter::executeFmModIndex(uint8_t synth_id, float factor) {
    uint8_t target = (synth_id == 0) ? 1 : synth_id;
    uint16_t voices[MAX_VOICES_PER_INSTRUMENT];
    int num_v = instrument_get_num_voices(target, voices);
    if (!fm_snapshot_valid_) {
        for (int v = 0; v < num_v; ++v) {
            uint16_t base = voice_to_base_osc[voices[v]];
            if (AMY_IS_SET(base) && synth[base]->wave == ALGO) {
                uint8_t algo_id = synth[base]->algorithm;
                if (algo_id < 1 || algo_id > 32) algo_id = 1;
                const auto& algo = algorithms[algo_id];
                for (uint8_t op = 0; op < MAX_ALGO_OPS; ++op) {
                    bool is_modulator = (algo.ops[op] & (FM_OUT_BUS_ONE | FM_OUT_BUS_TWO)) != 0;
                    if (is_modulator) {
                        uint16_t op_osc = synth[base]->algo_source[op];
                        if (AMY_IS_SET(op_osc)) {
                            synth[op_osc]->amp_coefs[COEF_CONST] = factor;
                        }
                    }
                }
            } else if (AMY_IS_SET(base) && AMY_IS_SET(synth[base]->mod_source)) {
                synth[synth[base]->mod_source]->amp_coefs[COEF_CONST] = factor;
            }
        }
        return;
    }

    factor = std::clamp(factor, 0.0f, 8.0f);
    for (int v = 0; v < num_v && v < (int)kMaxVoicesSnapshot; ++v) {
        uint16_t base = voice_to_base_osc[voices[v]];
        if (!AMY_IS_SET(base)) continue;
        if (synth[base]->wave == ALGO) {
            for (uint8_t op = 0; op < MAX_ALGO_OPS && op < (int)kMaxOpsSnapshot; ++op) {
                const auto& snap = fm_base_ops_[v][op];
                if (snap.valid && snap.is_modulator) {
                    uint16_t op_osc = synth[base]->algo_source[op];
                    if (AMY_IS_SET(op_osc)) {
                        synth[op_osc]->amp_coefs[COEF_CONST] = std::clamp(snap.base_level * factor, 0.0f, 16.0f);
                    }
                }
            }
        } else if (AMY_IS_SET(synth[base]->mod_source)) {
            const auto& snap = fm_base_mod_source_[v];
            if (snap.valid) {
                synth[synth[base]->mod_source]->amp_coefs[COEF_CONST] = std::clamp(snap.base_level * factor, 0.0f, 16.0f);
            }
        }
    }
}

void AmyAdapter::executeFmFeedback(uint8_t synth_id, float feedback) {
    uint8_t target = (synth_id == 0) ? 1 : synth_id;
    uint16_t voices[MAX_VOICES_PER_INSTRUMENT];
    int num_v = instrument_get_num_voices(target, voices);
    for (int v = 0; v < num_v; ++v) {
        uint16_t base = voice_to_base_osc[voices[v]];
        if (AMY_IS_SET(base)) {
            synth[base]->feedback = feedback;
        }
    }
}

void AmyAdapter::executeFmRatio(uint8_t synth_id, float ratio_factor) {
    if (ratio_factor <= 0.001f) ratio_factor = 0.001f;
    uint8_t target = (synth_id == 0) ? 1 : synth_id;
    uint16_t voices[MAX_VOICES_PER_INSTRUMENT];
    int num_v = instrument_get_num_voices(target, voices);
    if (!fm_snapshot_valid_) {
        for (int v = 0; v < num_v; ++v) {
            uint16_t base = voice_to_base_osc[voices[v]];
            if (AMY_IS_SET(base) && synth[base]->wave == ALGO) {
                uint8_t algo_id = synth[base]->algorithm;
                if (algo_id < 1 || algo_id > 32) algo_id = 1;
                const auto& algo = algorithms[algo_id];
                for (uint8_t op = 0; op < MAX_ALGO_OPS; ++op) {
                    bool is_modulator = (algo.ops[op] & (FM_OUT_BUS_ONE | FM_OUT_BUS_TWO)) != 0;
                    if (is_modulator) {
                        uint16_t op_osc = synth[base]->algo_source[op];
                        if (AMY_IS_SET(op_osc)) {
                            synth[op_osc]->logratio = log2f(ratio_factor);
                        }
                    }
                }
            } else if (AMY_IS_SET(base) && AMY_IS_SET(synth[base]->mod_source)) {
                synth[synth[base]->mod_source]->logratio = log2f(ratio_factor);
            }
        }
        return;
    }

    float log_mult = log2f(ratio_factor);
    for (int v = 0; v < num_v && v < (int)kMaxVoicesSnapshot; ++v) {
        uint16_t base = voice_to_base_osc[voices[v]];
        if (!AMY_IS_SET(base)) continue;
        if (synth[base]->wave == ALGO) {
            for (uint8_t op = 0; op < MAX_ALGO_OPS && op < (int)kMaxOpsSnapshot; ++op) {
                const auto& snap = fm_base_ops_[v][op];
                if (snap.valid && snap.is_modulator) {
                    uint16_t op_osc = synth[base]->algo_source[op];
                    if (AMY_IS_SET(op_osc)) {
                        synth[op_osc]->logratio = snap.base_logratio + log_mult;
                    }
                }
            }
        } else if (AMY_IS_SET(synth[base]->mod_source)) {
            const auto& snap = fm_base_mod_source_[v];
            if (snap.valid) {
                synth[synth[base]->mod_source]->logratio = snap.base_logratio + log_mult;
            }
        }
    }
}

void AmyAdapter::executeFmAlgorithm(uint8_t synth_id, uint8_t algo_id) {
    if (algo_id < 1 || algo_id > 32) return;
    uint8_t target = (synth_id == 0) ? 1 : synth_id;
    uint16_t voices[MAX_VOICES_PER_INSTRUMENT];
    int num_v = instrument_get_num_voices(target, voices);
    for (int v = 0; v < num_v; ++v) {
        uint16_t base = voice_to_base_osc[voices[v]];
        if (AMY_IS_SET(base)) {
            synth[base]->algorithm = algo_id;
        }
    }
}

void AmyAdapter::executeChorus(float depth, float rate, float level) {
    amy_event e = amy_default_event();
    e.bus = 0; // Target Synth bus 0 only
    e.chorus_level = level;
    e.chorus_depth = depth;
    e.chorus_lfo_freq = rate;
    amy_add_event(&e);
}

void AmyAdapter::executeChorusMode(uint8_t mode) {
    switch (mode) {
        case 0: executeChorus(0.0f, 0.0f, 0.0f); break;
        case 1: executeChorus(0.5f, 0.5f, 0.7f); break; // Classic
        case 2: executeChorus(0.8f, 0.6f, 0.85f); break; // Juno
        case 3: executeChorus(1.2f, 0.9f, 1.0f); break; // Ensemble
        case 4: executeChorus(1.5f, 0.4f, 0.9f); break; // Wide
        case 5: executeChorus(0.4f, 4.5f, 0.6f); break; // Vibrato
        default: break;
    }
}

void AmyAdapter::executeReverb(float room_size, float damp, float mix) {
    amy_event e = amy_default_event();
    e.bus = 0; // Target Synth bus 0 only
    e.reverb_level = mix;
    e.reverb_liveness = room_size;
    e.reverb_damping = damp;
    amy_add_event(&e);
}

void AmyAdapter::executeReverbFreeze(bool freeze) {
    reverb_freeze_.store(freeze, std::memory_order_relaxed);
    config_reverb_freeze(0, freeze ? 1 : 0);
}

void AmyAdapter::executeDelay(float delay_ms, float feedback, float mix) {
    amy_event e = amy_default_event();
    e.bus = 0; // Target Synth bus 0 only
    e.echo_level = mix;
    e.echo_delay_ms = delay_ms;
    e.echo_feedback = feedback;
    amy_add_event(&e);
}

void AmyAdapter::executeDrive(float drive) {
    drive_level_.store(std::clamp(drive, 0.0f, 1.0f), std::memory_order_relaxed);
}

void AmyAdapter::executeMasterTone(float tone) {
    tone = std::clamp(tone, -1.0f, 1.0f);
    master_tone_.store(tone, std::memory_order_relaxed);
    float eq_l = 1.0f;
    float eq_h = 1.0f;
    if (tone >= 0.0f) {
        eq_l = 1.0f - 0.4f * tone;
        eq_h = 1.0f + 0.6f * tone;
    } else {
        eq_l = 1.0f - 0.6f * tone;
        eq_h = 1.0f + 0.4f * tone;
    }
    amy_event e = amy_default_event();
    e.bus = 0;
    e.eq_l = eq_l;
    e.eq_h = eq_h;
    amy_add_event(&e);
}

void AmyAdapter::executeOscDetune(uint8_t synth_id, float cents) {
    uint8_t target = (synth_id == 0) ? 1 : synth_id;
    uint16_t voices[MAX_VOICES_PER_INSTRUMENT];
    int num_v = instrument_get_num_voices(target, voices);
    float detune_offset = cents / 1200.0f;
    for (int v = 0; v < num_v; ++v) {
        uint16_t base = voice_to_base_osc[voices[v]];
        if (!AMY_IS_SET(base)) continue;
        if (synth[base]->wave == ALGO) {
            uint8_t algo_id = synth[base]->algorithm;
            if (algo_id < 1 || algo_id > 32) algo_id = 1;
            const auto& algo = algorithms[algo_id];
            for (uint8_t op = 0; op < MAX_ALGO_OPS; ++op) {
                bool is_modulator = (algo.ops[op] & (FM_OUT_BUS_ONE | FM_OUT_BUS_TWO)) != 0;
                if (is_modulator) {
                    uint16_t op_osc = synth[base]->algo_source[op];
                    if (AMY_IS_SET(op_osc)) {
                        float base_lf = (fm_snapshot_valid_ && v < (int)kMaxVoicesSnapshot && op < (int)kMaxOpsSnapshot && fm_base_ops_[v][op].valid)
                                            ? fm_base_ops_[v][op].base_logfreq
                                            : 0.0f;
                        synth[op_osc]->logfreq_coefs[COEF_CONST] = base_lf + detune_offset;
                    }
                }
            }
        } else {
            int oscs_per_v = instrument_get_oscs_per_voice(target);
            if (oscs_per_v >= 2) {
                synth[base + 1]->logfreq_coefs[COEF_CONST] = detune_offset;
            }
        }
    }
}

void AmyAdapter::executeSubOscLevel(uint8_t synth_id, float level) {
    uint8_t target = (synth_id == 0) ? 1 : synth_id;
    uint16_t voices[MAX_VOICES_PER_INSTRUMENT];
    int num_v = instrument_get_num_voices(target, voices);
    int oscs_per_v = instrument_get_oscs_per_voice(target);
    if (oscs_per_v >= 3) {
        for (int v = 0; v < num_v; ++v) {
            uint16_t base = voice_to_base_osc[voices[v]];
            if (AMY_IS_SET(base)) {
                synth[base + 2]->amp_coefs[COEF_CONST] = level;
            }
        }
    }
}

void AmyAdapter::executeNoiseLevel(uint8_t synth_id, float level) {
    uint8_t target = (synth_id == 0) ? 1 : synth_id;
    uint16_t voices[MAX_VOICES_PER_INSTRUMENT];
    int num_v = instrument_get_num_voices(target, voices);
    int oscs_per_v = instrument_get_oscs_per_voice(target);
    if (oscs_per_v >= 4) {
        for (int v = 0; v < num_v; ++v) {
            uint16_t base = voice_to_base_osc[voices[v]];
            if (AMY_IS_SET(base)) {
                synth[base + 3]->amp_coefs[COEF_CONST] = level;
            }
        }
    }
}

void AmyAdapter::executeOscMix(uint8_t synth_id, float mix) {
    mix = std::clamp(mix, 0.0f, 1.0f);
    uint8_t target = (synth_id == 0) ? 1 : synth_id;
    uint16_t voices[MAX_VOICES_PER_INSTRUMENT];
    int num_v = instrument_get_num_voices(target, voices);
    int oscs_per_v = instrument_get_oscs_per_voice(target);
    if (oscs_per_v >= 2) {
        for (int v = 0; v < num_v; ++v) {
            uint16_t base = voice_to_base_osc[voices[v]];
            if (AMY_IS_SET(base)) {
                synth[base]->amp_coefs[COEF_CONST] = 1.0f - mix;
                synth[base + 1]->amp_coefs[COEF_CONST] = mix;
            }
        }
    }
}

void AmyAdapter::setSendLevels(uint8_t synth_id, float reverb_send, float chorus_send, float echo_send) {
    // Optional per-synth send levels
}

} // namespace smk
