import re

with open('smk-s3/components/ui/screens/pad_screen.cpp', 'r') as f:
    content = f.read()

square_logic = """
    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        using namespace theme;

        char subtitle[32];
        const char bank_char = active_bank_ == 1 ? 'A' : (active_bank_ == 2 ? 'B' : '?');
        snprintf(subtitle, sizeof(subtitle), "BANK %c", bank_char);

        uint16_t theme_color = active_bank_ == 2 ? ColorBankB : ColorBankA;
        components::HeaderWidget::draw(display, "PERFORMANCE PADS", subtitle, false, false, theme_color);

        // 2x4 Pads layout
        int16_t pad_w = 48;
        int16_t pad_h = 76;
        int16_t start_y = kHeaderHeight + kMargin + 16;
        int16_t start_x = (dw - (4 * pad_w + 3 * kMargin)) / 2;

        for (int i = 0; i < 8; ++i) {
            int16_t col = i % 4;
            int16_t row = i / 4;
            int16_t x = start_x + col * (pad_w + kMargin);
            int16_t y = start_y + row * (pad_h + kMargin);

            bool is_hit = (hit_frames_[i] > 0);
            uint16_t bg_color = is_hit ? dimColor(theme_color, 0.4f) : ColorSurfaceElev;
            uint16_t border_color = is_hit ? theme_color : ColorDivider;

            display.fillChamferRect(x, y, pad_w, pad_h, 4, bg_color);
            display.drawChamferRect(x, y, pad_w, pad_h, 4, border_color);

            // Pad index
            char idx_str[4];
            snprintf(idx_str, sizeof(idx_str), "%d", i + 1);
            FontRenderer::drawString(display, x + 4, y + 4, idx_str, ColorTextMuted, bg_color, FontType::Font3x5, 1);

            // Label
            const char* label = pad_labels_[i];
            if (!label || label[0] == '\\0') label = "---";

            int16_t label_w = FontRenderer::stringWidth(label, FontType::Font5x7, 1);
            FontRenderer::drawString(display, x + (pad_w - label_w) / 2, y + pad_h - 14, label, is_hit ? ColorTextPrimary : ColorTextSecondary, bg_color, FontType::Font5x7, 1);

            // Velocity feedback
            if (is_hit) {
                int16_t vel_h = (last_velocities_[i] * (pad_h - 24)) / 127;
                display.fillRect(x + 12, y + pad_h - 18 - vel_h, pad_w - 24, vel_h, theme_color);
            }
        }
"""

start_idx = content.find('if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {')
end_idx = content.find('} else if (dw <= 160) {')

if '#include "ui_theme.h"' not in content:
    content = '#include "ui_theme.h"\n#include "ui_components.h"\n' + content

new_content = content[:start_idx] + square_logic.strip() + '\n    } else if (dw <= 160) {' + content[end_idx + 23:]

with open('smk-s3/components/ui/screens/pad_screen.cpp', 'w') as f:
    f.write(new_content)
