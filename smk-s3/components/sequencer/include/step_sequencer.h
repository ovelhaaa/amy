#pragma once
#include <cstdint>
#include <array>
#include <algorithm>
#include "event_bus.h"

namespace smk {

struct StepData {
    uint8_t note = 60;        // MIDI note (0..127)
    uint8_t velocity = 100;   // Velocity (0..127)
    uint8_t gate_percent = 50;// Gate time (10..100%)
    uint8_t probability = 100;// Trigger probability (0..100%)
    uint8_t ratchet = 1;      // Sub-step ratchets (1..4)
    bool    active = false;   // Active step (on/off)
    bool    slide = false;    // Legato slide
};

enum class SequencerState : uint8_t {
    Stopped = 0,
    Playing = 1,
    Recording = 2
};

class StepSequencer {
public:
    static constexpr size_t kMaxTracks = 4;
    static constexpr size_t kMaxSteps = 16;
    static constexpr size_t kMaxPatterns = 8;

    StepSequencer();

    void setStep(uint8_t track_idx, uint8_t step_idx, uint8_t note, uint8_t velocity, bool active, bool slide = false);
    void setStep(uint8_t step_idx, uint8_t note, uint8_t velocity, bool active, bool slide = false); // Track 0 overload
    const StepData& step(uint8_t track_idx, uint8_t step_idx) const;
    StepData& step(uint8_t track_idx, uint8_t step_idx);
    const StepData& step(uint8_t step_idx) const;
    StepData& step(uint8_t step_idx);

    void play();
    void stop();
    void record();

    SequencerState state() const { return state_; }
    bool isPlaying() const { return state_ != SequencerState::Stopped; }

    uint8_t currentStep() const { return current_step_; }

    void selectPattern(uint8_t pattern_idx);
    uint8_t currentPattern() const { return current_pattern_; }

    void setSwing(float swing_pct) { swing_percent_ = std::clamp(swing_pct, 0.0f, 75.0f); }
    float swing() const { return swing_percent_; }

    void processTick(uint32_t tick_count, EventBus& event_bus);
    void reset();

private:
    std::array<std::array<std::array<StepData, kMaxSteps>, kMaxTracks>, kMaxPatterns> patterns_;
    SequencerState                                                                     state_ = SequencerState::Stopped;
    uint8_t                                                                            current_step_ = 0;
    uint8_t                                                                            current_pattern_ = 0;
    float                                                                              swing_percent_ = 0.0f;
    std::array<int16_t, kMaxTracks>                                                    last_played_notes_;
    std::array<uint32_t, kMaxTracks>                                                   note_off_ticks_;
};

} // namespace smk
