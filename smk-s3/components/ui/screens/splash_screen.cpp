#include "splash_screen.h"
#include "font_renderer.h"
#include "esp_timer.h"
#include <cmath>
#include <cstdio>

namespace smk {

static inline uint16_t dimColor(uint16_t color, float factor) {
    if (factor >= 1.0f) return color;
    if (factor <= 0.0f) return 0x0000;
    uint8_t r = static_cast<uint8_t>(((color >> 11) & 0x1F) * factor);
    uint8_t g = static_cast<uint8_t>(((color >> 5) & 0x3F) * factor);
    uint8_t b = static_cast<uint8_t>((color & 0x1F) * factor);
    return (r << 11) | (g << 5) | b;
}

SplashScreen::SplashScreen() {}

void SplashScreen::onEnter() {
    start_time_ms_ = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    is_finished_ = false;
    anim_frame_ = 0;
}

void SplashScreen::onExit() {
    is_finished_ = true;
}

void SplashScreen::update() {
    uint32_t now = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    anim_frame_++;
    if ((now - start_time_ms_) >= duration_ms_) {
        is_finished_ = true;
    }
}

void SplashScreen::render(DisplayDriver& display) {
    display.fillScreen(DisplayDriver::kColorBlack);

    int16_t dw = display.width();
    int16_t dh = display.height();
    uint32_t now = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    uint32_t elapsed = now - start_time_ms_;
    float progress = (duration_ms_ > 0) ? (static_cast<float>(elapsed) / static_cast<float>(duration_ms_)) : 1.0f;
    if (progress > 1.0f) progress = 1.0f;

    // Smooth Backlight PWM & Visual Luminosity Fade-In (0ms .. 2000ms)
    float fade_ratio = 1.0f;
    if (elapsed < 2000) {
        fade_ratio = static_cast<float>(elapsed) / 2000.0f;
        // Ease-in-out cubic curve for natural human perception of brightness
        fade_ratio = fade_ratio * fade_ratio * (3.0f - 2.0f * fade_ratio);
    }
    uint8_t bl_val = static_cast<uint8_t>(fade_ratio * 255.0f);
    display.setBrightness(bl_val);

    if (dw <= 160) {
        // ── 160x128 Display Layout (1.8" ST7735) ──
        // Outer decorative tech frame
        display.drawRect(2, 2, dw - 4, dh - 4, dimColor(DisplayDriver::kColorDarkGray, fade_ratio));
        display.drawRect(4, 4, dw - 8, dh - 8, dimColor(DisplayDriver::kColorMidGray, fade_ratio));

        // Header Title
        FontRenderer::drawString(display, 8, 10, "SMK-S3 SYNTH", dimColor(DisplayDriver::kColorCyan, fade_ratio), DisplayDriver::kColorBlack, FontType::Font5x7, 2);
        FontRenderer::drawString(display, 12, 26, "AMY DSP SYNTHESIZER", dimColor(DisplayDriver::kColorAmber, fade_ratio), DisplayDriver::kColorBlack, FontType::Font5x7, 1);

        // Animated Central Oscilloscope Graphic (y=38..78)
        display.drawRect(10, 38, 140, 42, dimColor(DisplayDriver::kColorDarkGray, fade_ratio));
        int center_y = 59;
        int prev_y = center_y;
        for (int x = 0; x < 136; ++x) {
            float phase = (x * 0.08f) + (anim_frame_ * 0.15f);
            float harmonic = std::sin(phase * 2.0f) * 0.3f;
            float wave = (std::sin(phase) + harmonic) * 14.0f;
            int y = center_y + static_cast<int>(wave);
            if (y < 40) y = 40;
            if (y > 76) y = 76;
            if (x == 0) prev_y = y;
            display.drawLine(12 + x, prev_y, 12 + x + 1, y, dimColor(DisplayDriver::kColorGreen, fade_ratio));
            prev_y = y;
        }

        // Subtitle badges
        FontRenderer::drawString(display, 12, 84, "PCM5102A 48kHz | 8V", dimColor(DisplayDriver::kColorLightGray, fade_ratio), DisplayDriver::kColorBlack, FontType::Font5x7, 1);

        // Status Boot text sequence for 4.0s
        const char* status_text = "BOOTING ESP32-S3...";
        uint16_t status_col = DisplayDriver::kColorAmber;
        if (elapsed > 3000) {
            status_text = "SYSTEM READY!";
            status_col = DisplayDriver::kColorGreen;
        } else if (elapsed > 2000) {
            status_text = "USB HOST CONNECTED";
            status_col = DisplayDriver::kColorCyan;
        } else if (elapsed > 1000) {
            status_text = "I2S DMA AUDIO OK";
            status_col = DisplayDriver::kColorYellow;
        }
        FontRenderer::drawString(display, 12, 98, status_text, dimColor(status_col, fade_ratio), DisplayDriver::kColorBlack, FontType::Font5x7, 1);

        // Progress bar at bottom (y=112..118)
        display.drawRect(10, 112, 140, 8, dimColor(DisplayDriver::kColorMidGray, fade_ratio));
        int fill_w = static_cast<int>(136.0f * progress);
        if (fill_w > 0) {
            display.fillRect(12, 114, fill_w, 4, dimColor(DisplayDriver::kColorCyan, fade_ratio));
        }
        return;
    }

    // ── 284x76 Ultra-Widescreen Panoramic Layout (ST7789) ──
    // Double technical border
    display.drawRect(0, 0, dw, dh, dimColor(DisplayDriver::kColorMidGray, fade_ratio));
    display.drawHLine(1, 1, dw - 2, dimColor(DisplayDriver::kColorCyan, fade_ratio));
    display.drawHLine(1, dh - 2, dw - 2, dimColor(DisplayDriver::kColorCyan, fade_ratio));

    // Title and Branding Banner
    FontRenderer::drawString(display, 8, 5, "SMK-S3 SYNTHESIZER", dimColor(DisplayDriver::kColorWhite, fade_ratio), DisplayDriver::kColorBlack, FontType::Font5x7, 2);
    FontRenderer::drawString(display, 168, 8, "v1.0.0 [AMY DSP]", dimColor(DisplayDriver::kColorAmber, fade_ratio), DisplayDriver::kColorBlack, FontType::Font5x7, 1);

    display.drawHLine(4, 20, dw - 8, dimColor(DisplayDriver::kColorDarkGray, fade_ratio));

    // Center Left: Animated Waveform Oscilloscope Graphic (x=8..128, y=24..54)
    display.drawRect(8, 23, 120, 32, dimColor(DisplayDriver::kColorDarkGray, fade_ratio));
    int center_y = 39;
    int prev_y = center_y;
    for (int x = 0; x < 116; ++x) {
        float phase = (x * 0.10f) + (anim_frame_ * 0.2f);
        float harmonic = std::sin(phase * 3.0f) * 0.25f;
        float wave = (std::sin(phase) + harmonic) * 11.0f;
        int y = center_y + static_cast<int>(wave);
        if (y < 25) y = 25;
        if (y > 52) y = 52;
        if (x == 0) prev_y = y;
        display.drawLine(10 + x, prev_y, 10 + x + 1, y, dimColor(DisplayDriver::kColorGreen, fade_ratio));
        prev_y = y;
    }

    // Center Right: Hardware System Checklist & Badges (x=134..276)
    FontRenderer::drawString(display, 136, 25, "CORE: ESP32-S3 N16R8", dimColor(DisplayDriver::kColorCyan, fade_ratio), DisplayDriver::kColorBlack, FontType::Font5x7, 1);
    FontRenderer::drawString(display, 136, 35, "DAC : PCM5102A 48kHz I2S", dimColor(DisplayDriver::kColorYellow, fade_ratio), DisplayDriver::kColorBlack, FontType::Font5x7, 1);
    FontRenderer::drawString(display, 136, 45, "MIDI: USB HOST M-VAVE", dimColor(DisplayDriver::kColorWhite, fade_ratio), DisplayDriver::kColorBlack, FontType::Font5x7, 1);

    // Bottom Status & Progress (y=58..70)
    display.drawHLine(4, 57, dw - 8, dimColor(DisplayDriver::kColorDarkGray, fade_ratio));

    const char* status_text = "INITIALIZING DSP ENGINES...";
    uint16_t status_col = DisplayDriver::kColorAmber;
    if (elapsed > 3000) {
        status_text = "READY! PRESS ANY KEY";
        status_col = DisplayDriver::kColorGreen;
    } else if (elapsed > 2000) {
        status_text = "USB HOST ENUMERATED OK";
        status_col = DisplayDriver::kColorCyan;
    } else if (elapsed > 1000) {
        status_text = "AUDIO DMA ENGINE STARTED";
        status_col = DisplayDriver::kColorYellow;
    }

    FontRenderer::drawString(display, 8, 62, status_text, dimColor(status_col, fade_ratio), DisplayDriver::kColorBlack, FontType::Font5x7, 1);

    // Bottom Right Progress Indicator
    int pb_x = 180;
    int pb_w = 96;
    display.drawRect(pb_x, 62, pb_w, 7, dimColor(DisplayDriver::kColorMidGray, fade_ratio));
    int fill_w = static_cast<int>((pb_w - 4) * progress);
    if (fill_w > 0) {
        display.fillRect(pb_x + 2, 64, fill_w, 3, dimColor(DisplayDriver::kColorCyan, fade_ratio));
    }
}

} // namespace smk
