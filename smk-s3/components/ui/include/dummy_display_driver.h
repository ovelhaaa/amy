#pragma once

#include "display_driver.h"
#include <cstdint>
#include <atomic>

namespace smk {

class DummyDisplayDriver : public DisplayDriver {
public:
    explicit DummyDisplayDriver(int16_t width = 284, int16_t height = 76);
    ~DummyDisplayDriver() override;

    bool begin() override;
    void setBrightness(uint8_t value) override;
    void fillScreen(uint16_t color) override;
    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
    void flush() override;
    void flushRegion(int16_t x, int16_t y, int16_t w, int16_t h) override;

    int16_t width() const override { return width_; }
    int16_t height() const override { return height_; }

    const uint16_t* framebuffer() const { return framebuffer_; }
    uint32_t flushCount() const { return flush_count_.load(); }
    uint8_t brightness() const { return brightness_; }

private:
    int16_t width_{284};
    int16_t height_{76};
    uint16_t* framebuffer_{nullptr};
    uint8_t brightness_{255};
    std::atomic<uint32_t> flush_count_{0};
};

} // namespace smk
