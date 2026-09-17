import re

with open('smk-s3/components/ui/screens/scene_screen.cpp', 'r') as f:
    content = f.read()

square_logic = """
    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        using namespace theme;
        components::HeaderWidget::draw(display, "SCENES", "1-8",
                                       false, false, ColorAccentPrimary);

        constexpr int16_t scene_w = 98;
        constexpr int16_t scene_h = 42;
        const int16_t start_x = (dw - (2 * scene_w + kMargin)) / 2;
        const int16_t start_y = kHeaderHeight + 20;

        for (uint8_t i = 0; i < 8; ++i) {
            const int16_t col = i % 2;
            const int16_t row = i / 2;
            const int16_t x = start_x + col * (scene_w + kMargin);
            const int16_t y = start_y + row * (scene_h + kMargin);
            const bool is_active = i == active_scene_index_;
            const bool is_queued = i == pending_scene_index_;

            uint16_t bg_color = ColorSurface;
            uint16_t border_color = ColorDivider;
            if (is_active) {
                bg_color = ColorAccentPrimary;
                border_color = ColorTextPrimary;
            } else if (is_queued) {
                border_color = ColorAccentSecondary;
            }

            display.fillChamferRect(x, y, scene_w, scene_h, 2, bg_color);
            display.drawChamferRect(x, y, scene_w, scene_h, 2, border_color);

            char num_str[4];
            snprintf(num_str, sizeof(num_str), "%02u", i + 1);
            FontRenderer::drawString(display, x + 6, y + 6, num_str,
                                     is_active ? ColorBackground
                                               : (is_queued ? ColorAccentSecondary
                                                            : ColorTextSecondary),
                                     bg_color, FontType::Font5x7, 1);
            FontRenderer::drawString(display, x + 26, y + 6, scene_names_[i],
                                     is_active ? ColorBackground : ColorTextSecondary,
                                     bg_color, FontType::Font5x7, 1);

            char meta_str[32];
            snprintf(meta_str, sizeof(meta_str), "P%03u  PAT %u",
                     scene_patches_[i], scene_patterns_[i] + 1);
            FontRenderer::drawString(display, x + 6, y + 24, meta_str,
                                     is_active ? ColorSurface : ColorTextMuted,
                                     bg_color, FontType::Font3x5, 1);
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
    print("Skipping smk-s3/components/ui/screens/scene_screen.cpp - could not find layout block")
    raise SystemExit(0)

new_content = content[:start_idx] + square_logic.strip() + '\n    ' + content[end_idx:]

with open('smk-s3/components/ui/screens/scene_screen.cpp', 'w') as f:
    f.write(new_content)
