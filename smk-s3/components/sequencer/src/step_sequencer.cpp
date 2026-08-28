#include "step_sequencer.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <algorithm>
#include <cstring>
#include <cmath>

static const char* TAG = "StepSequencer";

namespace smk {

StepSequencer::StepSequencer() {
    // Default drum track names
    snprintf(track_names_[0], sizeof(track_names_[0]), "BD");
    snprintf(track_names_[1], sizeof(track_names_[1]), "SD");
    snprintf(track_names_[2], sizeof(track_names_[2]), "CH");
    snprintf(track_names_[3], sizeof(track_names_[3]), "OH");

    track_notes_[0] = 36; // Kick
    track_notes_[1] = 38; // Snare
    track_notes_[2] = 42; // Closed Hat
    track_notes_[3] = 46; // Open Hat

    track_channels_.fill(9); // MIDI Channel 10 (0-indexed = 9)

    reset();
}

void StepSequencer::reset() {
    state_ = SequencerState::Stopped;
    current_step_ = 0;
    current_pattern_ = 0;
    selected_track_ = 0;
    step_page_ = 0;
    swing_percent_ = 0.0f;
    track_mutes_.fill(false);
    track_solos_.fill(false);
    last_played_notes_.fill(-1);
    note_off_ticks_.fill(0);

    chain_.enabled = false;
    chain_.length = 0;
    chain_.current_index = 0;
    chain_.loop = true;

    for (size_t p = 0; p < kMaxPatterns; ++p) {
        for (size_t t = 0; t < kMaxTracks; ++t) {
            for (size_t i = 0; i < kMaxSteps; ++i) {
                patterns_[p][t][i].note = track_notes_[t];
                patterns_[p][t][i].velocity = 100;
                patterns_[p][t][i].gate_percent = 50;
                patterns_[p][t][i].probability = 100;
                patterns_[p][t][i].ratchet = 1;
                patterns_[p][t][i].slide = false;
                patterns_[p][t][i].has_plock = false;
                patterns_[p][t][i].locked_param = 0xFF;
                patterns_[p][t][i].locked_val = 0.0f;

                // Classic groove defaults for Pattern 0
                if (p == 0) {
                    if (t == 0) patterns_[p][t][i].active = (i == 0 || i == 4 || i == 8 || i == 12 || i == 10); // Kick
                    else if (t == 1) patterns_[p][t][i].active = (i == 4 || i == 12); // Snare on 2 & 4
                    else if (t == 2) patterns_[p][t][i].active = (i % 2 == 0); // 8th note Closed Hat
                    else if (t == 3) patterns_[p][t][i].active = (i == 2 || i == 6 || i == 10 || i == 14); // Open Hat
                } else {
                    patterns_[p][t][i].active = false;
                }
            }
        }
    }
}

void StepSequencer::selectPattern(uint8_t pattern_idx) {
    if (pattern_idx < kMaxPatterns) {
        current_pattern_ = pattern_idx;
        ESP_LOGI(TAG, "Switched to Sequencer Pattern %d", current_pattern_);
    }
}

void StepSequencer::clearPattern(uint8_t pattern_idx) {
    if (pattern_idx >= kMaxPatterns) return;
    for (size_t t = 0; t < kMaxTracks; ++t) {
        for (size_t i = 0; i < kMaxSteps; ++i) {
            patterns_[pattern_idx][t][i].active = false;
            patterns_[pattern_idx][t][i].has_plock = false;
        }
    }
}

void StepSequencer::clearTrack(uint8_t track_idx) {
    if (track_idx >= kMaxTracks) return;
    for (size_t i = 0; i < kMaxSteps; ++i) {
        patterns_[current_pattern_][track_idx][i].active = false;
        patterns_[current_pattern_][track_idx][i].has_plock = false;
    }
}

void StepSequencer::setStep(uint8_t track_idx, uint8_t step_idx, uint8_t note, uint8_t velocity, bool active, bool slide) {
    if (track_idx >= kMaxTracks || step_idx >= kMaxSteps) return;
    patterns_[current_pattern_][track_idx][step_idx].note = note;
    patterns_[current_pattern_][track_idx][step_idx].velocity = velocity;
    patterns_[current_pattern_][track_idx][step_idx].active = active;
    patterns_[current_pattern_][track_idx][step_idx].slide = slide;
}

void StepSequencer::setStep(uint8_t step_idx, uint8_t note, uint8_t velocity, bool active, bool slide) {
    setStep(selected_track_, step_idx, note, velocity, active, slide);
}

void StepSequencer::toggleStep(uint8_t track_idx, uint8_t step_idx, uint8_t velocity) {
    if (track_idx >= kMaxTracks || step_idx >= kMaxSteps) return;
    auto& s = patterns_[current_pattern_][track_idx][step_idx];
    s.active = !s.active;
    if (s.active) {
        s.velocity = velocity > 0 ? velocity : 100;
        s.note = track_notes_[track_idx];
    }
}

uint16_t StepSequencer::getTrackStepMask(uint8_t track_idx) const {
    if (track_idx >= kMaxTracks) return 0;
    uint16_t mask = 0;
    for (size_t i = 0; i < kMaxSteps; ++i) {
        if (patterns_[current_pattern_][track_idx][i].active) {
            mask |= (1 << i);
        }
    }
    return mask;
}

uint16_t StepSequencer::getTrackPlockMask(uint8_t track_idx) const {
    if (track_idx >= kMaxTracks) return 0;
    uint16_t mask = 0;
    for (size_t i = 0; i < kMaxSteps; ++i) {
        if (patterns_[current_pattern_][track_idx][i].has_plock) {
            mask |= (1 << i);
        }
    }
    return mask;
}

void StepSequencer::setStepParamLock(uint8_t track_idx, uint8_t step_idx, uint8_t param_id, float value) {
    if (track_idx >= kMaxTracks || step_idx >= kMaxSteps) return;
    auto& s = patterns_[current_pattern_][track_idx][step_idx];
    s.has_plock = true;
    s.locked_param = param_id;
    s.locked_val = value;
    ESP_LOGD(TAG, "P-Lock set: Trk=%u Step=%u Param=%u Val=%.1f", track_idx, step_idx, param_id, value);
}

void StepSequencer::setStepParamLock(uint8_t step_idx, uint8_t param_id, float value) {
    setStepParamLock(selected_track_, step_idx, param_id, value);
}

void StepSequencer::clearStepParamLock(uint8_t track_idx, uint8_t step_idx) {
    if (track_idx >= kMaxTracks || step_idx >= kMaxSteps) return;
    auto& s = patterns_[current_pattern_][track_idx][step_idx];
    s.has_plock = false;
    s.locked_param = 0xFF;
    s.locked_val = 0.0f;
}

bool StepSequencer::hasStepParamLock(uint8_t track_idx, uint8_t step_idx) const {
    if (track_idx >= kMaxTracks || step_idx >= kMaxSteps) return false;
    return patterns_[current_pattern_][track_idx][step_idx].has_plock;
}

void StepSequencer::recordLiveMotion(uint8_t param_id, float value) {
    if (state_ == SequencerState::Recording) {
        setStepParamLock(selected_track_, current_step_, param_id, value);
    }
}

void StepSequencer::setPatternChain(const uint8_t* pattern_list, uint8_t length, bool loop) {
    if (!pattern_list || length == 0) {
        clearPatternChain();
        return;
    }
    chain_.length = std::min(length, static_cast<uint8_t>(kMaxChainLength));
    for (uint8_t i = 0; i < chain_.length; ++i) {
        chain_.patterns[i] = pattern_list[i] % kMaxPatterns;
    }
    chain_.current_index = 0;
    chain_.loop = loop;
    chain_.enabled = true;

    selectPattern(chain_.patterns[0]);
    ESP_LOGI(TAG, "Pattern Chain configured with %u patterns", chain_.length);
}

void StepSequencer::clearPatternChain() {
    chain_.enabled = false;
    chain_.length = 0;
    chain_.current_index = 0;
    ESP_LOGI(TAG, "Pattern Chain cleared");
}

void StepSequencer::togglePatternChain() {
    if (chain_.length > 0) {
        chain_.enabled = !chain_.enabled;
        ESP_LOGI(TAG, "Pattern Chain toggled: %s", chain_.enabled ? "ENABLED" : "DISABLED");
    }
}

const StepData& StepSequencer::step(uint8_t track_idx, uint8_t step_idx) const {
    if (track_idx >= kMaxTracks || step_idx >= kMaxSteps) return patterns_[current_pattern_][0][0];
    return patterns_[current_pattern_][track_idx][step_idx];
}

StepData& StepSequencer::step(uint8_t track_idx, uint8_t step_idx) {
    if (track_idx >= kMaxTracks || step_idx >= kMaxSteps) return patterns_[current_pattern_][0][0];
    return patterns_[current_pattern_][track_idx][step_idx];
}

const StepData& StepSequencer::step(uint8_t step_idx) const {
    return step(selected_track_, step_idx);
}

StepData& StepSequencer::step(uint8_t step_idx) {
    return step(selected_track_, step_idx);
}

void StepSequencer::setTrackMute(uint8_t track_idx, bool mute) {
    if (track_idx < kMaxTracks) track_mutes_[track_idx] = mute;
}

bool StepSequencer::isTrackMuted(uint8_t track_idx) const {
    if (track_idx < kMaxTracks) return track_mutes_[track_idx];
    return false;
}

void StepSequencer::toggleTrackMute(uint8_t track_idx) {
    if (track_idx < kMaxTracks) track_mutes_[track_idx] = !track_mutes_[track_idx];
}

void StepSequencer::setTrackSolo(uint8_t track_idx, bool solo) {
    if (track_idx < kMaxTracks) track_solos_[track_idx] = solo;
}

bool StepSequencer::isTrackSolo(uint8_t track_idx) const {
    if (track_idx < kMaxTracks) return track_solos_[track_idx];
    return false;
}

void StepSequencer::toggleTrackSolo(uint8_t track_idx) {
    if (track_idx < kMaxTracks) track_solos_[track_idx] = !track_solos_[track_idx];
}

bool StepSequencer::isAnyTrackSolo() const {
    for (size_t t = 0; t < kMaxTracks; ++t) {
        if (track_solos_[t]) return true;
    }
    return false;
}

const char* StepSequencer::trackName(uint8_t track_idx) const {
    if (track_idx < kMaxTracks) return track_names_[track_idx];
    return "";
}

uint8_t StepSequencer::trackNote(uint8_t track_idx) const {
    if (track_idx < kMaxTracks) return track_notes_[track_idx];
    return 36;
}

uint8_t StepSequencer::trackChannel(uint8_t track_idx) const {
    if (track_idx < kMaxTracks) return track_channels_[track_idx];
    return 9;
}

void StepSequencer::setTrackMetadata(uint8_t track_idx, const char* name, uint8_t default_note, uint8_t channel) {
    if (track_idx >= kMaxTracks) return;
    if (name) snprintf(track_names_[track_idx], sizeof(track_names_[track_idx]), "%s", name);
    track_notes_[track_idx] = default_note;
    track_channels_[track_idx] = channel;
}

void StepSequencer::recordLiveTrackHit(uint8_t track_idx, uint8_t velocity) {
    if (track_idx >= kMaxTracks) return;
    uint8_t s_idx = current_step_;
    patterns_[current_pattern_][track_idx][s_idx].active = true;
    patterns_[current_pattern_][track_idx][s_idx].velocity = velocity > 0 ? velocity : 100;
    patterns_[current_pattern_][track_idx][s_idx].note = track_notes_[track_idx];
}

void StepSequencer::recordLiveHit(uint8_t note, uint8_t velocity) {
    int match_track = -1;
    for (size_t t = 0; t < kMaxTracks; ++t) {
        if (track_notes_[t] == note) {
            match_track = static_cast<int>(t);
            break;
        }
    }
    if (match_track >= 0) {
        recordLiveTrackHit(static_cast<uint8_t>(match_track), velocity);
    } else {
        recordLiveTrackHit(selected_track_, velocity);
    }
}

void StepSequencer::play() {
    state_ = SequencerState::Playing;
    current_step_ = 0;
    last_played_notes_.fill(-1);
    ESP_LOGI(TAG, "StepSequencer PLAY");
}

void StepSequencer::stop() {
    state_ = SequencerState::Stopped;
    current_step_ = 0;
    last_played_notes_.fill(-1);
    ESP_LOGI(TAG, "StepSequencer STOP");
}

void StepSequencer::record() {
    if (state_ == SequencerState::Recording) {
        state_ = SequencerState::Playing;
    } else {
        state_ = SequencerState::Recording;
    }
    ESP_LOGI(TAG, "StepSequencer STATE: %d", static_cast<int>(state_));
}

void StepSequencer::processTick(uint32_t tick_count, EventBus& event_bus) {
    if (state_ == SequencerState::Stopped) return;

    constexpr uint32_t ticks_per_step = 6;
    bool any_solo = isAnyTrackSolo();

    // Determine if this tick is a step boundary (with swing)
    bool is_step_tick = false;
    if (swing_percent_ <= 50.0f) {
        is_step_tick = ((tick_count % ticks_per_step) == 0);
    } else {
        uint32_t tick_split = (uint32_t)std::round(12.0f * (swing_percent_ / 100.0f));
        if (tick_split < 6) tick_split = 6;
        if (tick_split > 9) tick_split = 9;
        uint32_t mod12 = tick_count % 12;
        is_step_tick = (mod12 == 0) || (mod12 == tick_split);
    }

    for (size_t t = 0; t < kMaxTracks; ++t) {
        if (last_played_notes_[t] >= 0 && tick_count >= note_off_ticks_[t]) {
            SynthEvent off_ev;
            off_ev.type = EventType::NoteOff;
            off_ev.source = EventSource::Sequencer;
            off_ev.channel = track_channels_[t];
            off_ev.id = (uint16_t)last_played_notes_[t];
            off_ev.value = 0;
            off_ev.timestamp_us = (uint32_t)esp_timer_get_time();
            event_bus.send(off_ev);

            last_played_notes_[t] = -1;
        }

        // Check Mute and Solo
        if (track_mutes_[t]) continue;
        if (any_solo && !track_solos_[t]) continue;

        if (is_step_tick) {
            uint8_t step_idx = current_step_;
            const auto& s = patterns_[current_pattern_][t][step_idx];

            if (s.active) {
                // Check probability
                if (s.probability < 100) {
                    uint8_t roll = static_cast<uint8_t>((esp_timer_get_time() % 100) + 1);
                    if (roll > s.probability) continue;
                }

                // Dispatch Parameter Lock Automation Event if present
                if (s.has_plock && s.locked_param < 8) {
                    SynthEvent plock_ev;
                    plock_ev.type = EventType::ControlChange;
                    plock_ev.source = EventSource::Sequencer;
                    plock_ev.channel = track_channels_[t];
                    plock_ev.id = static_cast<uint16_t>(s.locked_param + 1); // CC 1..8 for Macro 1..8
                    plock_ev.value = static_cast<int32_t>(s.locked_val);
                    plock_ev.timestamp_us = static_cast<uint32_t>(esp_timer_get_time());
                    event_bus.send(plock_ev);
                }

                SynthEvent on_ev;
                on_ev.type = EventType::NoteOn;
                on_ev.source = EventSource::Sequencer;
                on_ev.channel = track_channels_[t];
                on_ev.id = s.note;
                on_ev.value = s.velocity;
                on_ev.timestamp_us = (uint32_t)esp_timer_get_time();
                event_bus.send(on_ev);

                last_played_notes_[t] = s.note;
                uint32_t gate_ticks = static_cast<uint32_t>((ticks_per_step * s.gate_percent) / 100.0f);
                if (gate_ticks < 1) gate_ticks = 1;
                note_off_ticks_[t] = tick_count + gate_ticks;
            }
        }
    }

    if (is_step_tick) {
        if (current_step_ == (kMaxSteps - 1)) {
            // End of 16-step pattern reached -> Check Pattern Chain
            if (chain_.enabled && chain_.length > 1) {
                chain_.current_index = (chain_.current_index + 1) % chain_.length;
                if (!chain_.loop && chain_.current_index == 0) {
                    stop();
                    return;
                }
                current_pattern_ = chain_.patterns[chain_.current_index];
                ESP_LOGI(TAG, "Pattern Chain advanced to step %u (Pattern %u)", chain_.current_index + 1, current_pattern_);
            }
            current_step_ = 0;
        } else {
            current_step_ = (current_step_ + 1) % kMaxSteps;
        }
    }
}

} // namespace smk
