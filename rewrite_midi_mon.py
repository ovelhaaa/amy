import re
with open('smk-s3/components/ui/screens/midi_monitor_screen.cpp', 'r') as f:
    content = f.read()

square_logic = """
    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        using namespace theme;
        components::HeaderWidget::draw(display, "MIDI MONITOR", "", false, false, ColorAccentSecondary);

        int16_t start_y = kHeaderHeight + kMargin;
        display.fillChamferRect(kMargin, start_y, dw - 2*kMargin, dh - start_y - kMargin, 4, ColorSurfaceElev);

        for (int i = 0; i < 12; ++i) {
            int16_t y = start_y + 8 + i * 14;
            FontRenderer::drawString(display, kMargin + 8, y, lines_[i], ColorTextPrimary, ColorSurfaceElev, FontType::Font5x7, 1);
        }
"""
start_idx = content.find('if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {')
end_idx = content.find('} else if (dw <= 160) {')

if '#include "ui_theme.h"' not in content:
    content = '#include "ui_theme.h"\n#include "ui_components.h"\n' + content
new_content = content[:start_idx] + square_logic.strip() + '\n    } else if (dw <= 160) {' + content[end_idx + 23:]

with open('smk-s3/components/ui/screens/midi_monitor_screen.cpp', 'w') as f:
    f.write(new_content)
