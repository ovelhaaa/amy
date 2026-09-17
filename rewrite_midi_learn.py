import re
with open('smk-s3/components/ui/screens/midi_learn_screen.cpp', 'r') as f:
    content = f.read()

square_logic = """
    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        using namespace theme;
        components::HeaderWidget::draw(display, "MIDI LEARN", state_str, false, false, ColorAccentSecondary);

        int16_t box_y = kHeaderHeight + 20;
        int16_t box_w = 200;
        int16_t box_h = 140;
        int16_t box_x = (dw - box_w) / 2;

        display.fillChamferRect(box_x, box_y, box_w, box_h, 4, ColorSurfaceElev);

        // Target parameter
        FontRenderer::drawString(display, box_x + 10, box_y + 10, "TARGET:", ColorTextSecondary, ColorSurfaceElev, FontType::Font5x7, 1);
        FontRenderer::drawString(display, box_x + 10, box_y + 24, target_name_, ColorTextPrimary, ColorSurfaceElev, FontType::Font5x7, 1);

        display.drawHLine(box_x + 10, box_y + 40, box_w - 20, ColorDivider);

        // CC mapping
        FontRenderer::drawString(display, box_x + 10, box_y + 50, "MAPPED CC:", ColorTextSecondary, ColorSurfaceElev, FontType::Font5x7, 1);
        char cc_buf[16];
        if (mapped_cc_ != 255) {
            snprintf(cc_buf, sizeof(cc_buf), "%u", mapped_cc_);
        } else {
            snprintf(cc_buf, sizeof(cc_buf), "NONE");
        }
        FontRenderer::drawString(display, box_x + 10, box_y + 64, cc_buf, ColorAccentPrimary, ColorSurfaceElev, FontType::FontDisplay, 1);

        // Instructions
        FontRenderer::drawString(display, box_x + 10, box_y + 110, "TURN HARDWARE KNOB", ColorTextMuted, ColorSurfaceElev, FontType::Font3x5, 1);
        FontRenderer::drawString(display, box_x + 10, box_y + 120, "OR SEND MIDI CC", ColorTextMuted, ColorSurfaceElev, FontType::Font3x5, 1);
"""

start_idx = content.find('if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {')
end_idx = content.find('} else if (dw <= 160) {')

if '#include "ui_theme.h"' not in content:
    content = '#include "ui_theme.h"\n#include "ui_components.h"\n' + content
new_content = content[:start_idx] + square_logic.strip() + '\n    } else if (dw <= 160) {' + content[end_idx + 23:]

with open('smk-s3/components/ui/screens/midi_learn_screen.cpp', 'w') as f:
    f.write(new_content)
