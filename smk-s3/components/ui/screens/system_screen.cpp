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

    auto snap = Diagnostics::instance().takeSnapshot();

    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        using namespace theme;
        components::HeaderWidget::draw(display, "SYSTEM", "",
                                       false, false, ColorAccentSecondary);

        char cpu_value[32];
        char memory_value[32];
        char audio_value[32];
        char midi_value[32];
        char usb_value[32];
        char control_value[32];
        snprintf(cpu_value, sizeof(cpu_value), "%luMHz / %.1f%% / V%lu",
                 static_cast<unsigned long>(snap.cpu_freq_mhz), snap.render_load,
                 static_cast<unsigned long>(snap.active_voices));
        snprintf(memory_value, sizeof(memory_value), "%luK / %.1fM",
                 static_cast<unsigned long>(snap.free_internal_ram / 1024),
                 snap.free_psram / (1024.0f * 1024.0f));
        snprintf(audio_value, sizeof(audio_value), "UND %lu / MAX %.1fms",
                 static_cast<unsigned long>(snap.audio_underruns),
                 snap.max_render_us / 1000.0f);
        snprintf(midi_value, sizeof(midi_value), "ERR %lu / Q %lu",
                 static_cast<unsigned long>(snap.midi_parse_errors),
                 static_cast<unsigned long>(snap.event_queue_overflows));
        snprintf(usb_value, sizeof(usb_value), "%s / DC %lu",
                 snap.usb_connected ? "CONNECTED" : "NO DEVICE",
                 static_cast<unsigned long>(snap.usb_disconnects));
        snprintf(control_value, sizeof(control_value), "%.10s / SW %u / %s",
                 vel_curve_, swing_, limiter_ ? "LIM" : "NO LIM");

        const char* item_names_[] = {
            "ENGINE", "MEMORY", "AUDIO", "MIDI", "USB", "CONTROL"
        };
        const char* item_values_[] = {
            cpu_value, memory_value, audio_value, midi_value, usb_value, control_value
        };
        const size_t row_count = sizeof(item_names_) / sizeof(item_names_[0]);

        constexpr int16_t box_x = kMargin;
        const int16_t box_y = kHeaderHeight + kMargin;
        const int16_t box_w = dw - 2 * kMargin;
        constexpr int16_t row_h = 30;
        display.fillChamferRect(box_x, box_y, box_w,
                                static_cast<int16_t>(row_count * row_h + 8),
                                4, ColorSurfaceElev);

        for (size_t i = 0; i < row_count; ++i) {
            const int16_t y = box_y + 8 + static_cast<int16_t>(i * row_h);
            FontRenderer::drawString(display, box_x + 8, y, item_names_[i],
                                     ColorTextSecondary, ColorSurfaceElev,
                                     FontType::Font3x5, 1);
            FontRenderer::drawString(display, box_x + 72, y, item_values_[i],
                                     ColorTextPrimary, ColorSurfaceElev,
                                     FontType::Font5x7, 1);
            if (i + 1 < row_count) {
                display.drawHLine(box_x + 8, y + 20, box_w - 16, ColorDivider);
            }
        }
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
