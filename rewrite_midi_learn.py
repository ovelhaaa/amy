import re
with open('smk-s3/components/ui/screens/midi_learn_screen.cpp', 'r') as f:
    content = f.read()

square_logic = """
    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        using namespace theme;

        char state_str[20];
        if (is_learning) {
            snprintf(state_str, sizeof(state_str), "%02u/%02u", cur_step, total_steps);
        } else {
            snprintf(state_str, sizeof(state_str), "%s", is_complete ? "SAVED" : "READY");
        }
        components::HeaderWidget::draw(display, "MIDI LEARN", state_str,
                                       false, false, ColorAccentSecondary);

        constexpr int16_t box_w = 200;
        constexpr int16_t box_h = 140;
        const int16_t box_x = (dw - box_w) / 2;
        const int16_t box_y = kHeaderHeight + 20;
        display.fillChamferRect(box_x, box_y, box_w, box_h, 4, ColorSurfaceElev);

        if (is_learning) {
            FontRenderer::drawString(display, box_x + 10, box_y + 10, "TARGET:",
                                     ColorTextSecondary, ColorSurfaceElev,
                                     FontType::Font5x7, 1);
            const char* step_name = midi_learn_->currentStepName();
            FontRenderer::drawString(display, box_x + 10, box_y + 24,
                                     step_name ? step_name : "", ColorTextPrimary,
                                     ColorSurfaceElev, FontType::Font5x7, 1);

            display.drawHLine(box_x + 10, box_y + 40, box_w - 20, ColorDivider);
            const char* step_hint = midi_learn_->currentStepHint();
            FontRenderer::drawString(display, box_x + 10, box_y + 50,
                                     step_hint ? step_hint : "", ColorAccentSecondary,
                                     ColorSurfaceElev, FontType::Font3x5, 1);

            const float progress = total_steps > 0
                ? static_cast<float>(cur_step) / total_steps : 0.0f;
            display.drawRect(box_x + 10, box_y + 72, box_w - 20, 8, ColorDivider);
            const int16_t fill_w =
                static_cast<int16_t>((box_w - 22) * std::min(1.0f, progress));
            if (fill_w > 0) {
                display.fillRect(box_x + 11, box_y + 73, fill_w, 6,
                                 ColorAccentPrimary);
            }

            if (feedback_active) {
                FontRenderer::drawString(display, box_x + 10, box_y + 96,
                                         feedback_msg_, feedback_color_,
                                         ColorSurfaceElev, FontType::Font5x7, 1);
            } else {
                FontRenderer::drawString(display, box_x + 10, box_y + 96,
                                         "WAITING FOR MIDI...", ColorTextMuted,
                                         ColorSurfaceElev, FontType::Font5x7, 1);
            }
        } else {
            FontRenderer::drawString(display, box_x + 10, box_y + 28,
                                     is_complete ? "MAPPING COMPLETE" : "PRESS PLAY TO START",
                                     is_complete ? ColorStatePlay : ColorAccentSecondary,
                                     ColorSurfaceElev, FontType::Font5x7, 1);
            FontRenderer::drawString(display, box_x + 10, box_y + 56,
                                     "KEYS / KNOBS / PADS / TRANSPORT",
                                     ColorTextSecondary, ColorSurfaceElev,
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
    print("Skipping smk-s3/components/ui/screens/midi_learn_screen.cpp - could not find layout block")
    raise SystemExit(0)

new_content = content[:start_idx] + square_logic.strip() + '\n    ' + content[end_idx:]

with open('smk-s3/components/ui/screens/midi_learn_screen.cpp', 'w') as f:
    f.write(new_content)
