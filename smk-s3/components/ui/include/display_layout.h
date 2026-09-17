#pragma once

#include <cstdint>

namespace smk {

enum class DisplayLayoutClass : uint8_t {
    Compact,
    Square,
    Widescreen,
};

constexpr DisplayLayoutClass classifyDisplayLayout(int16_t width, int16_t height) {
    const bool squareish = width >= 180 && height >= 180 &&
                           width * 4 >= height * 3 && height * 4 >= width * 3;
    if (squareish) {
        return DisplayLayoutClass::Square;
    }
    if (width <= 160) {
        return DisplayLayoutClass::Compact;
    }
    return DisplayLayoutClass::Widescreen;
}

// A 240x240 RGB565 frame occupies 115,200 bytes and needs about 23 ms on a
// 40 MHz SPI link before command overhead. Capping large panels near 15 FPS
// leaves bounded time and PSRAM bandwidth for control work while audio stays
// on its dedicated core. Compact/widescreen panels retain the existing 30 FPS.
constexpr uint32_t displayFrameIntervalMs(int16_t width, int16_t height) {
    const int32_t pixel_count = static_cast<int32_t>(width) * height;
    return pixel_count > 40000 ? 66U : 33U;
}

} // namespace smk
