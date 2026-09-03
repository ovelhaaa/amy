#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <algorithm>
#include "event_bus.h"
#include <freertos/FreeRTOS.h>

namespace smk {

enum class ArpMode : uint8_t {
    Up       = 0,
    Down     = 1,
    UpDown   = 2,
    Random   = 3,
    AsPlayed = 4,
    Chord    = 5
};

enum class ArpDivision : uint8_t {
    Div1_4   = 0, // 24 PPQN ticks
    Div1_8   = 1, // 12 PPQN ticks
    Div1_16  = 2, // 6  PPQN ticks
    Div1_32  = 3, // 3  PPQN ticks
    Div1_8T  = 4, // 8  PPQN ticks (Triplet)
    Div1_16T = 5  // 4  PPQN ticks (Triplet)
};

class Arpeggiator {
public:
    struct HeldNote {
        uint8_t note;
        uint8_t velocity;
    };

    Arpeggiator();

    void noteOn(uint8_t note, uint8_t velocity);
    void noteOff(uint8_t note);

    void processTick(uint32_t tick_count, EventBus& event_bus);

    void setEnabled(bool enable, EventBus* event_bus = nullptr);
    bool isEnabled() const { return enabled_; }

    void setMode(ArpMode mode) { mode_ = mode; }
    ArpMode mode() const { return mode_; }

    void setDivision(ArpDivision div) { division_ = div; }
    ArpDivision division() const { return division_; }

    void setOctaves(uint8_t octaves);
    uint8_t octaves() const { return octaves_; }

    void setGatePercent(float gate) { gate_percent_ = gate; }
    float gatePercent() const { return gate_percent_; }

    void setSwing(float swing_pct) { swing_percent_ = std::clamp(swing_pct, 0.0f, 75.0f); }
    float swing() const { return swing_percent_; }

    void setLatch(bool enable);
    bool latch() const { return latch_; }

    void reset();
    void reset(EventBus& event_bus);

private:
    uint32_t getTicksPerStep() const;
    void generateArpPattern();

    bool                    enabled_ = false;
    ArpMode                 mode_ = ArpMode::Up;
    ArpDivision             division_ = ArpDivision::Div1_16;
    uint8_t                 octaves_ = 1;
    float                   gate_percent_ = 50.0f;
    float                   swing_percent_ = 0.0f;
    bool                    latch_ = false;
    portMUX_TYPE            mux_ = portMUX_INITIALIZER_UNLOCKED;

    std::vector<HeldNote>   held_notes_;
    std::vector<HeldNote>   latched_notes_;

    static constexpr size_t kMaxPatternNotes = 64;
    static constexpr size_t kMaxChordNotes = 16;
    std::array<HeldNote, kMaxPatternNotes> pattern_buffer_{};
    size_t                  pattern_len_ = 0;

    std::array<int16_t, kMaxChordNotes> active_chord_notes_{};
    size_t                  active_chord_count_ = 0;
    
    size_t                  current_step_idx_ = 0;
    bool                    up_direction_ = true;
    int16_t                 last_played_note_ = -1;
    uint32_t                note_off_tick_ = 0;
};

} // namespace smk
