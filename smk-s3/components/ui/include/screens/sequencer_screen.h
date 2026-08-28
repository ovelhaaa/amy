#pragma once

#include "screen_base.h"
#include <cstdint>
#include <array>

namespace smk {

class StepSequencer;

class SequencerScreen : public ScreenBase {
public:
    SequencerScreen();
    ~SequencerScreen() override = default;

    void update() override;
    void render(DisplayDriver& display) override;
    const char* name() const override { return "Sequencer"; }

    void setSequencer(const StepSequencer* seq) { sequencer_ = seq; }

    void setPatternNumber(uint8_t pat);
    void setBpm(float bpm);
    void setSwing(uint8_t swing_percent);
    void setPlaying(bool playing) { is_playing_ = playing; }
    void setRecording(bool recording) { is_recording_ = recording; }
    void setSelectedTrack(uint8_t track_idx) { selected_track_ = track_idx % 4; }
    void setStepPage(uint8_t page) { step_page_ = page > 0 ? 1 : 0; }
    void setCurrentStep(uint8_t step);

    void setTrackMask(uint8_t track_idx, uint16_t mask);
    void setTrackPlockMask(uint8_t track_idx, uint16_t mask);
    void setTrackMute(uint8_t track_idx, bool mute);
    void setTrackName(uint8_t track_idx, const char* name);
    void setTrackName(const char* name); // Compatibility overload for track 0

private:
    const StepSequencer* sequencer_{nullptr};

    uint8_t pattern_num_{1};
    float bpm_{124.0f};
    uint8_t swing_{50};
    bool is_playing_{false};
    bool is_recording_{false};
    uint8_t selected_track_{0};
    uint8_t step_page_{0}; // 0 = Steps 1-8, 1 = Steps 9-16
    uint8_t current_step_{0};

    bool chain_enabled_{false};
    uint8_t chain_length_{0};
    uint8_t chain_index_{0};
    std::array<uint8_t, 16> chain_patterns_{0};

    std::array<char[8], 4> track_names_{"BD", "SD", "CH", "OH"};
    std::array<uint16_t, 4> track_masks_{0x1111, 0x0404, 0x5555, 0x0040};
    std::array<uint16_t, 4> track_plock_masks_{0, 0, 0, 0};
    std::array<bool, 4> track_mutes_{false, false, false, false};
};

} // namespace smk
