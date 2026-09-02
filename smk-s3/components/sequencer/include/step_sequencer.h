#pragma once
#include <cstdint>
#include <array>
#include <algorithm>
#include "event_bus.h"

namespace smk {

struct StepData {
    uint8_t note = 36;        // MIDI note (0..127)
    uint8_t velocity = 100;   // Velocity (0..127)
    uint8_t gate_percent = 50;// Gate time (10..100%)
    uint8_t probability = 100;// Trigger probability (0..100%)
    uint8_t ratchet = 1;      // Sub-step ratchets (1..4)
    bool    active = false;   // Active step (on/off)
    bool    slide = false;    // Legato slide

    // Parameter Lock (P-Lock) Automation per Step
    bool    has_plock = false;
    uint8_t locked_param = 0xFF; // 0..7 for Macro A..H (or custom param ID)
    float   locked_val = 0.0f;   // Parameter value (0.0..127.0)
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
    static constexpr size_t kMaxChainLength = 16;

    struct PatternChain {
        std::array<uint8_t, kMaxChainLength> patterns{0};
        uint8_t length = 0;
        uint8_t current_index = 0;
        bool    enabled = false;
        bool    loop = true;
    };

    StepSequencer();

    void setStep(uint8_t track_idx, uint8_t step_idx, uint8_t note, uint8_t velocity, bool active, bool slide = false);
    void setStep(uint8_t step_idx, uint8_t note, uint8_t velocity, bool active, bool slide = false); // Track 0 overload
    void toggleStep(uint8_t track_idx, uint8_t step_idx, uint8_t velocity = 100);
    const StepData& step(uint8_t track_idx, uint8_t step_idx) const;
    StepData& step(uint8_t track_idx, uint8_t step_idx);
    const StepData& step(uint8_t step_idx) const;
    StepData& step(uint8_t step_idx);

    uint16_t getTrackStepMask(uint8_t track_idx) const;
    uint16_t getTrackPlockMask(uint8_t track_idx) const;

    // Parameter Locks (P-Locks)
    void setStepParamLock(uint8_t track_idx, uint8_t step_idx, uint8_t param_id, float value);
    void setStepParamLock(uint8_t step_idx, uint8_t param_id, float value);
    void clearStepParamLock(uint8_t track_idx, uint8_t step_idx);
    bool hasStepParamLock(uint8_t track_idx, uint8_t step_idx) const;
    void recordLiveMotion(uint8_t param_id, float value);

    // Pattern Chaining (Song Mode)
    void setPatternChain(const uint8_t* pattern_list, uint8_t length, bool loop = true);
    void clearPatternChain();
    void togglePatternChain();
    const PatternChain& patternChain() const { return chain_; }

    void play();
    void stop();
    void stop(EventBus& event_bus);
    void record();

    SequencerState state() const { return state_; }
    bool isPlaying() const { return state_ != SequencerState::Stopped; }
    bool isRecording() const { return state_ == SequencerState::Recording; }

    uint8_t currentStep() const { return current_step_; }

    void selectPattern(uint8_t pattern_idx);
    uint8_t currentPattern() const { return current_pattern_; }
    void clearPattern(uint8_t pattern_idx);
    void clearTrack(uint8_t track_idx);

    void setSwing(float swing_pct) { swing_percent_ = std::clamp(swing_pct, 0.0f, 75.0f); }
    float swing() const { return swing_percent_; }

    // Track Navigation & Selection
    uint8_t selectedTrack() const { return selected_track_; }
    void selectTrack(uint8_t track_idx) { if (track_idx < kMaxTracks) selected_track_ = track_idx; }
    void nextTrack() { selected_track_ = (selected_track_ + 1) % kMaxTracks; }
    void previousTrack() { selected_track_ = (selected_track_ == 0) ? (kMaxTracks - 1) : (selected_track_ - 1); }

    // Step Page Selection (Page 0: Steps 1-8, Page 1: Steps 9-16)
    uint8_t stepPage() const { return step_page_; }
    void setStepPage(uint8_t page) { step_page_ = (page > 0) ? 1 : 0; }
    void toggleStepPage() { step_page_ = (step_page_ == 0) ? 1 : 0; }

    // Track Mute / Solo
    void setTrackMute(uint8_t track_idx, bool mute);
    bool isTrackMuted(uint8_t track_idx) const;
    void toggleTrackMute(uint8_t track_idx);
    void setTrackSolo(uint8_t track_idx, bool solo);
    bool isTrackSolo(uint8_t track_idx) const;
    void toggleTrackSolo(uint8_t track_idx);
    bool isAnyTrackSolo() const;

    // Track metadata
    const char* trackName(uint8_t track_idx) const;
    uint8_t trackNote(uint8_t track_idx) const;
    uint8_t trackChannel(uint8_t track_idx) const;
    void setTrackMetadata(uint8_t track_idx, const char* name, uint8_t default_note, uint8_t channel);

    // Live Tap Recording (quantized to current step)
    void recordLiveHit(uint8_t note, uint8_t velocity);
    void recordLiveTrackHit(uint8_t track_idx, uint8_t velocity);

    void processTick(uint32_t tick_count, EventBus& event_bus);
    void reset();

private:
    std::array<std::array<std::array<StepData, kMaxSteps>, kMaxTracks>, kMaxPatterns> patterns_;
    SequencerState                                                                     state_ = SequencerState::Stopped;
    uint8_t                                                                            current_step_ = 0;
    uint8_t                                                                            current_pattern_ = 0;
    uint8_t                                                                            selected_track_ = 0;
    uint8_t                                                                            step_page_ = 0; // 0=Steps 1..8, 1=Steps 9..16
    float                                                                              swing_percent_ = 0.0f;

    PatternChain                                                                       chain_;

    std::array<bool, kMaxTracks>                                                       track_mutes_{false, false, false, false};
    std::array<bool, kMaxTracks>                                                       track_solos_{false, false, false, false};
    std::array<char[8], kMaxTracks>                                                    track_names_{"BD", "SD", "CH", "OH"};
    std::array<uint8_t, kMaxTracks>                                                    track_notes_{36, 38, 42, 46};
    std::array<uint8_t, kMaxTracks>                                                    track_channels_{9, 9, 9, 9};

    std::array<int16_t, kMaxTracks>                                                    last_played_notes_;
    std::array<uint32_t, kMaxTracks>                                                   note_off_ticks_;
};

} // namespace smk
