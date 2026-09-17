import re

with open('smk-s3/components/ui/screens/sequencer_screen.cpp', 'r') as f:
    content = f.read()

square_logic = """
    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        using namespace theme;

        char subtitle[32];
        snprintf(subtitle, sizeof(subtitle), "PAT %d", current_pattern_);
        components::HeaderWidget::draw(display, "SEQUENCER", subtitle, false, false, ColorAccentPrimary);

        // Transport state
        int16_t transport_y = kHeaderHeight + 6;
        if (is_playing_) {
            display.fillChamferRect(dw - 36, transport_y, 24, 12, 2, is_recording_ ? ColorStateRecord : ColorStatePlay);
            FontRenderer::drawChar(display, dw - 28, transport_y + 3, is_recording_ ? 'R' : 'P', ColorBackground, is_recording_ ? ColorStateRecord : ColorStatePlay, FontType::Font5x7, 1);
        }

        int16_t grid_y = kHeaderHeight + 24;
        int16_t step_w = 12;
        int16_t step_h = 16;
        int16_t step_gap = 2;
        int16_t track_gap = 4;

        // 4 Tracks x 16 Steps
        for (int track = 0; track < 4; ++track) {
            int16_t ty = grid_y + track * (step_h + track_gap);

            // Track header/mute
            bool muted = track_muted_[track];
            bool selected = (track == selected_track_);

            uint16_t track_color = selected ? ColorAccentPrimary : ColorTextSecondary;
            if (muted) track_color = ColorStateMuted;

            char t_str[4];
            snprintf(t_str, sizeof(t_str), "T%d", track + 1);
            FontRenderer::drawString(display, 4, ty + 4, t_str, track_color, ColorBackground, FontType::Font5x7, 1);

            if (selected) {
                display.drawVLine(2, ty, step_h, ColorAccentPrimary);
            }
            if (muted) {
                display.drawHLine(4, ty + 7, 12, ColorStateMuted); // strike-through
            }

            // Steps
            int16_t start_x = 24;
            for (int step = 0; step < 16; ++step) {
                // Group by 4 visually by adding a small gap
                int16_t group_gap = (step / 4) * 4;
                int16_t sx = start_x + step * (step_w + step_gap) + group_gap;

                bool active = step_active_[track][step];
                bool is_current = (step == current_step_ && is_playing_);
                bool has_plock = step_has_plock_[track][step];

                uint16_t bg_color = ColorSurface;
                if (active) bg_color = selected ? ColorAccentPrimary : ColorTextMuted;
                if (muted && active) bg_color = ColorStateMuted;

                if (is_current) {
                    display.fillRect(sx, ty, step_w, step_h, ColorTextPrimary);
                } else {
                    display.fillChamferRect(sx, ty, step_w, step_h, 1, bg_color);

                    // P-lock indicator
                    if (has_plock) {
                        display.drawHLine(sx + 2, ty + step_h - 3, step_w - 4, ColorAccentSecondary);
                    }
                }
            }
        }

        // Bottom: 8 pads
        int16_t pads_y = grid_y + 4 * (step_h + track_gap) + 12;
        display.drawHLine(0, pads_y - 6, dw, ColorDivider);

        int16_t pad_w = 26;
        int16_t pad_h = 26;
        int16_t px_start = (dw - (8 * pad_w + 7 * 2)) / 2;
        if (px_start < 2) px_start = 2; // safety

        for (int i = 0; i < 8; ++i) {
            int16_t px = px_start + i * (pad_w + 2);
            bool pad_active = (hit_frames_[i] > 0);

            display.fillChamferRect(px, pads_y, pad_w, pad_h, 2, pad_active ? ColorAccentPrimary : ColorSurfaceElev);
            char p_str[2];
            snprintf(p_str, sizeof(p_str), "%d", i+1);
            FontRenderer::drawString(display, px + 8, pads_y + 10, p_str, pad_active ? ColorBackground : ColorTextMuted, pad_active ? ColorAccentPrimary : ColorSurfaceElev, FontType::Font5x7, 1);
        }
"""

start_idx = content.find('if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {')
end_idx = content.find('} else if (dw <= 160) {')

if '#include "ui_theme.h"' not in content:
    content = '#include "ui_theme.h"\n#include "ui_components.h"\n' + content

new_content = content[:start_idx] + square_logic.strip() + '\n    } else if (dw <= 160) {' + content[end_idx + 23:]

with open('smk-s3/components/ui/screens/sequencer_screen.cpp', 'w') as f:
    f.write(new_content)
