import re

squares = {
    'splash_screen.cpp': """
    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        using namespace theme;
        display.fillScreen(ColorBackground);

        uint16_t logo_color = ColorAccentPrimary;
        uint16_t text_color = ColorTextPrimary;
        uint16_t dim_text = ColorTextMuted;

        // Draw monogram / logo mark
        int16_t mark_y = 60;
        int16_t mark_size = 40;
        int16_t mark_x = (dw - mark_size) / 2;

        display.drawRect(mark_x, mark_y, mark_size, mark_size, logo_color);
        display.drawRect(mark_x+1, mark_y+1, mark_size-2, mark_size-2, logo_color);
        display.fillRect(mark_x + 8, mark_y + 8, 12, 12, logo_color);
        display.fillRect(mark_x + 20, mark_y + 20, 12, 12, logo_color);

        const int16_t title_width = FontRenderer::stringWidth("SMK-S3", FontType::FontDisplay, 1);
        FontRenderer::drawString(display, (dw - title_width) / 2, 114, "SMK-S3", text_color, ColorBackground, FontType::FontDisplay, 1);

        const int16_t credit_width = FontRenderer::stringWidth("POWERED BY AMY", FontType::Font3x5, 1);
        FontRenderer::drawString(display, (dw - credit_width) / 2, 146, "POWERED BY AMY", ColorAccentSecondary, ColorBackground, FontType::Font3x5, 1);

        const int16_t ready_width = FontRenderer::stringWidth("USB MIDI / I2S AUDIO", FontType::Font3x5, 1);
        FontRenderer::drawString(display, (dw - ready_width) / 2, 210, "USB MIDI / I2S AUDIO", dim_text, ColorBackground, FontType::Font3x5, 1);
""",
    'parameter_screen.cpp': """
    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        using namespace theme;
        const bool is_captured = takeover_ == TakeoverStatus::Captured;

        // Darkened background for overlay
        display.fillRect(0, 0, dw, dh, ColorBackground); // simplified since dimColor not available globally

        // Container box
        int16_t box_w = 200;
        int16_t box_h = 160;
        int16_t box_x = (dw - box_w) / 2;
        int16_t box_y = (dh - box_h) / 2;

        display.fillChamferRect(box_x, box_y, box_w, box_h, 4, ColorSurfaceElev);
        display.drawChamferRect(box_x, box_y, box_w, box_h, 4, is_captured ? ColorAccentPrimary : ColorSurface);

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
            snprintf(val_buf, sizeof(val_buf), "%d %s", (int)current_val_, unit_str_);
        } else {
            snprintf(val_buf, sizeof(val_buf), "%d", (int)current_val_);
        }

        int16_t val_w = FontRenderer::stringWidth(val_buf, FontType::FontDisplay, 1);
        uint16_t val_color = is_captured ? ColorTextPrimary : ColorTextSecondary;
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
            display.fillChamferRect(bar_x, bar_y, fill_w, bar_h, 2, is_captured ? ColorAccentPrimary : ColorDivider);
        }

        // Saved/Preset indicator
        int16_t saved_x = bar_x + (saved_val_ * bar_w) / 127;
        display.drawVLine(saved_x, bar_y - 4, bar_h + 8, ColorAccentSecondary);
        display.drawPixel(saved_x - 1, bar_y - 4, ColorAccentSecondary);
        display.drawPixel(saved_x + 1, bar_y - 4, ColorAccentSecondary);

        // Takeover guidance
        if (!is_captured) {
            char takeover_buf[32];
            snprintf(takeover_buf, sizeof(takeover_buf), "%s TO CAPTURE", current_val_ < saved_val_ ? "TURN ->" : "<- TURN");
            int16_t t_w = FontRenderer::stringWidth(takeover_buf, FontType::Font3x5, 1);
            FontRenderer::drawString(display, box_x + (box_w - t_w) / 2, box_y + 116, takeover_buf, ColorStateWarning, ColorSurfaceElev, FontType::Font3x5, 1);
        } else {
            int16_t s_w = FontRenderer::stringWidth("SAVED POSITION", FontType::Font3x5, 1);
            FontRenderer::drawString(display, box_x + (box_w - s_w) / 2, box_y + 116, "SAVED POSITION", ColorTextMuted, ColorSurfaceElev, FontType::Font3x5, 1);

            // Draw small saved value below indicator
            char sv_buf[8];
            snprintf(sv_buf, sizeof(sv_buf), "%d", (int)saved_val_);
            int16_t sv_w = FontRenderer::stringWidth(sv_buf, FontType::Font3x5, 1);
            FontRenderer::drawString(display, saved_x - sv_w/2, bar_y + 12, sv_buf, ColorAccentSecondary, ColorSurfaceElev, FontType::Font3x5, 1);
        }
"""
}

for filepath, logic in squares.items():
    with open('smk-s3/components/ui/screens/' + filepath, 'r') as f:
        content = f.read()

    start_idx = content.find('if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {')
    end_idx = content.find('} else if (dw <= 160) {')

    if start_idx == -1 or end_idx == -1:
        continue

    prefix = content[:start_idx]
    suffix = content[end_idx:]

    new_content = prefix + logic.strip() + '\n    ' + suffix

    if '#include "ui_theme.h"' not in new_content:
        new_content = '#include "ui_theme.h"\n#include "ui_components.h"\n' + new_content

    with open('smk-s3/components/ui/screens/' + filepath, 'w') as f:
        f.write(new_content)
