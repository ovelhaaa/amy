#pragma once

#include "display_driver.h"

namespace smk {

class ScreenBase {
public:
    virtual ~ScreenBase() = default;

    virtual void onEnter() {}
    virtual void onExit() {}
    virtual void update() = 0;
    virtual void render(DisplayDriver& display) = 0;
    virtual const char* name() const = 0;
};

} // namespace smk
