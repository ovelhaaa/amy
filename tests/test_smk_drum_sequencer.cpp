#include <cstdio>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <array>
#include <vector>

// Minimal mock event bus and synth event for host unit test
enum class EventType : uint8_t {
    None,
    NoteOn,
    NoteOff,
    ControlChange,
    ButtonPress,
    PitchBend,
    Modulation,
    AllNotesOff,
    Panic,
    UsbConnect,
    UsbDisconnect
};

enum class EventSource : uint8_t {
    Internal,
    UsbMidi,
    Sequencer,
    Arpeggiator,
    UI,
    BleMidi,
    DinMidi
};

struct SynthEvent {
    EventType type = EventType::None;
    EventSource source = EventSource::Internal;
    uint8_t channel = 0;
    uint16_t id = 0;
    int32_t value = 0;
    uint32_t timestamp_us = 0;
};

class EventBus {
public:
    void send(const SynthEvent& ev) {
        events_.push_back(ev);
    }
    std::vector<SynthEvent> events_;
};

// Include StepSequencer definitions directly for unit testing
namespace smk {

struct StepData {
    uint8_t note = 36;
    uint8_t velocity = 100;
    uint8_t gate_percent = 50;
    uint8_t probability = 100;
    uint8_t ratchet = 1;
    bool    active = false;
    bool    slide = false;

    // Parameter Lock (P-Lock) Automation per Step
    bool    has_plock = false;
    uint8_t locked_param = 0xFF; // 0..7 for Macro A..H
    float   locked_val = 0.0f;
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

    StepSequencer() {
        snprintf(track_names_[0], sizeof(track_names_[0]), "BD");
        snprintf(track_names_[1], sizeof(track_names_[1]), "SD");
        snprintf(track_names_[2], sizeof(track_names_[2]), "CH");
        snprintf(track_names_[3], sizeof(track_names_[3]), "OH");

        track_notes_[0] = 36;
        track_notes_[1] = 38;
        track_notes_[2] = 42;
        track_notes_[3] = 46;
        track_channels_.fill(9);

        reset();
    }

    void reset() {
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

                    if (p == 0) {
                        if (t == 0) patterns_[p][t][i].active = (i == 0 || i == 4 || i == 8 || i == 12 || i == 10);
                        else if (t == 1) patterns_[p][t][i].active = (i == 4 || i == 12);
                        else if (t == 2) patterns_[p][t][i].active = (i % 2 == 0);
                        else if (t == 3) patterns_[p][t][i].active = (i == 2 || i == 6 || i == 10 || i == 14);
                    } else {
                        patterns_[p][t][i].active = false;
                    }
                }
            }
        }
    }

    void selectPattern(uint8_t pattern_idx) {
        if (pattern_idx < kMaxPatterns) current_pattern_ = pattern_idx;
    }

    uint8_t currentPattern() const { return current_pattern_; }

    void clearTrack(uint8_t track_idx) {
        if (track_idx >= kMaxTracks) return;
        for (size_t i = 0; i < kMaxSteps; ++i) {
            patterns_[current_pattern_][track_idx][i].active = false;
            patterns_[current_pattern_][track_idx][i].has_plock = false;
        }
    }

    void toggleStep(uint8_t track_idx, uint8_t step_idx, uint8_t velocity = 100) {
        if (track_idx >= kMaxTracks || step_idx >= kMaxSteps) return;
        auto& s = patterns_[current_pattern_][track_idx][step_idx];
        s.active = !s.active;
        if (s.active) {
            s.velocity = velocity;
            s.note = track_notes_[track_idx];
        }
    }

    uint16_t getTrackStepMask(uint8_t track_idx) const {
        if (track_idx >= kMaxTracks) return 0;
        uint16_t mask = 0;
        for (size_t i = 0; i < kMaxSteps; ++i) {
            if (patterns_[current_pattern_][track_idx][i].active) {
                mask |= (1 << i);
            }
        }
        return mask;
    }

    uint16_t getTrackPlockMask(uint8_t track_idx) const {
        if (track_idx >= kMaxTracks) return 0;
        uint16_t mask = 0;
        for (size_t i = 0; i < kMaxSteps; ++i) {
            if (patterns_[current_pattern_][track_idx][i].has_plock) {
                mask |= (1 << i);
            }
        }
        return mask;
    }

    // Parameter Locks
    void setStepParamLock(uint8_t track_idx, uint8_t step_idx, uint8_t param_id, float value) {
        if (track_idx >= kMaxTracks || step_idx >= kMaxSteps) return;
        auto& s = patterns_[current_pattern_][track_idx][step_idx];
        s.has_plock = true;
        s.locked_param = param_id;
        s.locked_val = value;
    }

    void clearStepParamLock(uint8_t track_idx, uint8_t step_idx) {
        if (track_idx >= kMaxTracks || step_idx >= kMaxSteps) return;
        auto& s = patterns_[current_pattern_][track_idx][step_idx];
        s.has_plock = false;
        s.locked_param = 0xFF;
        s.locked_val = 0.0f;
    }

    bool hasStepParamLock(uint8_t track_idx, uint8_t step_idx) const {
        if (track_idx >= kMaxTracks || step_idx >= kMaxSteps) return false;
        return patterns_[current_pattern_][track_idx][step_idx].has_plock;
    }

    void recordLiveMotion(uint8_t param_id, float value) {
        if (state_ == SequencerState::Recording) {
            setStepParamLock(selected_track_, current_step_, param_id, value);
        }
    }

    // Pattern Chaining
    void setPatternChain(const uint8_t* pattern_list, uint8_t length, bool loop = true) {
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
    }

    void clearPatternChain() {
        chain_.enabled = false;
        chain_.length = 0;
        chain_.current_index = 0;
    }

    const PatternChain& patternChain() const { return chain_; }

    void setTrackMute(uint8_t track_idx, bool mute) { if (track_idx < kMaxTracks) track_mutes_[track_idx] = mute; }
    bool isTrackMuted(uint8_t track_idx) const { return track_idx < kMaxTracks ? track_mutes_[track_idx] : false; }
    void toggleTrackMute(uint8_t track_idx) { if (track_idx < kMaxTracks) track_mutes_[track_idx] = !track_mutes_[track_idx]; }

    void setTrackSolo(uint8_t track_idx, bool solo) { if (track_idx < kMaxTracks) track_solos_[track_idx] = solo; }
    bool isTrackSolo(uint8_t track_idx) const { return track_idx < kMaxTracks ? track_solos_[track_idx] : false; }
    void toggleTrackSolo(uint8_t track_idx) { if (track_idx < kMaxTracks) track_solos_[track_idx] = !track_solos_[track_idx]; }
    bool isAnyTrackSolo() const {
        for (size_t t = 0; t < kMaxTracks; ++t) {
            if (track_solos_[t]) return true;
        }
        return false;
    }

    void recordLiveTrackHit(uint8_t track_idx, uint8_t velocity) {
        if (track_idx >= kMaxTracks) return;
        uint8_t s_idx = current_step_;
        patterns_[current_pattern_][track_idx][s_idx].active = true;
        patterns_[current_pattern_][track_idx][s_idx].velocity = velocity > 0 ? velocity : 100;
        patterns_[current_pattern_][track_idx][s_idx].note = track_notes_[track_idx];
    }

    void play() { state_ = SequencerState::Playing; current_step_ = 0; }
    void stop() { state_ = SequencerState::Stopped; current_step_ = 0; }
    void record() { state_ = (state_ == SequencerState::Recording) ? SequencerState::Playing : SequencerState::Recording; }

    bool isPlaying() const { return state_ != SequencerState::Stopped; }
    bool isRecording() const { return state_ == SequencerState::Recording; }
    uint8_t currentStep() const { return current_step_; }
    uint8_t selectedTrack() const { return selected_track_; }
    void selectTrack(uint8_t t) { if (t < kMaxTracks) selected_track_ = t; }
    uint8_t stepPage() const { return step_page_; }
    void toggleStepPage() { step_page_ = (step_page_ == 0) ? 1 : 0; }
    const char* trackName(uint8_t t) const { return track_names_[t]; }
    uint8_t trackNote(uint8_t t) const { return track_notes_[t]; }

    void processTick(uint32_t tick_count, EventBus& event_bus) {
        if (state_ == SequencerState::Stopped) return;

        constexpr uint32_t ticks_per_step = 6;
        bool any_solo = isAnyTrackSolo();

        for (size_t t = 0; t < kMaxTracks; ++t) {
            if (last_played_notes_[t] >= 0 && tick_count >= note_off_ticks_[t]) {
                SynthEvent off_ev;
                off_ev.type = EventType::NoteOff;
                off_ev.source = EventSource::Sequencer;
                off_ev.channel = track_channels_[t];
                off_ev.id = (uint16_t)last_played_notes_[t];
                off_ev.value = 0;
                event_bus.send(off_ev);
                last_played_notes_[t] = -1;
            }

            if (track_mutes_[t]) continue;
            if (any_solo && !track_solos_[t]) continue;

            if ((tick_count % ticks_per_step) == 0) {
                uint8_t step_idx = current_step_;
                const auto& s = patterns_[current_pattern_][t][step_idx];

                if (s.active) {
                    if (s.has_plock && s.locked_param < 8) {
                        SynthEvent plock_ev;
                        plock_ev.type = EventType::ControlChange;
                        plock_ev.source = EventSource::Sequencer;
                        plock_ev.channel = track_channels_[t];
                        plock_ev.id = static_cast<uint16_t>(s.locked_param + 1);
                        plock_ev.value = static_cast<int32_t>(s.locked_val);
                        event_bus.send(plock_ev);
                    }

                    SynthEvent on_ev;
                    on_ev.type = EventType::NoteOn;
                    on_ev.source = EventSource::Sequencer;
                    on_ev.channel = track_channels_[t];
                    on_ev.id = s.note;
                    on_ev.value = s.velocity;
                    event_bus.send(on_ev);

                    last_played_notes_[t] = s.note;
                    uint32_t gate_ticks = static_cast<uint32_t>((ticks_per_step * s.gate_percent) / 100.0f);
                    if (gate_ticks < 1) gate_ticks = 1;
                    note_off_ticks_[t] = tick_count + gate_ticks;
                }
            }
        }

        if ((tick_count % ticks_per_step) == 0) {
            if (current_step_ == (kMaxSteps - 1)) {
                if (chain_.enabled && chain_.length > 1) {
                    chain_.current_index = (chain_.current_index + 1) % chain_.length;
                    if (!chain_.loop && chain_.current_index == 0) {
                        stop();
                        return;
                    }
                    current_pattern_ = chain_.patterns[chain_.current_index];
                }
                current_step_ = 0;
            } else {
                current_step_ = (current_step_ + 1) % kMaxSteps;
            }
        }
    }

private:
    std::array<std::array<std::array<StepData, kMaxSteps>, kMaxTracks>, kMaxPatterns> patterns_;
    SequencerState state_ = SequencerState::Stopped;
    uint8_t current_step_ = 0;
    uint8_t current_pattern_ = 0;
    uint8_t selected_track_ = 0;
    uint8_t step_page_ = 0;
    float swing_percent_ = 0.0f;

    PatternChain chain_;

    std::array<bool, kMaxTracks> track_mutes_{false, false, false, false};
    std::array<bool, kMaxTracks> track_solos_{false, false, false, false};
    std::array<char[8], kMaxTracks> track_names_{"BD", "SD", "CH", "OH"};
    std::array<uint8_t, kMaxTracks> track_notes_{36, 38, 42, 46};
    std::array<uint8_t, kMaxTracks> track_channels_{9, 9, 9, 9};

    std::array<int16_t, kMaxTracks> last_played_notes_;
    std::array<uint32_t, kMaxTracks> note_off_ticks_;
};

} // namespace smk

int main() {
    printf("=== Running SMK-S3 Drum Sequencer + P-Locks + Pattern Chain Tests ===\n");

    smk::StepSequencer seq;
    EventBus bus;

    // 1. Check default track setup
    assert(strcmp(seq.trackName(0), "BD") == 0);
    assert(strcmp(seq.trackName(1), "SD") == 0);
    assert(strcmp(seq.trackName(2), "CH") == 0);
    assert(strcmp(seq.trackName(3), "OH") == 0);
    assert(seq.trackNote(0) == 36);
    assert(seq.trackNote(1) == 38);
    printf("[PASS] Default drum track metadata verified.\n");

    // 2. Check mask generation on Pattern 0
    uint16_t bd_mask = seq.getTrackStepMask(0);
    assert((bd_mask & (1 << 0)) != 0); // Step 0 active
    assert((bd_mask & (1 << 4)) != 0); // Step 4 active
    printf("[PASS] Track Step Mask generation verified (BD Mask: 0x%04X).\n", bd_mask);

    // 3. Test Step Toggle
    seq.toggleStep(0, 1, 110); // Turn on Step 1
    assert((seq.getTrackStepMask(0) & (1 << 1)) != 0);
    seq.toggleStep(0, 1); // Turn off Step 1
    assert((seq.getTrackStepMask(0) & (1 << 1)) == 0);
    printf("[PASS] Direct step toggle verified.\n");

    // 4. Test Parameter Lock (P-Lock) Assignment & Mask
    seq.setStepParamLock(0, 4, 1, 88.0f); // Lock Macro 2 (Brightness) = 88 on Step 4 of BD
    assert(seq.hasStepParamLock(0, 4));
    assert((seq.getTrackPlockMask(0) & (1 << 4)) != 0);
    printf("[PASS] Parameter Lock assignment and P-Lock mask verified.\n");

    // 5. Test Playback and P-Lock Event Dispatch
    seq.play();
    assert(seq.isPlaying());
    assert(seq.currentStep() == 0);

    // Advance to Step 4 (4 steps * 6 ticks = 24 ticks)
    for (uint32_t t = 0; t <= 24; ++t) {
        seq.processTick(t, bus);
    }
    bool found_plock_ev = false;
    for (const auto& ev : bus.events_) {
        if (ev.type == EventType::ControlChange && ev.id == 2 && ev.value == 88) {
            found_plock_ev = true;
            break;
        }
    }
    assert(found_plock_ev);
    printf("[PASS] Parameter Lock event (CC #2 = 88) dispatched on Step 4.\n");

    // 6. Test Live Motion Recording
    seq.record(); // Enter recording
    assert(seq.isRecording());
    uint8_t cur_s = seq.currentStep();
    seq.recordLiveMotion(0, 120.0f); // Record Macro 1 = 120
    assert(seq.hasStepParamLock(seq.selectedTrack(), cur_s));
    printf("[PASS] Live Motion Record into current step (%u) verified.\n", cur_s);

    // 7. Test Pattern Chaining (Chain P0 -> P1 -> P2)
    uint8_t chain_pat[3] = {0, 1, 2};
    seq.setPatternChain(chain_pat, 3, true);
    assert(seq.patternChain().enabled);
    assert(seq.patternChain().length == 3);
    assert(seq.currentPattern() == 0);

    // Advance through all 16 steps of Pattern 0 (16 * 6 = 96 ticks)
    for (uint32_t t = 0; t < 96; ++t) {
        seq.processTick(t, bus);
    }
    assert(seq.currentPattern() == 1);
    printf("[PASS] Pattern Chain automatically advanced to Pattern 1 after 16 steps.\n");

    // Advance through Pattern 1 (96 ticks)
    for (uint32_t t = 0; t < 96; ++t) {
        seq.processTick(t, bus);
    }
    assert(seq.currentPattern() == 2);
    printf("[PASS] Pattern Chain automatically advanced to Pattern 2 after 32 steps.\n");

    // Advance through Pattern 2 (96 ticks) -> should loop to Pattern 0
    for (uint32_t t = 0; t < 96; ++t) {
        seq.processTick(t, bus);
    }
    assert(seq.currentPattern() == 0);
    printf("[PASS] Pattern Chain looped back to Pattern 0 successfully.\n");

    printf("=== ALL UNIT TESTS PASSED SUCCESSFULLY! ===\n");
    return 0;
}
