#pragma once

#include "screen_base.h"
#include <cstdint>

namespace smk {

class SystemScreen : public ScreenBase {
public:
    SystemScreen() = default;
    ~SystemScreen() override = default;

    void update() override;
    void render(DisplayDriver& display) override;
    const char* name() const override { return "SystemDiagnostics"; }
};

} // namespace smk
