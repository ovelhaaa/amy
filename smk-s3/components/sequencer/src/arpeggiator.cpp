#include "arpeggiator.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <algorithm>
#include <random>

[[maybe_unused]] static const char* TAG = "Arpeggiator";

namespace smk {

Arpeggiator::Arpeggiator() {
    held_notes_.reserve(16);
    latched_notes_.reserve(16);
}

void Arpeggiator::setEnabled(bool enable) {
    enabled_ = enable;
    if (!enabled_) {
        reset();
    }
}

void Arpeggiator::setOctaves(uint8_t octaves) {
    octaves_ = std::clamp(octaves, (uint8_t)1, (uint8_t)4);
}

void Arpeggiator::setLatch(bool enable) {
    latch_ = enable;
    if (!latch_) {
        latched_notes_.clear();
    }
}

void Arpeggiator::noteOn(uint8_t note, uint8_t velocity) {
    // Check if note already present
    auto it = std::find_if(held_notes_.begin(), held_notes_.end(), [note](const HeldNote& hn) {
        return hn.note == note;
    });

    if (it == held_notes_.end()) {
        held_notes_.push_back({note, velocity});
    } else {
        it->velocity = velocity;
    }

    if (latch_) {
        latched_notes_ = held_notes_;
    }
}

void Arpeggiator::noteOff(uint8_t note) {
    auto it = std::find_if(held_notes_.begin(), held_notes_.end(), [note](const HeldNote& hn) {
        return hn.note == note;
    });

    if (it != held_notes_.end()) {
        held_notes_.erase(it);
    }
}

void Arpeggiator::reset() {
    held_notes_.clear();
    latched_notes_.clear();
    current_step_idx_ = 0;
    up_direction_ = true;
    last_played_note_ = -1;
}

uint32_t Arpeggiator::getTicksPerStep() const {
    switch (division_) {
        case ArpDivision::Div1_4:   return 24;
        case ArpDivision::Div1_8:   return 12;
        case ArpDivision::Div1_16:  return 6;
        case ArpDivision::Div1_32:  return 3;
        case ArpDivision::Div1_8T:  return 8;
        case ArpDivision::Div1_16T: return 4;
        default:                    return 6;
    }
}

void Arpeggiator::generateArpPattern(std::vector<HeldNote>& pattern_out) {
    pattern_out.clear();
    const auto& base_notes = (!held_notes_.empty()) ? held_notes_ : latched_notes_;

    if (base_notes.empty()) return;

    std::vector<HeldNote> sorted_notes = base_notes;
    if (mode_ != ArpMode::AsPlayed) {
        std::sort(sorted_notes.begin(), sorted_notes.end(), [](const HeldNote& a, const HeldNote& b) {
            return a.note < b.note;
        });
    }

    for (uint8_t oct = 0; oct < octaves_; ++oct) {
        for (const auto& hn : sorted_notes) {
            int16_t transposed_note = hn.note + (oct * 12);
            if (transposed_note <= 127) {
                pattern_out.push_back({(uint8_t)transposed_note, hn.velocity});
            }
        }
    }
}

void Arpeggiator::processTick(uint32_t tick_count, EventBus& event_bus) {
    if (!enabled_) return;

    uint32_t ticks_per_step = getTicksPerStep();

    // Check if we need to emit a NoteOff for the previous arpeggiated note
    if (last_played_note_ >= 0 && tick_count >= note_off_tick_) {
        SynthEvent off_ev;
        off_ev.type = EventType::NoteOff;
        off_ev.source = EventSource::Arpeggiator;
        off_ev.channel = 0;
        off_ev.id = (uint16_t)last_played_note_;
        off_ev.value = 0;
        off_ev.timestamp_us = (uint32_t)esp_timer_get_time();
        event_bus.send(off_ev);

        last_played_note_ = -1;
    }

    // Check if current tick reaches a step boundary (with swing on 16th notes)
    bool is_step_tick = false;
    if (ticks_per_step == 6 && swing_percent_ > 50.0f) {
        uint32_t tick_split = (uint32_t)std::round(12.0f * (swing_percent_ / 100.0f));
        if (tick_split < 6) tick_split = 6;
        if (tick_split > 9) tick_split = 9;
        uint32_t mod12 = tick_count % 12;
        is_step_tick = (mod12 == 0) || (mod12 == tick_split);
    } else {
        is_step_tick = ((tick_count % ticks_per_step) == 0);
    }

    if (is_step_tick) {
        std::vector<HeldNote> pattern;
        generateArpPattern(pattern);

        if (pattern.empty()) {
            current_step_idx_ = 0;
            return;
        }

        if (current_step_idx_ >= pattern.size()) {
            current_step_idx_ = 0;
        }

        HeldNote current_hn = pattern[current_step_idx_];

        // Advance index according to ArpMode
        switch (mode_) {
            case ArpMode::Up:
            case ArpMode::AsPlayed:
                current_step_idx_ = (current_step_idx_ + 1) % pattern.size();
                break;
            case ArpMode::Down:
                if (current_step_idx_ == 0) current_step_idx_ = pattern.size() - 1;
                else current_step_idx_--;
                break;
            case ArpMode::UpDown:
                if (up_direction_) {
                    if (current_step_idx_ + 1 < pattern.size()) {
                        current_step_idx_++;
                    } else {
                        up_direction_ = false;
                        if (current_step_idx_ > 0) current_step_idx_--;
                    }
                } else {
                    if (current_step_idx_ > 0) {
                        current_step_idx_--;
                    } else {
                        up_direction_ = true;
                        if (pattern.size() > 1) current_step_idx_ = 1;
                    }
                }
                break;
            case ArpMode::Random:
                current_step_idx_ = rand() % pattern.size();
                break;
            case ArpMode::Chord:
                current_step_idx_ = 0;
                break;
        }

        if (mode_ == ArpMode::Chord) {
            // Emit NoteOn for all notes in pattern simultaneously
            for (const auto& hn : pattern) {
                SynthEvent on_ev;
                on_ev.type = EventType::NoteOn;
                on_ev.source = EventSource::Arpeggiator;
                on_ev.channel = 0;
                on_ev.id = hn.note;
                on_ev.value = hn.velocity;
                on_ev.timestamp_us = (uint32_t)esp_timer_get_time();
                event_bus.send(on_ev);
            }
            last_played_note_ = pattern[0].note;
        } else {
            // Emit NoteOn for single arpeggiated note
            SynthEvent on_ev;
            on_ev.type = EventType::NoteOn;
            on_ev.source = EventSource::Arpeggiator;
            on_ev.channel = 0;
            on_ev.id = current_hn.note;
            on_ev.value = current_hn.velocity;
            on_ev.timestamp_us = (uint32_t)esp_timer_get_time();
            event_bus.send(on_ev);
            last_played_note_ = current_hn.note;
        }

        uint32_t gate_ticks = static_cast<uint32_t>((ticks_per_step * gate_percent_) / 100.0f);
        if (gate_ticks < 1) gate_ticks = 1;
        note_off_tick_ = tick_count + gate_ticks;
    }
}

} // namespace smk
