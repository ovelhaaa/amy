#include "ui_theme.h"
#include "ui_components.h"
#include "parameter_screen.h"
#include "display_layout.h"
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
    int16_t dw = display.width();
    int16_t dh = display.height();

    // Draw overlay border box
    display.fillRect(4, 4, dw - 8, dh - 8, DisplayDriver::kColorBlack);
    display.drawRect(4, 4, dw - 8, dh - 8, DisplayDriver::kColorCyan);

    // Title line: PARAMETER (LAYER)
    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), "%s (%s)", param_name_, target_layer_);
    FontRenderer::drawString(display, 8, 8, title_buf, DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);

    // Soft takeover status indicator
    const char* takeover_str = "[LOCKED *]";
    const char* guide_hint = "CAPTURED";
    uint16_t takeover_color = DisplayDriver::kColorGreen;

    switch (takeover_) {
        case TakeoverStatus::ApproachingFromBelow:
            takeover_str = "[TAKEOVER: <]";
            guide_hint = "TURN RIGHT > TO CAPTURE";
            takeover_color = DisplayDriver::kColorYellow;
            break;
        case TakeoverStatus::ApproachingFromAbove:
            takeover_str = "[TAKEOVER: >]";
            guide_hint = "TURN LEFT < TO CAPTURE";
            takeover_color = DisplayDriver::kColorYellow;
            break;
        case TakeoverStatus::Decoupled:
            takeover_str = "[DECOUPLED]";
            guide_hint = "TURN TO MATCH PRESET";
            takeover_color = DisplayDriver::kColorRed;
            break;
        case TakeoverStatus::Captured:
        default:
            break;
    }

    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        using namespace theme;
        const bool is_captured = takeover_ == TakeoverStatus::Captured;

        display.fillRect(0, 0, dw, dh, ColorBackground);

        constexpr int16_t box_w = 200;
        constexpr int16_t box_h = 160;
        const int16_t box_x = (dw - box_w) / 2;
        const int16_t box_y = (dh - box_h) / 2;

        display.fillChamferRect(box_x, box_y, box_w, box_h, 4, ColorSurfaceElev);
        display.drawChamferRect(box_x, box_y, box_w, box_h, 4,
                                is_captured ? ColorAccentPrimary : ColorSurface);

        char title_upper[32];
        snprintf(title_upper, sizeof(title_upper), "%s", param_name_);
        for (char* p = title_upper; *p; ++p) {
            if (*p >= 'a' && *p <= 'z') *p -= 32;
        }

        const int16_t title_w = FontRenderer::stringWidth(title_upper, FontType::Font5x7, 1);
        FontRenderer::drawString(display, box_x + (box_w - title_w) / 2, box_y + 16,
                                 title_upper, ColorTextSecondary, ColorSurfaceElev,
                                 FontType::Font5x7, 1);
        display.drawHLine(box_x + 20, box_y + 30, box_w - 40, ColorDivider);

        char val_buf[24];
        if (unit_str_[0] != '\0') {
            snprintf(val_buf, sizeof(val_buf), "%.1f %s", current_val_, unit_str_);
        } else {
            snprintf(val_buf, sizeof(val_buf), "%.1f", current_val_);
        }
        const int16_t val_w = FontRenderer::stringWidth(val_buf, FontType::FontDisplay, 1);
        const uint16_t val_color = is_captured ? ColorTextPrimary : ColorTextSecondary;
        FontRenderer::drawString(display, box_x + (box_w - val_w) / 2, box_y + 46,
                                 val_buf, val_color, ColorSurfaceElev,
                                 FontType::FontDisplay, 1);

        constexpr int16_t bar_w = box_w - 40;
        constexpr int16_t bar_h = 8;
        const int16_t bar_x = box_x + 20;
        const int16_t bar_y = box_y + 90;
        display.fillChamferRect(bar_x, bar_y, bar_w, bar_h, 2, ColorSurface);

        const float current_norm = std::max(0.0f, std::min(1.0f, current_val_ / 127.0f));
        const int16_t fill_w = static_cast<int16_t>(current_norm * bar_w);
        if (fill_w > 0) {
            display.fillChamferRect(bar_x, bar_y, fill_w, bar_h, 2,
                                    is_captured ? ColorAccentPrimary : ColorDivider);
        }

        const float saved_norm = std::max(0.0f, std::min(1.0f, saved_val_ / 127.0f));
        const int16_t saved_x = bar_x + static_cast<int16_t>(saved_norm * bar_w);
        display.drawVLine(saved_x, bar_y - 4, bar_h + 8, ColorAccentSecondary);
        display.drawPixel(saved_x - 1, bar_y - 4, ColorAccentSecondary);
        display.drawPixel(saved_x + 1, bar_y - 4, ColorAccentSecondary);

        if (!is_captured) {
            char takeover_buf[32];
            snprintf(takeover_buf, sizeof(takeover_buf), "%s TO CAPTURE",
                     current_val_ < saved_val_ ? "TURN ->" : "<- TURN");
            const int16_t takeover_w =
                FontRenderer::stringWidth(takeover_buf, FontType::Font3x5, 1);
            FontRenderer::drawString(display, box_x + (box_w - takeover_w) / 2,
                                     box_y + 116, takeover_buf, ColorStateWarning,
                                     ColorSurfaceElev, FontType::Font3x5, 1);
        } else {
            const int16_t saved_label_w =
                FontRenderer::stringWidth("SAVED POSITION", FontType::Font3x5, 1);
            FontRenderer::drawString(display, box_x + (box_w - saved_label_w) / 2,
                                     box_y + 116, "SAVED POSITION", ColorTextMuted,
                                     ColorSurfaceElev, FontType::Font3x5, 1);
            char saved_buf[16];
            snprintf(saved_buf, sizeof(saved_buf), "%.1f", saved_val_);
            const int16_t saved_w =
                FontRenderer::stringWidth(saved_buf, FontType::Font3x5, 1);
            FontRenderer::drawString(display, saved_x - saved_w / 2, bar_y + 12,
                                     saved_buf, ColorAccentSecondary, ColorSurfaceElev,
                                     FontType::Font3x5, 1);
        }
        return;
    }
    if (dw <= 160) {
        // ── 160x128 Compact Centered HUD Modal (x=8, y=20, w=144, h=88) ──
        int16_t mx = 8;
        int16_t my = 20;
        int16_t mw = 144;
        int16_t mh = 88;

        display.fillRect(mx, my, mw, mh, DisplayDriver::kColorBlack);
        display.drawRect(mx, my, mw, mh, DisplayDriver::kColorCyan);

        // Header Title
        char title_buf[48];
        snprintf(title_buf, sizeof(title_buf), "%s (%s)", param_name_, target_layer_);
        FontRenderer::drawString(display, mx + 6, my + 6, title_buf, DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);

        // Takeover Badge
        int16_t tw = FontRenderer::stringWidth(takeover_str, FontType::Font5x7);
        FontRenderer::drawString(display, mx + mw - tw - 6, my + 18, takeover_str, takeover_color, DisplayDriver::kColorBlack, FontType::Font5x7);

        // Large Value
        char val_buf[32];
        snprintf(val_buf, sizeof(val_buf), "%.1f %s", current_val_, unit_str_);
        FontRenderer::drawString(display, mx + 6, my + 30, val_buf, DisplayDriver::kColorCyan, DisplayDriver::kColorBlack, FontType::Font8x12);

        // Saved Value
        char saved_buf[32];
        snprintf(saved_buf, sizeof(saved_buf), "SAVED: %.1f %s", saved_val_, unit_str_);
        FontRenderer::drawString(display, mx + 6, my + 46, saved_buf, DisplayDriver::kColorLightGray, DisplayDriver::kColorBlack, FontType::Font3x5);

        // Slider Bar with Ghost Needle
        int16_t bx = mx + 6;
        int16_t by = my + 54;
        int16_t bw = mw - 12;
        int16_t bh = 14;
        display.drawRect(bx, by, bw, bh, DisplayDriver::kColorMidGray);

        float cur_norm = std::max(0.0f, std::min(1.0f, current_val_ / 127.0f));
        int16_t fill_w = (int16_t)((bw - 2) * cur_norm);
        if (fill_w > 0) {
            display.fillRect(bx + 1, by + 1, fill_w, bh - 2, DisplayDriver::kColorCyan);
        }
        int16_t empty_w = (bw - 2) - fill_w;
        if (empty_w > 0) {
            display.fillRect(bx + 1 + fill_w, by + 1, empty_w, bh - 2, DisplayDriver::kColorBlack);
        }

        // Ghost Needle for Saved Value
        float save_norm = std::max(0.0f, std::min(1.0f, saved_val_ / 127.0f));
        int16_t gx = bx + 1 + (int16_t)((bw - 2) * save_norm);
        display.drawVLine(gx, by - 2, bh + 4, DisplayDriver::kColorYellow);

        // Direction Guidance Hint
        FontRenderer::drawString(display, mx + 6, my + 74, guide_hint, takeover_color, DisplayDriver::kColorBlack, FontType::Font3x5);
    } else {
        // ── 284x76 Widescreen Centered HUD Modal (x=46, y=8, w=192, h=60) ──
        int16_t mx = 46;
        int16_t my = 8;
        int16_t mw = 192;
        int16_t mh = 60;

        display.fillRect(mx, my, mw, mh, DisplayDriver::kColorBlack);
        display.drawRect(mx, my, mw, mh, DisplayDriver::kColorCyan);

        // Title Line
        char title_buf[48];
        snprintf(title_buf, sizeof(title_buf), "%s (%s)", param_name_, target_layer_);
        FontRenderer::drawString(display, mx + 8, my + 6, title_buf, DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);

        // Soft Takeover Badge
        int16_t tw = FontRenderer::stringWidth(takeover_str, FontType::Font5x7);
        FontRenderer::drawString(display, mx + mw - tw - 8, my + 6, takeover_str, takeover_color, DisplayDriver::kColorBlack, FontType::Font5x7);

        // Value Display
        char val_buf[32];
        snprintf(val_buf, sizeof(val_buf), "%.1f %s", current_val_, unit_str_);
        FontRenderer::drawString(display, mx + 8, my + 18, val_buf, DisplayDriver::kColorCyan, DisplayDriver::kColorBlack, FontType::Font8x12);

        char saved_buf[32];
        snprintf(saved_buf, sizeof(saved_buf), "(SAVED: %.1f %s)  %s", saved_val_, unit_str_, guide_hint);
        FontRenderer::drawString(display, mx + 90, my + 21, saved_buf, DisplayDriver::kColorLightGray, DisplayDriver::kColorBlack, FontType::Font3x5);

        // Slider Bar with Ghost Needle
        int16_t bx = mx + 8;
        int16_t by = my + 36;
        int16_t bw = mw - 16;
        int16_t bh = 15;
        display.drawRect(bx, by, bw, bh, DisplayDriver::kColorMidGray);

        float cur_norm = std::max(0.0f, std::min(1.0f, current_val_ / 127.0f));
        int16_t fill_w = (int16_t)((bw - 2) * cur_norm);
        if (fill_w > 0) {
            display.fillRect(bx + 1, by + 1, fill_w, bh - 2, DisplayDriver::kColorCyan);
        }
        int16_t empty_w = (bw - 2) - fill_w;
        if (empty_w > 0) {
            display.fillRect(bx + 1 + fill_w, by + 1, empty_w, bh - 2, DisplayDriver::kColorBlack);
        }

        // Ghost Needle for Saved Value
        float save_norm = std::max(0.0f, std::min(1.0f, saved_val_ / 127.0f));
        int16_t gx = bx + 1 + (int16_t)((bw - 2) * save_norm);
        display.drawVLine(gx, by - 2, bh + 4, DisplayDriver::kColorYellow);
        display.drawPixel(gx - 1, by - 2, DisplayDriver::kColorYellow);
        display.drawPixel(gx + 1, by - 2, DisplayDriver::kColorYellow);
    }
}

} // namespace smk
