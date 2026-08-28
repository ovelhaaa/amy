#include "dummy_display_driver.h"
#ifdef ESP_PLATFORM
#include <esp_log.h>
#include <esp_heap_caps.h>
static const char* TAG = "DummyDisplay";
#endif
#include <cstring>
#include <cstdlib>
#include <algorithm>

namespace smk {

DummyDisplayDriver::DummyDisplayDriver(int16_t width, int16_t height)
    : width_(width), height_(height) {}

DummyDisplayDriver::~DummyDisplayDriver() {
    if (framebuffer_) {
        free(framebuffer_);
        framebuffer_ = nullptr;
    }
}

bool DummyDisplayDriver::begin() {
    size_t fb_size = (size_t)width_ * height_ * sizeof(uint16_t);
#ifdef ESP_PLATFORM
    // Allocate framebuffer in PSRAM if available, fallback to internal RAM
    framebuffer_ = (uint16_t*)heap_caps_malloc(fb_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!framebuffer_) {
        framebuffer_ = (uint16_t*)heap_caps_malloc(fb_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (!framebuffer_) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer (%zu bytes)", fb_size);
        return false;
    }
    ESP_LOGI(TAG, "DummyDisplayDriver initialized successfully (%dx%d, Framebuffer: %zu KB)", 
             width_, height_, fb_size / 1024);
#else
    framebuffer_ = (uint16_t*)malloc(fb_size);
    if (!framebuffer_) {
        return false;
    }
#endif
    
    fillScreen(kColorBlack);
    invalidate();
    return true;
}

void DummyDisplayDriver::setBrightness(uint8_t value) {
    brightness_ = value;
}

void DummyDisplayDriver::fillScreen(uint16_t color) {
    if (!framebuffer_) return;
    int32_t total = (int32_t)width_ * height_;
    for (int32_t i = 0; i < total; ++i) {
        framebuffer_[i] = color;
    }
    invalidate();
}

void DummyDisplayDriver::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (!framebuffer_ || x < 0 || x >= width_ || y < 0 || y >= height_) return;
    framebuffer_[y * width_ + x] = color;
    markDirty(x, y, 1, 1);
}

void DummyDisplayDriver::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (!framebuffer_ || w <= 0 || h <= 0) return;
    int16_t x2 = std::min((int16_t)(x + w), width_);
    int16_t y2 = std::min((int16_t)(y + h), height_);
    int16_t x1 = std::max(x, (int16_t)0);
    int16_t y1 = std::max(y, (int16_t)0);
    if (x1 >= x2 || y1 >= y2) return;

    for (int16_t iy = y1; iy < y2; ++iy) {
        for (int16_t ix = x1; ix < x2; ++ix) {
            framebuffer_[iy * width_ + ix] = color;
        }
    }
    markDirty(x1, y1, x2 - x1, y2 - y1);
}

void DummyDisplayDriver::flush() {
    if (!is_dirty_) return;
    flush_count_.fetch_add(1, std::memory_order_relaxed);
    clearDirty();
}

void DummyDisplayDriver::flushRegion(int16_t x, int16_t y, int16_t w, int16_t h) {
    (void)x; (void)y; (void)w; (void)h;
    flush_count_.fetch_add(1, std::memory_order_relaxed);
}

} // namespace smk
