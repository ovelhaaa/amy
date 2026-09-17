import re

with open('smk-s3/components/ui/screens/pad_screen.cpp', 'r') as f:
    content = f.read()

square_logic = """
    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        using namespace theme;

        const char bank_char = observed_input_bank_ == 1 ? 'A'
            : (observed_input_bank_ == 2 ? 'B' : '?');
        char subtitle[32];
        snprintf(subtitle, sizeof(subtitle), "BANK %c", bank_char);
        const uint16_t theme_color = observed_input_bank_ == 2 ? ColorBankB : ColorBankA;
        components::HeaderWidget::draw(display, "PERFORMANCE PADS", subtitle,
                                       false, false, theme_color);

        constexpr int16_t pad_w = 48;
        constexpr int16_t pad_h = 76;
        const int16_t start_y = kHeaderHeight + kMargin + 16;
        const int16_t start_x = (dw - (4 * pad_w + 3 * kMargin)) / 2;

        for (uint8_t i = 0; i < 8; ++i) {
            const int16_t col = i % 4;
            const int16_t row = i / 4;
            const int16_t x = start_x + col * (pad_w + kMargin);
            const int16_t y = start_y + row * (pad_h + kMargin);
            const bool is_hit = hit_recent && last_hit_pad_ == i;
            const uint16_t bg_color = is_hit ? theme_color : ColorSurfaceElev;
            const uint16_t border_color = is_hit ? ColorTextPrimary : ColorDivider;

            display.fillChamferRect(x, y, pad_w, pad_h, 4, bg_color);
            display.drawChamferRect(x, y, pad_w, pad_h, 4, border_color);

            char idx_str[4];
            snprintf(idx_str, sizeof(idx_str), "%u", i + 1);
            FontRenderer::drawString(display, x + 4, y + 4, idx_str, ColorTextMuted,
                                     bg_color, FontType::Font3x5, 1);

            const char* label = labelForInputBank(observed_input_bank_, i, pad_labels_[i]);
            const int16_t label_w = FontRenderer::stringWidth(label, FontType::Font5x7, 1);
            FontRenderer::drawString(display, x + (pad_w - label_w) / 2, y + pad_h - 14,
                                     label, is_hit ? ColorBackground : ColorTextSecondary,
                                     bg_color, FontType::Font5x7, 1);

            if (is_hit) {
                const int16_t vel_h = (last_hit_vel_ * (pad_h - 24)) / 127;
                display.fillRect(x + 12, y + pad_h - 18 - vel_h, pad_w - 24,
                                 vel_h, ColorTextPrimary);
            }
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
    print("Skipping smk-s3/components/ui/screens/pad_screen.cpp - could not find layout block")
    raise SystemExit(0)

new_content = content[:start_idx] + square_logic.strip() + '\n    ' + content[end_idx:]

with open('smk-s3/components/ui/screens/pad_screen.cpp', 'w') as f:
    f.write(new_content)
