import re

with open('smk-s3/components/ui/screens/sequencer_screen.cpp', 'r') as f:
    content = f.read()

square_logic = """
    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        using namespace theme;

        char subtitle[32];
        snprintf(subtitle, sizeof(subtitle), "PAT %u", pattern_num_);
        components::HeaderWidget::draw(display, "SEQUENCER", subtitle,
                                       false, false, ColorAccentPrimary);

        const int16_t transport_y = kHeaderHeight + 6;
        if (is_playing_) {
            const uint16_t transport_color = is_recording_ ? ColorStateRecord : ColorStatePlay;
            display.fillChamferRect(dw - 36, transport_y, 24, 12, 2, transport_color);
            FontRenderer::drawChar(display, dw - 28, transport_y + 3,
                                   is_recording_ ? 'R' : 'P', ColorBackground,
                                   transport_color, FontType::Font5x7, 1);
        }

        constexpr int16_t grid_y = kHeaderHeight + 24;
        constexpr int16_t step_w = 10;
        constexpr int16_t step_h = 16;
        constexpr int16_t step_gap = 2;
        constexpr int16_t track_gap = 4;

        for (uint8_t track = 0; track < 4; ++track) {
            const int16_t ty = grid_y + track * (step_h + track_gap);
            const bool muted = track_mutes_[track];
            const bool selected = track == selected_track_;
            const uint16_t track_color = muted ? ColorStateMuted
                : (selected ? ColorAccentPrimary : ColorTextSecondary);

            FontRenderer::drawString(display, 4, ty + 4, track_names_[track],
                                     track_color, ColorBackground,
                                     FontType::Font5x7, 1);
            if (selected) display.drawVLine(2, ty, step_h, ColorAccentPrimary);
            if (muted) display.drawHLine(4, ty + 7, 12, ColorStateMuted);

            constexpr int16_t start_x = 24;
            for (uint8_t step = 0; step < 16; ++step) {
                const int16_t group_gap = (step / 4) * 2;
                const int16_t sx = start_x + step * (step_w + step_gap) + group_gap;
                const bool active = (track_masks_[track] & (1U << step)) != 0;
                const bool current = step == current_step_ && is_playing_;
                const bool has_plock = (track_plock_masks_[track] & (1U << step)) != 0;

                uint16_t bg_color = ColorSurface;
                if (active) bg_color = selected ? ColorAccentPrimary : ColorTextMuted;
                if (muted && active) bg_color = ColorStateMuted;

                if (current) {
                    display.fillRect(sx, ty, step_w, step_h, ColorTextPrimary);
                } else {
                    display.fillChamferRect(sx, ty, step_w, step_h, 1, bg_color);
                    if (has_plock) {
                        display.drawHLine(sx + 2, ty + step_h - 3, step_w - 4,
                                          ColorAccentSecondary);
                    }
                }
            }
        }

        const int16_t pads_y = grid_y + 4 * (step_h + track_gap) + 12;
        display.drawHLine(0, pads_y - 6, dw, ColorDivider);
        constexpr int16_t pad_w = 26;
        constexpr int16_t pad_h = 26;
        int16_t px_start = (dw - (8 * pad_w + 7 * 2)) / 2;
        if (px_start < 2) px_start = 2;
        const uint8_t base_step = step_page_ * 8;

        for (uint8_t i = 0; i < 8; ++i) {
            const uint8_t step = base_step + i;
            const int16_t px = px_start + i * (pad_w + 2);
            const bool active = (track_masks_[selected_track_] & (1U << step)) != 0;
            const uint16_t pad_color = active ? ColorAccentPrimary : ColorSurfaceElev;
            display.fillChamferRect(px, pads_y, pad_w, pad_h, 2, pad_color);
            char pad_text[3];
            snprintf(pad_text, sizeof(pad_text), "%u", i + 1);
            FontRenderer::drawString(display, px + 8, pads_y + 10, pad_text,
                                     active ? ColorBackground : ColorTextMuted,
                                     pad_color, FontType::Font5x7, 1);
            if ((track_plock_masks_[selected_track_] & (1U << step)) != 0) {
                display.fillRect(px + pad_w - 5, pads_y + 3, 2, 2,
                                 ColorAccentSecondary);
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
    print("Skipping smk-s3/components/ui/screens/sequencer_screen.cpp - could not find layout block")
    raise SystemExit(0)

new_content = content[:start_idx] + square_logic.strip() + '\n    ' + content[end_idx:]

with open('smk-s3/components/ui/screens/sequencer_screen.cpp', 'w') as f:
    f.write(new_content)
