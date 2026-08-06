#include "dummy_display_driver.h"
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <cstring>
#include <cstdlib>
#include <algorithm>

static const char* TAG = "DummyDisplay";

namespace smk {

DummyDisplayDriver::DummyDisplayDriver() {}

DummyDisplayDriver::~DummyDisplayDriver() {
    if (framebuffer_) {
        free(framebuffer_);
        framebuffer_ = nullptr;
    }
}

bool DummyDisplayDriver::begin() {
    size_t fb_size = kWidth * kHeight * sizeof(uint16_t);
    // Allocate framebuffer in PSRAM if available, fallback to internal RAM
    framebuffer_ = (uint16_t*)heap_caps_malloc(fb_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!framebuffer_) {
        framebuffer_ = (uint16_t*)heap_caps_malloc(fb_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    
    if (!framebuffer_) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer (%zu bytes)", fb_size);
        return false;
    }
    
    fillScreen(kColorBlack);
    ESP_LOGI(TAG, "DummyDisplayDriver initialized successfully (Framebuffer: %zu KB)", fb_size / 1024);
    return true;
}

void DummyDisplayDriver::setBrightness(uint8_t value) {
    brightness_ = value;
}

void DummyDisplayDriver::fillScreen(uint16_t color) {
    if (!framebuffer_) return;
    for (int i = 0; i < kWidth * kHeight; ++i) {
        framebuffer_[i] = color;
    }
}

void DummyDisplayDriver::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (!framebuffer_ || x < 0 || x >= kWidth || y < 0 || y >= kHeight) return;
    framebuffer_[y * kWidth + x] = color;
}

void DummyDisplayDriver::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (!framebuffer_ || w <= 0 || h <= 0) return;
    int16_t x2 = std::min((int16_t)(x + w), kWidth);
    int16_t y2 = std::min((int16_t)(y + h), kHeight);
    int16_t x1 = std::max(x, (int16_t)0);
    int16_t y1 = std::max(y, (int16_t)0);

    for (int16_t iy = y1; iy < y2; ++iy) {
        for (int16_t ix = x1; ix < x2; ++ix) {
            framebuffer_[iy * kWidth + ix] = color;
        }
    }
}

void DummyDisplayDriver::flush() {
    flush_count_.fetch_add(1, std::memory_order_relaxed);
}

void DummyDisplayDriver::flushRegion(int16_t x, int16_t y, int16_t w, int16_t h) {
    (void)x; (void)y; (void)w; (void)h;
    flush_count_.fetch_add(1, std::memory_order_relaxed);
}

} // namespace smk
