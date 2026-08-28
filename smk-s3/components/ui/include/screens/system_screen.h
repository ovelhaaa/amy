#pragma once

#include "screen_base.h"
#include <cstdint>
#include <cstdio>

namespace smk {

class SystemScreen : public ScreenBase {
public:
    SystemScreen() = default;
    ~SystemScreen() override = default;

    void update() override;
    void render(DisplayDriver& display) override;
    const char* name() const override { return "SystemDiagnostics"; }

    void setConfigInfo(const char* vel_curve, uint8_t swing, bool limiter) {
        if (vel_curve) snprintf(vel_curve_, sizeof(vel_curve_), "%s", vel_curve);
        swing_ = swing;
        limiter_ = limiter;
    }

private:
    char vel_curve_[16]{"LINEAR"};
    uint8_t swing_{50};
    bool limiter_{true};
};

} // namespace smk
