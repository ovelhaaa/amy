#pragma once

#include "screen_base.h"
#include <cstdint>

namespace smk {

class SplashScreen : public ScreenBase {
public:
    SplashScreen();
    ~SplashScreen() override = default;

    void onEnter() override;
    void onExit() override;
    void update() override;
    void render(DisplayDriver& display) override;
    const char* name() const override { return "Splash"; }

    bool isFinished() const { return is_finished_; }
    void dismiss() { is_finished_ = true; }
    void setDuration(uint32_t duration_ms) { duration_ms_ = duration_ms; }
    uint32_t startTime() const { return start_time_ms_; }

private:
    uint32_t start_time_ms_{0};
    uint32_t duration_ms_{1500}; // 1.5s splash duration with fast transition to Home
    bool is_finished_{false};
    uint8_t anim_frame_{0};
};

} // namespace smk
