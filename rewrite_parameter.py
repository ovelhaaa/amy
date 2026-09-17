import re

with open('smk-s3/components/ui/screens/parameter_screen.cpp', 'r') as f:
    content = f.read()

square_logic = """
    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        using namespace theme;

        // Darkened background for overlay
        display.fillRect(0, 0, dw, dh, dimColor(ColorBackground, 0.9f));

        // Container box
        int16_t box_w = 200;
        int16_t box_h = 160;
        int16_t box_x = (dw - box_w) / 2;
        int16_t box_y = (dh - box_h) / 2;

        display.fillChamferRect(box_x, box_y, box_w, box_h, 4, ColorSurfaceElev);
        display.drawChamferRect(box_x, box_y, box_w, box_h, 4, is_captured_ ? ColorAccentPrimary : ColorSurface);

        // Parameter name
        char title_upper[32];
        snprintf(title_upper, sizeof(title_upper), "%s", param_name_);
        for (char* p = title_upper; *p; ++p) {
            if (*p >= 'a' && *p <= 'z') *p -= 32;
        }

        int16_t title_w = FontRenderer::stringWidth(title_upper, FontType::Font5x7, 1);
        FontRenderer::drawString(display, box_x + (box_w - title_w) / 2, box_y + 16, title_upper, ColorTextSecondary, ColorSurfaceElev, FontType::Font5x7, 1);
        display.drawHLine(box_x + 20, box_y + 30, box_w - 40, ColorDivider);

        // Current Value
        char val_buf[16];
        if (unit_str_[0] != '\\0') {
            snprintf(val_buf, sizeof(val_buf), "%u %s", current_val_, unit_str_);
        } else {
            snprintf(val_buf, sizeof(val_buf), "%u", current_val_);
        }

        int16_t val_w = FontRenderer::stringWidth(val_buf, FontType::FontDisplay, 1);
        uint16_t val_color = is_captured_ ? ColorTextPrimary : ColorTextSecondary;
        FontRenderer::drawString(display, box_x + (box_w - val_w) / 2, box_y + 46, val_buf, val_color, ColorSurfaceElev, FontType::FontDisplay, 1);

        // Visual Bar
        int16_t bar_w = box_w - 40;
        int16_t bar_h = 8;
        int16_t bar_x = box_x + 20;
        int16_t bar_y = box_y + 90;

        display.fillChamferRect(bar_x, bar_y, bar_w, bar_h, 2, ColorSurface);

        // Fill portion
        int16_t fill_w = (current_val_ * bar_w) / 127;
        if (fill_w > 0) {
            display.fillChamferRect(bar_x, bar_y, fill_w, bar_h, 2, is_captured_ ? ColorAccentPrimary : ColorDivider);
        }

        // Saved/Preset indicator
        int16_t saved_x = bar_x + (saved_val_ * bar_w) / 127;
        display.drawVLine(saved_x, bar_y - 4, bar_h + 8, ColorAccentSecondary);
        display.drawPixel(saved_x - 1, bar_y - 4, ColorAccentSecondary);
        display.drawPixel(saved_x + 1, bar_y - 4, ColorAccentSecondary);

        // Takeover guidance
        if (!is_captured_) {
            char takeover_buf[32];
            snprintf(takeover_buf, sizeof(takeover_buf), "%s TO CAPTURE", current_val_ < saved_val_ ? "TURN ->" : "<- TURN");
            int16_t t_w = FontRenderer::stringWidth(takeover_buf, FontType::Font3x5, 1);
            FontRenderer::drawString(display, box_x + (box_w - t_w) / 2, box_y + 116, takeover_buf, ColorStateWarning, ColorSurfaceElev, FontType::Font3x5, 1);
        } else {
            int16_t s_w = FontRenderer::stringWidth("SAVED POSITION", FontType::Font3x5, 1);
            FontRenderer::drawString(display, box_x + (box_w - s_w) / 2, box_y + 116, "SAVED POSITION", ColorTextMuted, ColorSurfaceElev, FontType::Font3x5, 1);

            // Draw small saved value below indicator
            char sv_buf[8];
            snprintf(sv_buf, sizeof(sv_buf), "%u", saved_val_);
            int16_t sv_w = FontRenderer::stringWidth(sv_buf, FontType::Font3x5, 1);
            FontRenderer::drawString(display, saved_x - sv_w/2, bar_y + 12, sv_buf, ColorAccentSecondary, ColorSurfaceElev, FontType::Font3x5, 1);
        }
"""

start_idx = content.find('if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {')
end_idx = content.find('} else if (dw <= 160) {')

if '#include "ui_theme.h"' not in content:
    content = '#include "ui_theme.h"\n' + content

new_content = content[:start_idx] + square_logic.strip() + '\n    } else if (dw <= 160) {' + content[end_idx + 23:]

with open('smk-s3/components/ui/screens/parameter_screen.cpp', 'w') as f:
    f.write(new_content)
