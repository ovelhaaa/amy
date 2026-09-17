#pragma once

#include "display_driver.h"
#include "font_renderer.h"
#include "widgets.h"
#include "ui_theme.h"
#include <cstdio>

namespace smk {
namespace components {

class HeaderWidget {
public:
    static void draw(DisplayDriver& display, const char* title, const char* subtitle,
                     bool midi_active, bool usb_active, uint16_t theme_color) {
        using namespace theme;
        // Background
        display.fillRect(0, 0, kDisplayWidth, kHeaderHeight, ColorHeader);
        display.drawHLine(0, kHeaderHeight - 1, kDisplayWidth, ColorDivider);

        // Title
        FontRenderer::drawString(display, kMargin, kMargin - 1, title, ColorTextPrimary, ColorHeader, FontType::Font5x7, 1);
        if (subtitle && subtitle[0] != '\0') {
            int16_t title_w = FontRenderer::stringWidth(title, FontType::Font5x7, 1);
            FontRenderer::drawString(display, kMargin + title_w + 6, kMargin - 1, subtitle, theme_color, ColorHeader, FontType::Font5x7, 1);
        }

        // Status badges
        int16_t badge_y = 6;
        int16_t badge_w = 12;
        int16_t badge_h = 10;
        int16_t badge_x = kDisplayWidth - kMargin - badge_w;

        // USB Badge
        uint16_t usb_color = usb_active ? ColorStatePlay : ColorTextMuted;
        display.fillChamferRect(badge_x, badge_y, badge_w, badge_h, 2, usb_color);
        FontRenderer::drawChar(display, badge_x + 3, badge_y + 3, 'U', usb_active ? ColorBackground : ColorSurfaceElev, usb_color, FontType::Font3x5, 1);

        badge_x -= (badge_w + 4);

        // MIDI Badge
        uint16_t midi_color = midi_active ? ColorStateWarning : ColorSurfaceElev;
        display.fillChamferRect(badge_x, badge_y, badge_w, badge_h, 2, midi_color);
        FontRenderer::drawChar(display, badge_x + 3, badge_y + 3, 'M', midi_active ? ColorBackground : ColorTextMuted, midi_color, FontType::Font3x5, 1);
    }
};

class MacroTile {
public:
    static void draw(DisplayDriver& display, int16_t x, int16_t y, const char* label,
                     ParametricGlyph glyph, uint8_t value, uint16_t theme_color) {
        using namespace theme;
        display.fillChamferRect(x, y, kMacroTileWidth, kMacroTileHeight, 2, ColorSurface);

        // Label
        FontRenderer::drawString(display, x + 4, y + 4, label, ColorTextSecondary, ColorSurface, FontType::Font3x5, 1);

        // Glyph
        GlyphRenderer::drawGlyph(display, x + 4, y + 16, glyph, theme_color);

        // Value
        char val_str[8];
        snprintf(val_str, sizeof(val_str), "%u", value);
        int16_t val_w = FontRenderer::stringWidth(val_str, FontType::Font5x7, 1);
        FontRenderer::drawString(display, x + kMacroTileWidth - val_w - 4, y + 20, val_str, ColorTextPrimary, ColorSurface, FontType::Font5x7, 1);

        // Progress Bar (minimalist)
        int16_t bar_w = kMacroTileWidth - 8;
        display.drawHLine(x + 4, y + 34, bar_w, ColorDivider);
        if (value > 0) {
            int16_t fill_w = (value * bar_w) / 127;
            display.drawHLine(x + 4, y + 34, fill_w, theme_color);
            display.drawHLine(x + 4, y + 35, fill_w, theme_color); // 2px thick
        }
    }
};

} // namespace components
} // namespace smk
