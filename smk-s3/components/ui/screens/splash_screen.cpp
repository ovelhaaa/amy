#include "splash_screen.h"
#include "font_renderer.h"
#include "esp_timer.h"
#include <cmath>

namespace smk {
namespace {

constexpr uint32_t kFadeInMs = 350;

uint16_t dimColor(uint16_t color, float factor) {
    if (factor >= 1.0f) return color;
    if (factor <= 0.0f) return 0;
    const uint8_t r = static_cast<uint8_t>(((color >> 11) & 0x1F) * factor);
    const uint8_t g = static_cast<uint8_t>(((color >> 5) & 0x3F) * factor);
    const uint8_t b = static_cast<uint8_t>((color & 0x1F) * factor);
    return (r << 11) | (g << 5) | b;
}

// Decorative waveform, driven by elapsed time rather than display frame rate.
void drawWaveMark(DisplayDriver& display, int16_t x, int16_t y,
                  uint32_t elapsed_ms, uint16_t color, uint16_t muted) {
    display.drawVLine(x, y, 36, muted);
    display.drawVLine(x + 43, y, 36, muted);
    display.drawHLine(x, y, 7, color);
    display.drawHLine(x + 37, y + 35, 7, color);
    int16_t previous_y = y + 18;
    for (int16_t i = 0; i < 34; ++i) {
        const float phase = i * 0.23f - elapsed_ms * 0.002f;
        const float envelope = std::sin(i * 3.14159265f / 33.0f);
        const int16_t wave_y = y + 18 + static_cast<int16_t>(
            std::sin(phase) * envelope * 12.0f);
        if (i > 0) {
            display.drawLine(x + 4 + i, previous_y, x + 5 + i, wave_y, color);
            display.drawLine(x + 4 + i, previous_y + 1, x + 5 + i, wave_y + 1, color);
        }
        previous_y = wave_y;
    }
}

} // namespace

SplashScreen::SplashScreen() {}

void SplashScreen::onEnter() {
    start_time_ms_ = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    is_finished_ = false;
}

void SplashScreen::onExit() {
    is_finished_ = true;
}

void SplashScreen::update() {
    const uint32_t now = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    if ((now - start_time_ms_) >= duration_ms_) {
        is_finished_ = true;
    }
}

void SplashScreen::render(DisplayDriver& display) {
    display.fillScreen(DisplayDriver::kColorBlack);

    const int16_t dw = display.width();
    const int16_t dh = display.height();
    const uint32_t elapsed = static_cast<uint32_t>(esp_timer_get_time() / 1000) - start_time_ms_;
    const float progress = duration_ms_ > 0
        ? std::fmin(1.0f, static_cast<float>(elapsed) / duration_ms_) : 1.0f;

    // Fade only the backlight, so text is not dimmed twice.
    float fade = std::fmin(1.0f, static_cast<float>(elapsed) / kFadeInMs);
    fade = fade * fade * (3.0f - 2.0f * fade);
    display.setBrightness(static_cast<uint8_t>(fade * 255.0f));

    constexpr uint16_t black = DisplayDriver::kColorBlack;
    constexpr uint16_t white = DisplayDriver::kColorWhite;
    constexpr uint16_t cyan = DisplayDriver::kColorCyan;
    constexpr uint16_t gray = DisplayDriver::kColorLightGray;
    const uint16_t muted = dimColor(cyan, 0.25f);

    if (dw <= 160) {
        FontRenderer::drawString(display, (dw - 108) / 2, 12, "SMK-S3",
                                 white, black, FontType::Font5x7, 3);
        const int16_t subtitle_width = FontRenderer::stringWidth("STANDALONE SYNTHESIZER", FontType::Font3x5);
        FontRenderer::drawString(display, (dw - subtitle_width) / 2, 40, "STANDALONE SYNTHESIZER",
                                 cyan, black, FontType::Font3x5);
        drawWaveMark(display, (dw - 44) / 2, 53, elapsed, cyan, muted);
        FontRenderer::drawString(display, (dw - 56) / 2, 98, "POWERED BY AMY",
                                 gray, black, FontType::Font3x5);
    } else {
        // Independent zones for mark, product name and AMY credit.
        drawWaveMark(display, 12, 13, elapsed, cyan, muted);
        FontRenderer::drawString(display, 70, 15, "SMK-S3",
                                 white, black, FontType::Font5x7, 3);
        FontRenderer::drawString(display, 72, 42, "STANDALONE SYNTHESIZER",
                                 cyan, black, FontType::Font3x5);

        const int16_t credit_x = dw - 58;
        display.drawVLine(credit_x - 10, 18, 27, DisplayDriver::kColorDarkGray);
        FontRenderer::drawString(display, credit_x, 20, "POWERED BY",
                                 gray, black, FontType::Font3x5);
        FontRenderer::drawString(display, credit_x, 31, "AMY",
                                 DisplayDriver::kColorAmber, black, FontType::Font5x7, 2);
    }

    const int16_t footer_y = dh - 17;
    FontRenderer::drawString(display, 12, footer_y, "TOQUE UMA TECLA",
                             gray, black, FontType::Font3x5);
    if (dw > 160) {
        constexpr const char* interfaces = "USB MIDI / I2S AUDIO";
        const int16_t text_width = FontRenderer::stringWidth(interfaces, FontType::Font3x5);
        FontRenderer::drawString(display, dw - 12 - text_width, footer_y, interfaces,
                                 gray, black, FontType::Font3x5);
    }

    // Time-to-Home indicator only; it does not represent hardware readiness.
    const int16_t track_width = dw - 24;
    display.drawHLine(12, dh - 7, track_width, DisplayDriver::kColorDarkGray);
    const int16_t fill_width = static_cast<int16_t>(track_width * progress);
    if (fill_width > 0) display.drawHLine(12, dh - 7, fill_width, cyan);
}

} // namespace smk
