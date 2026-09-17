import re

with open('smk-s3/components/ui/screens/scene_screen.cpp', 'r') as f:
    content = f.read()

square_logic = """
    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        using namespace theme;

        components::HeaderWidget::draw(display, "SCENES", "1-8", false, false, ColorAccentPrimary);

        int16_t scene_w = 98;
        int16_t scene_h = 42;
        int16_t start_x = (dw - (2 * scene_w + kMargin)) / 2;
        int16_t start_y = kHeaderHeight + 20;

        for (int i = 0; i < 8; ++i) {
            int16_t col = i % 2;
            int16_t row = i / 2;
            int16_t x = start_x + col * (scene_w + kMargin);
            int16_t y = start_y + row * (scene_h + kMargin);

            bool is_active = (i == active_scene_);
            bool is_queued = (i == queued_scene_);

            uint16_t bg_color = ColorSurface;
            uint16_t border_color = ColorDivider;

            if (is_active) {
                bg_color = dimColor(ColorAccentPrimary, 0.2f);
                border_color = ColorAccentPrimary;
            } else if (is_queued) {
                border_color = ColorAccentSecondary;
            }

            display.fillChamferRect(x, y, scene_w, scene_h, 2, bg_color);
            display.drawChamferRect(x, y, scene_w, scene_h, 2, border_color);

            // Queue animation pulse indicator
            if (is_queued && (elapsed_ms_ % 500) < 250) {
                display.drawChamferRect(x - 1, y - 1, scene_w + 2, scene_h + 2, 3, ColorAccentSecondary);
            }

            // Number
            char num_str[4];
            snprintf(num_str, sizeof(num_str), "%02d", i + 1);
            FontRenderer::drawString(display, x + 6, y + 6, num_str, is_active ? ColorAccentPrimary : (is_queued ? ColorAccentSecondary : ColorTextSecondary), bg_color, FontType::Font5x7, 1);

            // Name
            const char* name = scenes_[i].name;
            if (!name || name[0] == '\\0') name = "EMPTY";
            FontRenderer::drawString(display, x + 26, y + 6, name, is_active ? ColorTextPrimary : ColorTextSecondary, bg_color, FontType::Font5x7, 1);

            // Metadata (Patch / Pattern)
            char meta_str[32];
            snprintf(meta_str, sizeof(meta_str), "P%03u  PAT %u", scenes_[i].patch_number, scenes_[i].pattern_number);
            FontRenderer::drawString(display, x + 6, y + 24, meta_str, ColorTextMuted, bg_color, FontType::Font3x5, 1);
        }
"""

start_idx = content.find('if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {')
end_idx = content.find('} else if (dw <= 160) {')

if '#include "ui_theme.h"' not in content:
    content = '#include "ui_theme.h"\n#include "ui_components.h"\n' + content

new_content = content[:start_idx] + square_logic.strip() + '\n    } else if (dw <= 160) {' + content[end_idx + 23:]

with open('smk-s3/components/ui/screens/scene_screen.cpp', 'w') as f:
    f.write(new_content)
