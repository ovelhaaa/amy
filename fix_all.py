import os
import re

files_to_fix = [
    'parameter_screen.cpp',
    'pad_screen.cpp',
    'scene_screen.cpp',
    'sequencer_screen.cpp',
    'midi_learn_screen.cpp',
    'midi_monitor_screen.cpp',
    'system_screen.cpp'
]

squares = {
    'parameter_screen.cpp': """
    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        using namespace theme;
        const bool is_captured = takeover_ == TakeoverStatus::Captured;

        // Darkened background for overlay
        display.fillRect(0, 0, dw, dh, dimColor(ColorBackground, 0.9f));

        // Container box
        int16_t box_w = 200;
        int16_t box_h = 160;
        int16_t box_x = (dw - box_w) / 2;
        int16_t box_y = (dh - box_h) / 2;

        display.fillChamferRect(box_x, box_y, box_w, box_h, 4, ColorSurfaceElev);
        display.drawChamferRect(box_x, box_y, box_w, box_h, 4, is_captured ? ColorAccentPrimary : ColorSurface);

        // Parameter name
        char title_upper[32];
        snprintf(title_upper, sizeof(title_upper), "%s", param_name_);
        for (char* p = title_upper; *p; ++p) {
            if (*p >= 'a' && *p <= 'z') *p -= 32;
        }

        int16_t title_w = FontRenderer::stringWidth(title_upper, FontType::Font5x7, 1);
        FontRenderer::drawString(display, box_x + (box_w - title_w) / 2, box_y + 16, title_upper, ColorTextSecondary, ColorSurfaceElev, FontType::Font5x7, 1);
        display.drawHLine(box_x + 20, box_y + 30, box_w - 40, ColorDivider);

        // Current Value
        char val_buf[16];
        if (unit_str_[0] != '\\0') {
            snprintf(val_buf, sizeof(val_buf), "%d %s", (int)current_val_, unit_str_);
        } else {
            snprintf(val_buf, sizeof(val_buf), "%d", (int)current_val_);
        }

        int16_t val_w = FontRenderer::stringWidth(val_buf, FontType::FontDisplay, 1);
        uint16_t val_color = is_captured ? ColorTextPrimary : ColorTextSecondary;
        FontRenderer::drawString(display, box_x + (box_w - val_w) / 2, box_y + 46, val_buf, val_color, ColorSurfaceElev, FontType::FontDisplay, 1);

        // Visual Bar
        int16_t bar_w = box_w - 40;
        int16_t bar_h = 8;
        int16_t bar_x = box_x + 20;
        int16_t bar_y = box_y + 90;

        display.fillChamferRect(bar_x, bar_y, bar_w, bar_h, 2, ColorSurface);

        // Fill portion
        int16_t fill_w = (current_val_ * bar_w) / 127;
        if (fill_w > 0) {
            display.fillChamferRect(bar_x, bar_y, fill_w, bar_h, 2, is_captured ? ColorAccentPrimary : ColorDivider);
        }

        // Saved/Preset indicator
        int16_t saved_x = bar_x + (saved_val_ * bar_w) / 127;
        display.drawVLine(saved_x, bar_y - 4, bar_h + 8, ColorAccentSecondary);
        display.drawPixel(saved_x - 1, bar_y - 4, ColorAccentSecondary);
        display.drawPixel(saved_x + 1, bar_y - 4, ColorAccentSecondary);

        // Takeover guidance
        if (!is_captured) {
            char takeover_buf[32];
            snprintf(takeover_buf, sizeof(takeover_buf), "%s TO CAPTURE", current_val_ < saved_val_ ? "TURN ->" : "<- TURN");
            int16_t t_w = FontRenderer::stringWidth(takeover_buf, FontType::Font3x5, 1);
            FontRenderer::drawString(display, box_x + (box_w - t_w) / 2, box_y + 116, takeover_buf, ColorStateWarning, ColorSurfaceElev, FontType::Font3x5, 1);
        } else {
            int16_t s_w = FontRenderer::stringWidth("SAVED POSITION", FontType::Font3x5, 1);
            FontRenderer::drawString(display, box_x + (box_w - s_w) / 2, box_y + 116, "SAVED POSITION", ColorTextMuted, ColorSurfaceElev, FontType::Font3x5, 1);

            // Draw small saved value below indicator
            char sv_buf[8];
            snprintf(sv_buf, sizeof(sv_buf), "%d", (int)saved_val_);
            int16_t sv_w = FontRenderer::stringWidth(sv_buf, FontType::Font3x5, 1);
            FontRenderer::drawString(display, saved_x - sv_w/2, bar_y + 12, sv_buf, ColorAccentSecondary, ColorSurfaceElev, FontType::Font3x5, 1);
        }
""",
    'pad_screen.cpp': """
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
""",
    'scene_screen.cpp': """
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
""",
    'sequencer_screen.cpp': """
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
        int16_t step_w = 10;
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
                int16_t group_gap = (step / 4) * 2;
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
""",
    'midi_learn_screen.cpp': """
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
""",
    'midi_monitor_screen.cpp': """
    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        using namespace theme;
        components::HeaderWidget::draw(display, "MIDI MONITOR", "", false, false, ColorAccentSecondary);

        int16_t start_y = kHeaderHeight + kMargin;
        display.fillChamferRect(kMargin, start_y, dw - 2*kMargin, dh - start_y - kMargin, 4, ColorSurfaceElev);

        for (int i = 0; i < 12; ++i) {
            int16_t y = start_y + 8 + i * 14;
            FontRenderer::drawString(display, kMargin + 8, y, lines_[i], ColorTextPrimary, ColorSurfaceElev, FontType::Font5x7, 1);
        }
""",
    'system_screen.cpp': """
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
}

def inject(filepath, logic):
    with open('smk-s3/components/ui/screens/' + filepath, 'r') as f:
        content = f.read()

    # Find boundaries
    start_idx = content.find('if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {')
    end_idx = content.find('} else if (dw <= 160) {')

    if start_idx == -1 or end_idx == -1:
        print(f"Skipping {filepath} - could not find block")
        return

    # Replace block carefully
    prefix = content[:start_idx]
    suffix = content[end_idx:]

    new_content = prefix + logic.strip() + '\n    ' + suffix

    # Add includes right after namespace smk { if not present
    ns_idx = new_content.find('namespace smk {')
    if ns_idx != -1:
        inc = ""
        if '#include "ui_theme.h"' not in new_content:
            inc += '#include "ui_theme.h"\n'
        if '#include "ui_components.h"' not in new_content:
            inc += '#include "ui_components.h"\n'

        if inc:
            # We want to put includes BEFORE namespace smk {
            new_content = new_content[:ns_idx] + inc + new_content[ns_idx:]

    with open('smk-s3/components/ui/screens/' + filepath, 'w') as f:
        f.write(new_content)

for f, logic in squares.items():
    inject(f, logic)
