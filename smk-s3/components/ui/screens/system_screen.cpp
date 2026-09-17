#include "ui_theme.h"
#include "ui_components.h"
#include "system_screen.h"
#include "display_layout.h"
#include "font_renderer.h"
#include "diagnostics.h"
#include <cstdio>

namespace smk {

void SystemScreen::update() {
    // Polled on refresh
}

void SystemScreen::render(DisplayDriver& display) {
    display.fillScreen(DisplayDriver::kColorBlack);
    int16_t dw = display.width();
    int16_t dh = display.height();

    auto snaif (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        using namespace theme;
        components::HeaderWidget::draw(display, "SYSTEM", "", false, false, ColorAccentSecondary);

        int16_t box_w = 210;
        int16_t box_h = 180;
        int16_t box_x = (dw - box_w) / 2;
        int16_t box_y = kHeaderHeight + kMargin;

        display.fillChamferRect(box_x, box_y, box_w, box_h, 4, ColorSurfaceElev);

        for (int i = 0; i < 7; ++i) {
            int16_t y = box_y + 12 + i * 22;
            bool selected = (i == selected_item_);

            if (selected) {
                display.fillChamferRect(box_x + 4, y - 4, box_w - 8, 20, 2, ColorSurface);
                display.drawVLine(box_x + 6, y - 2, 16, ColorAccentPrimary);
            }

            FontRenderer::drawString(display, box_x + 16, y, item_names_[i], selected ? ColorTextPrimary : ColorTextSecondary, selected ? ColorSurface : ColorSurfaceElev, FontType::Font5x7, 1);
            FontRenderer::drawString(display, box_x + 130, y, item_values_[i], selected ? ColorAccentPrimary : ColorTextMuted, selected ? ColorSurface : ColorSurfaceElev, FontType::Font5x7, 1);
        }
    } else if (dw <= 160) {#include "ui_components.h"
#include "system_screen.h"
#include "display_layout.h"
#include "font_renderer.h"
#include "diagnostics.h"
#include <cstdio>

namespace smk {

void SystemScreen::update() {
    // Polled on refresh
}

void SystemScreen::render(DisplayDriver& display) {
    display.fillScreen(DisplayDriver::kColorBlack);
    int16_t dw = display.width();
    int16_t dh = display.height();

    auto snap = Diagnostics::instance().takeSnapshot();

    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        display.fillRect(0, 0, dw, 30, DisplayDriver::kColorDarkGray);
        FontRenderer::drawString(display, 4, 4, "SYSTEM", DisplayDriver::kColorCyan,
                                 DisplayDriver::kColorDarkGray, FontType::Font5x7, 2);
        char version[24];
        snprintf(version, sizeof(version), "FW %s", snap.firmware_version ? snap.firmware_version : "0.1.0");
        const int16_t version_w = FontRenderer::stringWidth(version, FontType::Font3x5);
        FontRenderer::drawString(display, dw - version_w - 5, 21, version,
                                 DisplayDriver::kColorLightGray, DisplayDriver::kColorDarkGray,
                                 FontType::Font3x5);

        auto draw_card = [&display](int16_t x, int16_t y, const char* label,
                                    const char* line1, const char* line2, uint16_t color) {
            constexpr int16_t card_w = 113;
            constexpr int16_t card_h = 50;
            display.drawRect(x, y, card_w, card_h, DisplayDriver::kColorDarkGray);
            FontRenderer::drawString(display, x + 5, y + 5, label, color,
                                     DisplayDriver::kColorBlack, FontType::Font3x5);
            FontRenderer::drawString(display, x + 5, y + 18, line1, DisplayDriver::kColorWhite,
                                     DisplayDriver::kColorBlack, FontType::Font5x7);
            FontRenderer::drawString(display, x + 5, y + 34, line2, DisplayDriver::kColorLightGray,
                                     DisplayDriver::kColorBlack, FontType::Font3x5);
        };

        char line1[32];
        char line2[32];
        snprintf(line1, sizeof(line1), "%s", snap.usb_connected ? "CONNECTED" : "NO DEVICE");
        snprintf(line2, sizeof(line2), "DC %lu  RC %lu", snap.usb_disconnects, snap.usb_reconnects);
        draw_card(4, 36, "USB MIDI", line1, line2,
                  snap.usb_connected ? DisplayDriver::kColorGreen : DisplayDriver::kColorRed);

        snprintf(line1, sizeof(line1), "UNDERRUN %lu", snap.audio_underruns);
        snprintf(line2, sizeof(line2), "AVG %.1f  MAX %.1fms", snap.avg_render_us / 1000.0f,
                 snap.max_render_us / 1000.0f);
        draw_card(123, 36, "AUDIO", line1, line2,
                  snap.audio_underruns == 0 ? DisplayDriver::kColorGreen : DisplayDriver::kColorRed);

        snprintf(line1, sizeof(line1), "INT %luK", static_cast<unsigned long>(snap.free_internal_ram / 1024));
        snprintf(line2, sizeof(line2), "PSRAM %.1fM", snap.free_psram / (1024.0f * 1024.0f));
        draw_card(4, 92, "MEMORY", line1, line2, DisplayDriver::kColorCyan);

        snprintf(line1, sizeof(line1), "CPU %luMHz", snap.cpu_freq_mhz);
        snprintf(line2, sizeof(line2), "DSP %.1f%%  V %lu", snap.render_load, snap.active_voices);
        draw_card(123, 92, "ENGINE", line1, line2, DisplayDriver::kColorAmber);

        snprintf(line1, sizeof(line1), "ERROR %lu", snap.midi_parse_errors);
        snprintf(line2, sizeof(line2), "QUEUE %lu", snap.event_queue_overflows);
        draw_card(4, 148, "EVENTS", line1, line2,
                  (snap.midi_parse_errors + snap.event_queue_overflows) == 0
                      ? DisplayDriver::kColorGreen : DisplayDriver::kColorRed);

        snprintf(line1, sizeof(line1), "PANIC %lu", snap.panic_count);
        snprintf(line2, sizeof(line2), "VEL %.10s SW %u", vel_curve_, swing_);
        draw_card(123, 148, "CONTROL", line1, line2, DisplayDriver::kColorYellow);

        display.drawHLine(0, 207, dw, DisplayDriver::kColorDarkGray);
        FontRenderer::drawString(display, 4, 216, "PAD B3: HOME  B5/B6: PAGES",
                                 DisplayDriver::kColorLightGray, DisplayDriver::kColorBlack,
                                 FontType::Font3x5);
        return;
    }

    if (dw <= 160) {
        FontRenderer::drawString(display, 2, 2, "SYSTEM DIAGNOSTICS", DisplayDriver::kColorCyan, DisplayDriver::kColorBlack, FontType::Font5x7);
        display.drawHLine(0, 11, dw, DisplayDriver::kColorMidGray);

        char buf[64];
        snprintf(buf, sizeof(buf), "CPU:%luMHz V:%02lu/12", snap.cpu_freq_mhz, snap.active_voices);
        FontRenderer::drawString(display, 2, 14, buf, DisplayDriver::kColorYellow, DisplayDriver::kColorBlack, FontType::Font5x7);

        snprintf(buf, sizeof(buf), "DSP:%.1f%% UNDR:%lu", snap.render_load, snap.audio_underruns);
        FontRenderer::drawString(display, 2, 25, buf, snap.audio_underruns > 0 ? DisplayDriver::kColorRed : DisplayDriver::kColorGreen, DisplayDriver::kColorBlack, FontType::Font5x7);

        snprintf(buf, sizeof(buf), "VEL:%s SW:%u%%", vel_curve_, swing_);
        FontRenderer::drawString(display, 2, 36, buf, DisplayDriver::kColorAmber, DisplayDriver::kColorBlack, FontType::Font5x7);

        snprintf(buf, sizeof(buf), "LIMITER: %s", limiter_ ? "ON (SOFT-KNEE)" : "DISABLED");
        FontRenderer::drawString(display, 2, 47, buf, limiter_ ? DisplayDriver::kColorGreen : DisplayDriver::kColorRed, DisplayDriver::kColorBlack, FontType::Font5x7);

        snprintf(buf, sizeof(buf), "IRAM:%luK PSRAM:%.1fM", (unsigned long)(snap.free_internal_ram / 1024), (float)snap.free_psram / (1024.0f * 1024.0f));
        FontRenderer::drawString(display, 2, 58, buf, DisplayDriver::kColorGreen, DisplayDriver::kColorBlack, FontType::Font5x7);

        snprintf(buf, sizeof(buf), "USB: %s", snap.usb_connected ? "SMK25 CONNECTED" : "NO DEVICE");
        FontRenderer::drawString(display, 2, 69, buf, snap.usb_connected ? DisplayDriver::kColorCyan : DisplayDriver::kColorMidGray, DisplayDriver::kColorBlack, FontType::Font5x7);

        snprintf(buf, sizeof(buf), "RNDR: %.1fms / %.1fms", (float)snap.avg_render_us / 1000.0f, (float)snap.max_render_us / 1000.0f);
        FontRenderer::drawString(display, 2, 80, buf, DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);

        snprintf(buf, sizeof(buf), "MIDI ERR:%lu PANIC:%lu", snap.midi_parse_errors, snap.panic_count);
        FontRenderer::drawString(display, 2, 91, buf, DisplayDriver::kColorLightGray, DisplayDriver::kColorBlack, FontType::Font5x7);

        display.drawHLine(0, 106, dw, DisplayDriver::kColorMidGray);
        FontRenderer::drawString(display, 2, 110, "SMK-S3 (1.8\" ST7735)", DisplayDriver::kColorMidGray, DisplayDriver::kColorBlack, FontType::Font5x7);
        return;
    }

    // Title line
    FontRenderer::drawString(display, 2, 2, "SYSTEM DIAGNOSTICS", DisplayDriver::kColorCyan, DisplayDriver::kColorBlack, FontType::Font5x7);
    char ver_str[32];
    snprintf(ver_str, sizeof(ver_str), "FW v%s (ESP32-S3)", snap.firmware_version ? snap.firmware_version : "0.1.0");
    FontRenderer::drawString(display, 170, 2, ver_str, DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);
    display.drawHLine(0, 11, dw, DisplayDriver::kColorMidGray);

    // Row 1: CPU, DSP Load, Voices, Underruns
    char r1[64];
    snprintf(r1, sizeof(r1), "CPU: %luMHz | DSP LOAD: %.1f%% | VOICES: %02lu/12 | UNDERRUNS: %06lu",
             snap.cpu_freq_mhz, snap.render_load, snap.active_voices, snap.audio_underruns);
    FontRenderer::drawString(display, 2, 16, r1, DisplayDriver::kColorYellow, DisplayDriver::kColorBlack, FontType::Font5x7);

    // Row 2: Int RAM, PSRAM, USB State
    char r2[64];
    snprintf(r2, sizeof(r2), "INT RAM: %lu KB | PSRAM: %.1f MB | USB: %s",
             (unsigned long)(snap.free_internal_ram / 1024), (float)snap.free_psram / (1024.0f * 1024.0f),
             snap.usb_connected ? "HOST CONNECTED" : "DISCONNECTED");
    FontRenderer::drawString(display, 2, 32, r2, DisplayDriver::kColorGreen, DisplayDriver::kColorBlack, FontType::Font5x7);

    // Row 3: Max Render, Avg Render, MIDI errors
    char r3[64];
    snprintf(r3, sizeof(r3), "MAX RNDR: %.1fms | AVG RNDR: %.1fms | MIDI ERR: %04lu | PANICS: %02lu",
             (float)snap.max_render_us / 1000.0f, (float)snap.avg_render_us / 1000.0f,
             snap.midi_parse_errors, snap.panic_count);
    FontRenderer::drawString(display, 2, 48, r3, DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);

    // Bottom border line
    display.drawHLine(0, 64, dw, DisplayDriver::kColorMidGray);
    FontRenderer::drawString(display, 2, 66, "PAD B3: HOME | PAD B5/B6: PAGES", DisplayDriver::kColorLightGray, DisplayDriver::kColorBlack, FontType::Font5x7);
}

} // namespace smk
