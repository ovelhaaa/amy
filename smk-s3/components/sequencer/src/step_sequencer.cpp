#include "step_sequencer.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <algorithm>

static const char* TAG = "StepSequencer";

namespace smk {

StepSequencer::StepSequencer() {
    reset();
}

void StepSequencer::reset() {
    state_ = SequencerState::Stopped;
    current_step_ = 0;
    current_pattern_ = 0;
    swing_percent_ = 0.0f;
    last_played_notes_.fill(-1);
    note_off_ticks_.fill(0);

    for (size_t p = 0; p < kMaxPatterns; ++p) {
        for (size_t t = 0; t < kMaxTracks; ++t) {
            for (size_t i = 0; i < kMaxSteps; ++i) {
                patterns_[p][t][i].note = (t == 2) ? (36 + (i % 8)) : (60 + ((i + p * 2) % 12));
                patterns_[p][t][i].velocity = 90;
                patterns_[p][t][i].gate_percent = 50;
                patterns_[p][t][i].probability = 100;
                patterns_[p][t][i].ratchet = 1;
                patterns_[p][t][i].active = (i % 2 == 0);
                patterns_[p][t][i].slide = false;
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

void StepSequencer::setStep(uint8_t track_idx, uint8_t step_idx, uint8_t note, uint8_t velocity, bool active, bool slide) {
    if (track_idx >= kMaxTracks || step_idx >= kMaxSteps) return;
    patterns_[current_pattern_][track_idx][step_idx].note = note;
    patterns_[current_pattern_][track_idx][step_idx].velocity = velocity;
    patterns_[current_pattern_][track_idx][step_idx].active = active;
    patterns_[current_pattern_][track_idx][step_idx].slide = slide;
}

void StepSequencer::setStep(uint8_t step_idx, uint8_t note, uint8_t velocity, bool active, bool slide) {
    setStep(0, step_idx, note, velocity, active, slide);
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
    return step(0, step_idx);
}

StepData& StepSequencer::step(uint8_t step_idx) {
    return step(0, step_idx);
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
    state_ = SequencerState::Recording;
    current_step_ = 0;
    ESP_LOGI(TAG, "StepSequencer RECORD");
}

void StepSequencer::processTick(uint32_t tick_count, EventBus& event_bus) {
    if (state_ == SequencerState::Stopped) return;

    constexpr uint32_t ticks_per_step = 6;

    for (size_t t = 0; t < kMaxTracks; ++t) {
        if (last_played_notes_[t] >= 0 && tick_count >= note_off_ticks_[t]) {
            SynthEvent off_ev;
            off_ev.type = EventType::NoteOff;
            off_ev.source = EventSource::Sequencer;
            off_ev.channel = static_cast<uint8_t>(t);
            off_ev.id = (uint16_t)last_played_notes_[t];
            off_ev.value = 0;
            off_ev.timestamp_us = (uint32_t)esp_timer_get_time();
            event_bus.send(off_ev);

            last_played_notes_[t] = -1;
        }

        if ((tick_count % ticks_per_step) == 0) {
            uint8_t step_idx = current_step_;
            const auto& s = patterns_[current_pattern_][t][step_idx];

            if (s.active) {
                SynthEvent on_ev;
                on_ev.type = EventType::NoteOn;
                on_ev.source = EventSource::Sequencer;
                on_ev.channel = static_cast<uint8_t>(t);
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

    if ((tick_count % ticks_per_step) == 0) {
        current_step_ = (current_step_ + 1) % kMaxSteps;
    }
}

} // namespace smk
