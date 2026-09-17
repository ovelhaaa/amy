import re
with open('smk-s3/components/ui/screens/system_screen.cpp', 'r') as f:
    content = f.read()

square_logic = """
    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
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
"""
start_idx = content.find('if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {')
end_idx = content.find('} else if (dw <= 160) {')

if '#include "ui_theme.h"' not in content:
    content = '#include "ui_theme.h"\n#include "ui_components.h"\n' + content
new_content = content[:start_idx] + square_logic.strip() + '\n    } else if (dw <= 160) {' + content[end_idx + 23:]

with open('smk-s3/components/ui/screens/system_screen.cpp', 'w') as f:
    f.write(new_content)
