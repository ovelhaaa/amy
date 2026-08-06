#pragma once

#include "screen_base.h"
#include <cstdint>

namespace smk {

class SequencerScreen : public ScreenBase {
public:
    SequencerScreen();
    ~SequencerScreen() override = default;

    void update() override;
    void render(DisplayDriver& display) override;
    const char* name() const override { return "SequencerPattern"; }

    void setPatternNumber(uint8_t pat);
    void setBpm(float bpm);
    void setSwing(uint8_t swing_percent);
    void setTrackName(const char* track_name);
    void setStepActive(uint8_t step, bool active);
    void setCurrentStep(uint8_t step);

private:
    uint8_t pattern_num_{3};
    float bpm_{124.0f};
    uint8_t swing_{54};
    char track_name_[16]{"BD (DRUM)"};
    uint16_t step_mask_{0x9249}; // Default pattern: steps 0, 3, 6, 9, 12, 15
    uint8_t current_step_{0};
};

} // namespace smk
