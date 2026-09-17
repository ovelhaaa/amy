import re

with open('smk-s3/components/ui/screens/parameter_screen.cpp', 'r') as f:
    content = f.read()

square_logic = """
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
        if (unit_str_[0] != '\\0') {
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
"""

include_prefix = ""
if '#include "ui_theme.h"' not in content:
    include_prefix += '#include "ui_theme.h"\n'
if '#include "ui_components.h"' not in content:
    include_prefix += '#include "ui_components.h"\n'
if include_prefix:
    content = include_prefix + content

start_idx = content.find('if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {')
end_idx = content.find('} else if (dw <= 160) {')

if start_idx == -1 or end_idx == -1:
    raise RuntimeError("Could not find layout markers in smk-s3/components/ui/screens/parameter_screen.cpp")

new_content = content[:start_idx] + square_logic.strip() + '\n    ' + content[end_idx:]

with open('smk-s3/components/ui/screens/parameter_screen.cpp', 'w') as f:
    f.write(new_content)
