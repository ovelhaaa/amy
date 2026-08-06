#include "parameter_screen.h"
#include "font_renderer.h"
#include "esp_timer.h"
#include <cstdio>
#include <cstring>

namespace smk {

ParameterScreen::ParameterScreen()
    : progress_bar_(12, 48, 260, 16, DisplayDriver::kColorGreen, DisplayDriver::kColorWhite) {
}

void ParameterScreen::onEnter() {
    resetTimer();
    expired_ = false;
}

void ParameterScreen::resetTimer() {
    active_start_ms_ = (uint32_t)(esp_timer_get_time() / 1000);
}

bool ParameterScreen::isExpired() const {
    return expired_;
}

void ParameterScreen::showParameter(const char* name, const char* target_layer,
                                     float current_val, float saved_val, 
                                     const char* unit_str, TakeoverStatus takeover) {
    if (name) snprintf(param_name_, sizeof(param_name_), "%s", name);
    if (target_layer) snprintf(target_layer_, sizeof(target_layer_), "%s", target_layer);
    if (unit_str) snprintf(unit_str_, sizeof(unit_str_), "%s", unit_str);

    current_val_ = current_val;
    saved_val_ = saved_val;
    takeover_ = takeover;

    float norm = current_val / 127.0f; // Default assumption for 0..127 range
    progress_bar_.setValue(norm);
    resetTimer();
    expired_ = false;
}

void ParameterScreen::update() {
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    if (now - active_start_ms_ >= timeout_ms_) {
        expired_ = true;
    }
}

void ParameterScreen::render(DisplayDriver& display) {
    // Draw semi-transparent overlay border box
    display.fillRect(4, 4, DisplayDriver::kWidth - 8, DisplayDriver::kHeight - 8, DisplayDriver::kColorBlack);
    display.drawRect(4, 4, DisplayDriver::kWidth - 8, DisplayDriver::kHeight - 8, DisplayDriver::kColorCyan);

    // Title line: PARAMETER (LAYER)
    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), "%s (%s)", param_name_, target_layer_);
    FontRenderer::drawString(display, 10, 8, title_buf, DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);

    // Soft takeover status indicator
    const char* takeover_str = "[CAPTURED *]";
    uint16_t takeover_color = DisplayDriver::kColorGreen;

    switch (takeover_) {
        case TakeoverStatus::ApproachingFromBelow:
            takeover_str = "[TAKEOVER: <]";
            takeover_color = DisplayDriver::kColorYellow;
            break;
        case TakeoverStatus::ApproachingFromAbove:
            takeover_str = "[TAKEOVER: >]";
            takeover_color = DisplayDriver::kColorYellow;
            break;
        case TakeoverStatus::Decoupled:
            takeover_str = "[DECOUPLED]";
            takeover_color = DisplayDriver::kColorRed;
            break;
        case TakeoverStatus::Captured:
        default:
            break;
    }

    FontRenderer::drawString(display, 190, 8, takeover_str, takeover_color, DisplayDriver::kColorBlack, FontType::Font5x7);

    // Values line: Saved vs Current
    char val_buf[64];
    snprintf(val_buf, sizeof(val_buf), "Saved: %.2f %s   CURRENT: %.2f %s", 
             saved_val_, unit_str_, current_val_, unit_str_);
    FontRenderer::drawString(display, 10, 26, val_buf, DisplayDriver::kColorYellow, DisplayDriver::kColorBlack, FontType::Font5x7);

    // Render Progress Bar
    progress_bar_.draw(display);
}

} // namespace smk
