#pragma once

#include "screen_base.h"
#include "widgets.h"
#include <cstdint>

namespace smk {

enum class TakeoverStatus : uint8_t {
    Captured,
    ApproachingFromBelow,
    ApproachingFromAbove,
    Decoupled
};

class ParameterScreen : public ScreenBase {
public:
    ParameterScreen();
    ~ParameterScreen() override = default;

    void onEnter() override;
    void update() override;
    void render(DisplayDriver& display) override;
    const char* name() const override { return "ParameterOverlay"; }

    void showParameter(const char* name, const char* target_layer,
                       float current_val, float saved_val, 
                       const char* unit_str, TakeoverStatus takeover);

    bool isExpired() const;
    void resetTimer();

private:
    char param_name_[32]{"CUTOFF FREQUENCY"};
    char target_layer_[12]{"SYNTH"};
    char unit_str_[12]{"kHz"};
    float current_val_{2.40f};
    float saved_val_{1.25f};
    TakeoverStatus takeover_{TakeoverStatus::Captured};
    
    uint32_t active_start_ms_{0};
    uint32_t timeout_ms_{1500};
    bool expired_{false};

    ProgressBar progress_bar_;
};

} // namespace smk
