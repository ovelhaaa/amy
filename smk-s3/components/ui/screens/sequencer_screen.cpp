#include "ui_theme.h"
#include "ui_components.h"
#include "sequencer_screen.h"
#include "display_layout.h"
#include "font_renderer.h"
#include "clock_manager.h"
#include "step_sequencer.h"
#include "esp_timer.h"
#include <cstdio>
#include <cstring>

namespace smk {

SequencerScreen::SequencerScreen() {}

void SequencerScreen::setPatternNumber(uint8_t pat) { pattern_num_ = pat; }
void SequencerScreen::setBpm(float bpm) { bpm_ = bpm; }
void SequencerScreen::setSwing(uint8_t swing) { swing_ = swing; }

void SequencerScreen::setCurrentStep(uint8_t step) {
    current_step_ = step % 16;
}

void SequencerScreen::setTrackMask(uint8_t track_idx, uint16_t mask) {
    if (track_idx < 4) track_masks_[track_idx] = mask;
}

void SequencerScreen::setTrackMute(uint8_t track_idx, bool mute) {
    if (track_idx < 4) track_mutes_[track_idx] = mute;
}

void SequencerScreen::setTrackName(uint8_t track_idx, const char* name) {
    if (track_idx < 4 && name) {
        snprintf(track_names_[track_idx], sizeof(track_names_[track_idx]), "%s", name);
    }
}

void SequencerScreen::setTrackName(const char* name) {
    setTrackName(0, name);
}

void SequencerScreen::setTrackPlockMask(uint8_t track_idx, uint16_t mask) {
    if (track_idx < 4) track_plock_masks_[track_idx] = mask;
}

void SequencerScreen::update() {
    if (sequencer_) {
        pattern_num_ = sequencer_->currentPattern() + 1;
        if (clock_manager_) {
            bpm_ = clock_manager_->bpm();
            swing_ = clock_manager_->swing();
        } else {
            // Keep the sequencer's swing as a safe fallback until a clock is wired.
            swing_ = static_cast<uint8_t>(sequencer_->swing());
        }
        is_playing_ = sequencer_->isPlaying();
        is_recording_ = sequencer_->isRecording();
        selected_track_ = sequencer_->selectedTrack();
        step_page_ = sequencer_->stepPage();
        current_step_ = sequencer_->currentStep();

        const auto& chain = sequencer_->patternChain();
        chain_enabled_ = chain.enabled;
        chain_length_ = chain.length;
        chain_index_ = chain.current_index;
        for (uint8_t i = 0; i < chain_length_ && i < 16; ++i) {
            chain_patterns_[i] = chain.patterns[i];
        }

        for (uint8_t t = 0; t < 4; ++t) {
            track_masks_[t] = sequencer_->getTrackStepMask(t);
            track_plock_masks_[t] = sequencer_->getTrackPlockMask(t);
            track_mutes_[t] = sequencer_->isTrackMuted(t);
            snprintf(track_names_[t], sizeof(track_names_[t]), "%s", sequencer_->trackName(t));
        }
    }
}

namespace {
namespace compact_layout {
    constexpr int16_t kHeaderDividerY = 11;
    constexpr int16_t kTrackStartRowY = 14;
    constexpr int16_t kTrackRowH = 13;
    constexpr int16_t kMidDividerY = 70;
    constexpr int16_t kPadBoxY = 85;
    constexpr int16_t kPadBoxW = 17;
    constexpr int16_t kPadBoxH = 24;
}
namespace wide_layout {
    constexpr int16_t kHeaderDividerY = 12;
    constexpr int16_t kTrackStartRowY = 14;
    constexpr int16_t kTrackRowH = 10;
    constexpr int16_t kStepBoxW = 12;
    constexpr int16_t kStepBoxH = 8;
    constexpr int16_t kGridDividerY = 56;
    constexpr int16_t kPadMatrixY = 58;
    constexpr int16_t kPadBoxW = 20;
    constexpr int16_t kPadBoxH = 15;
    constexpr int16_t kPadPitchX = 23;
    constexpr int16_t kShortcutsX = 192;
}
} // anonymous namespace

void SequencerScreen::render(DisplayDriver& display) {
    display.fillScreen(DisplayDriver::kColorBlack);
    int16_t dw = display.if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
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
    } else if (dw <= 160) {#include "ui_components.h"
#include "sequencer_screen.h"
#include "display_layout.h"
#include "font_renderer.h"
#include "clock_manager.h"
#include "step_sequencer.h"
#include "esp_timer.h"
#include <cstdio>
#include <cstring>

namespace smk {

SequencerScreen::SequencerScreen() {}

void SequencerScreen::setPatternNumber(uint8_t pat) { pattern_num_ = pat; }
void SequencerScreen::setBpm(float bpm) { bpm_ = bpm; }
void SequencerScreen::setSwing(uint8_t swing) { swing_ = swing; }

void SequencerScreen::setCurrentStep(uint8_t step) {
    current_step_ = step % 16;
}

void SequencerScreen::setTrackMask(uint8_t track_idx, uint16_t mask) {
    if (track_idx < 4) track_masks_[track_idx] = mask;
}

void SequencerScreen::setTrackMute(uint8_t track_idx, bool mute) {
    if (track_idx < 4) track_mutes_[track_idx] = mute;
}

void SequencerScreen::setTrackName(uint8_t track_idx, const char* name) {
    if (track_idx < 4 && name) {
        snprintf(track_names_[track_idx], sizeof(track_names_[track_idx]), "%s", name);
    }
}

void SequencerScreen::setTrackName(const char* name) {
    setTrackName(0, name);
}

void SequencerScreen::setTrackPlockMask(uint8_t track_idx, uint16_t mask) {
    if (track_idx < 4) track_plock_masks_[track_idx] = mask;
}

void SequencerScreen::update() {
    if (sequencer_) {
        pattern_num_ = sequencer_->currentPattern() + 1;
        if (clock_manager_) {
            bpm_ = clock_manager_->bpm();
            swing_ = clock_manager_->swing();
        } else {
            // Keep the sequencer's swing as a safe fallback until a clock is wired.
            swing_ = static_cast<uint8_t>(sequencer_->swing());
        }
        is_playing_ = sequencer_->isPlaying();
        is_recording_ = sequencer_->isRecording();
        selected_track_ = sequencer_->selectedTrack();
        step_page_ = sequencer_->stepPage();
        current_step_ = sequencer_->currentStep();

        const auto& chain = sequencer_->patternChain();
        chain_enabled_ = chain.enabled;
        chain_length_ = chain.length;
        chain_index_ = chain.current_index;
        for (uint8_t i = 0; i < chain_length_ && i < 16; ++i) {
            chain_patterns_[i] = chain.patterns[i];
        }

        for (uint8_t t = 0; t < 4; ++t) {
            track_masks_[t] = sequencer_->getTrackStepMask(t);
            track_plock_masks_[t] = sequencer_->getTrackPlockMask(t);
            track_mutes_[t] = sequencer_->isTrackMuted(t);
            snprintf(track_names_[t], sizeof(track_names_[t]), "%s", sequencer_->trackName(t));
        }
    }
}

namespace {
namespace compact_layout {
    constexpr int16_t kHeaderDividerY = 11;
    constexpr int16_t kTrackStartRowY = 14;
    constexpr int16_t kTrackRowH = 13;
    constexpr int16_t kMidDividerY = 70;
    constexpr int16_t kPadBoxY = 85;
    constexpr int16_t kPadBoxW = 17;
    constexpr int16_t kPadBoxH = 24;
}
namespace wide_layout {
    constexpr int16_t kHeaderDividerY = 12;
    constexpr int16_t kTrackStartRowY = 14;
    constexpr int16_t kTrackRowH = 10;
    constexpr int16_t kStepBoxW = 12;
    constexpr int16_t kStepBoxH = 8;
    constexpr int16_t kGridDividerY = 56;
    constexpr int16_t kPadMatrixY = 58;
    constexpr int16_t kPadBoxW = 20;
    constexpr int16_t kPadBoxH = 15;
    constexpr int16_t kPadPitchX = 23;
    constexpr int16_t kShortcutsX = 192;
}
} // anonymous namespace

void SequencerScreen::render(DisplayDriver& display) {
    display.fillScreen(DisplayDriver::kColorBlack);
    int16_t dw = display.width();
    int16_t dh = display.height();

    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        const char* state = is_recording_ ? "REC" : (is_playing_ ? "PLAY" : "STOP");
        const uint16_t state_color = is_recording_ ? DisplayDriver::kColorRed
            : (is_playing_ ? DisplayDriver::kColorGreen : DisplayDriver::kColorMidGray);
        display.fillRect(0, 0, dw, 30, DisplayDriver::kColorDarkGray);
        char header[48];
        if (chain_enabled_ && chain_length_ > 0) {
            snprintf(header, sizeof(header), "CHAIN %u/%u  P%02u", chain_index_ + 1,
                     chain_length_, pattern_num_);
        } else {
            snprintf(header, sizeof(header), "SEQ P%02u  %.0f BPM", pattern_num_, bpm_);
        }
        FontRenderer::drawString(display, 4, 4, header, DisplayDriver::kColorWhite,
                                 DisplayDriver::kColorDarkGray, FontType::Font5x7, 2);
        const int16_t state_w = FontRenderer::stringWidth(state, FontType::Font5x7);
        display.fillRect(dw - state_w - 12, 19, state_w + 8, 9, state_color);
        FontRenderer::drawString(display, dw - state_w - 8, 20, state,
                                 DisplayDriver::kColorBlack, state_color, FontType::Font5x7);

        for (uint8_t s = 0; s < 16; ++s) {
            if ((s % 4) == 0) {
                char number[4];
                snprintf(number, sizeof(number), "%u", s + 1);
                FontRenderer::drawString(display, 31 + s * 12, 35, number,
                                         DisplayDriver::kColorLightGray, DisplayDriver::kColorBlack,
                                         FontType::Font3x5);
            }
        }

        for (uint8_t track = 0; track < 4; ++track) {
            const int16_t y = 47 + track * 27;
            const bool selected = track == selected_track_;
            const bool muted = track_mutes_[track];
            const uint16_t label_color = muted ? DisplayDriver::kColorRed
                : (selected ? DisplayDriver::kColorYellow : DisplayDriver::kColorWhite);
            FontRenderer::drawString(display, 4, y + 5, track_names_[track], label_color,
                                     DisplayDriver::kColorBlack, FontType::Font5x7);
            if (selected) {
                display.drawRect(2, y + 2, 24, 16, DisplayDriver::kColorCyan);
            }
            for (uint8_t step = 0; step < 16; ++step) {
                const int16_t x = 31 + step * 12;
                const bool active = (track_masks_[track] & (1U << step)) != 0;
                const bool locked = (track_plock_masks_[track] & (1U << step)) != 0;
                uint16_t step_color = DisplayDriver::kColorGreen;
                if (track == 0) step_color = DisplayDriver::kColorAmber;
                if (track == 1) step_color = DisplayDriver::kColorCyan;
                if (track == 3) step_color = DisplayDriver::kColorLightGray;
                if (muted) step_color = DisplayDriver::kColorDarkGray;
                if (active) {
                    display.fillRect(x, y, 9, 20, step_color);
                } else {
                    display.drawRect(x, y, 9, 20, (step % 4) == 0
                        ? DisplayDriver::kColorMidGray : DisplayDriver::kColorDarkGray);
                }
                if (locked) display.fillRect(x + 5, y + 2, 2, 3, DisplayDriver::kColorYellow);
                if (is_playing_ && step == current_step_) {
                    display.drawRect(x - 1, y - 1, 11, 22, DisplayDriver::kColorWhite);
                }
            }
        }

        display.drawHLine(0, 158, dw, DisplayDriver::kColorDarkGray);
        char edit[40];
        snprintf(edit, sizeof(edit), "EDIT %s  PAGE %u  SW %u%%", track_names_[selected_track_],
                 step_page_ + 1, swing_);
        FontRenderer::drawString(display, 4, 163, edit, DisplayDriver::kColorAmber,
                                 DisplayDriver::kColorBlack, FontType::Font5x7);
        const uint8_t base_step = step_page_ * 8;
        for (uint8_t pad = 0; pad < 8; ++pad) {
            const uint8_t step = base_step + pad;
            const int16_t x = 4 + pad * 29;
            constexpr int16_t y = 174;
            const bool active = (track_masks_[selected_track_] & (1U << step)) != 0;
            display.fillRect(x, y, 25, 28, active ? DisplayDriver::kColorDimGreen
                                                  : DisplayDriver::kColorBlack);
            display.drawRect(x, y, 25, 28, active ? DisplayDriver::kColorYellow
                                                  : DisplayDriver::kColorMidGray);
            char pad_text[4];
            snprintf(pad_text, sizeof(pad_text), "%02u", step + 1);
            FontRenderer::drawString(display, x + 6, y + 10, pad_text,
                                     active ? DisplayDriver::kColorYellow : DisplayDriver::kColorLightGray,
                                     active ? DisplayDriver::kColorDimGreen : DisplayDriver::kColorBlack,
                                     FontType::Font5x7);
            if ((track_plock_masks_[selected_track_] & (1U << step)) != 0) {
                display.fillRect(x + 19, y + 3, 3, 3, DisplayDriver::kColorAmber);
            }
        }
        FontRenderer::drawString(display, 4, 211, "PLAY/STOP: TRANSPORT  REC: RECORD",
                                 state_color, DisplayDriver::kColorBlack, FontType::Font3x5);
        FontRenderer::drawString(display, 4, 226, "PAD B1-B8: TRACK / PAGE / MUTE / CLEAR",
                                 DisplayDriver::kColorLightGray, DisplayDriver::kColorBlack,
                                 FontType::Font3x5);
        return;
    }

    // ── 160x128 (1.8" Display Layout) ──
    if (dw <= 160) {
        // Header
        const char* state_str = is_recording_ ? "[REC]" : (is_playing_ ? "[PLAY]" : "[STOP]");
        uint16_t state_col = is_recording_ ? DisplayDriver::kColorRed : (is_playing_ ? DisplayDriver::kColorGreen : DisplayDriver::kColorLightGray);

        char header_buf[48];
        if (chain_enabled_ && chain_length_ > 0) {
            snprintf(header_buf, sizeof(header_buf), "CHN %u/%u:P%02u %s", chain_index_ + 1, chain_length_, pattern_num_, state_str);
        } else {
            snprintf(header_buf, sizeof(header_buf), "SEQ P%02u %s %.0fBPM", pattern_num_, state_str, bpm_);
        }
        FontRenderer::drawString(display, 2, 2, header_buf, DisplayDriver::kColorCyan, DisplayDriver::kColorBlack, FontType::Font5x7);

        display.drawHLine(0, compact_layout::kHeaderDividerY, dw, DisplayDriver::kColorMidGray);

        // 4 Multi-track rows (y=14..68)
        for (uint8_t t = 0; t < 4; ++t) {
            int y = compact_layout::kTrackStartRowY + t * compact_layout::kTrackRowH;
            bool is_sel = (t == selected_track_);
            bool is_mute = track_mutes_[t];

            // Track label
            uint16_t lbl_col = is_mute ? DisplayDriver::kColorRed : (is_sel ? DisplayDriver::kColorYellow : DisplayDriver::kColorWhite);
            FontRenderer::drawString(display, 2, y + 1, track_names_[t], lbl_col, DisplayDriver::kColorBlack, FontType::Font5x7);

            // 16 Steps (compact 6px boxes)
            for (uint8_t s = 0; s < 16; ++s) {
                int group = s / 4;
                int x = 24 + s * 8 + group * 2;
                bool active = (track_masks_[t] & (1 << s)) != 0;
                bool has_lock = (track_plock_masks_[t] & (1 << s)) != 0;

                uint16_t col;
                if (active) {
                    col = is_mute ? DisplayDriver::kColorDarkGray : (is_sel ? DisplayDriver::kColorCyan : DisplayDriver::kColorGreen);
                    display.fillRect(x, y, 6, 9, col);
                } else {
                    col = (s % 4 == 0) ? DisplayDriver::kColorMidGray : DisplayDriver::kColorDarkGray;
                    display.drawRect(x, y, 6, 9, col);
                }

                // Draw P-Lock Accent Dot
                if (has_lock) {
                    display.fillRect(x + 1, y + 1, 2, 2, DisplayDriver::kColorAmber);
                }

                if (s == current_step_ && is_playing_) {
                    display.drawRect(x - 1, y - 1, 8, 11, DisplayDriver::kColorWhite);
                }
            }
        }

        display.drawHLine(0, compact_layout::kMidDividerY, dw, DisplayDriver::kColorMidGray);

        // Selected Track Detailed Steps & Velocity View (y=74..114)
        char det_buf[48];
        snprintf(det_buf, sizeof(det_buf), "EDIT: %s [PG%u: %u-%u]", 
                 track_names_[selected_track_], step_page_ + 1, step_page_ * 8 + 1, step_page_ * 8 + 8);
        FontRenderer::drawString(display, 2, 73, det_buf, DisplayDriver::kColorAmber, DisplayDriver::kColorBlack, FontType::Font5x7);

        // 8 Interactive Pad boxes for current page
        uint8_t base_step = step_page_ * 8;
        for (uint8_t p = 0; p < 8; ++p) {
            uint8_t s = base_step + p;
            int x = 4 + p * 19;
            int y = compact_layout::kPadBoxY;
            bool active = (track_masks_[selected_track_] & (1 << s)) != 0;
            bool has_lock = (track_plock_masks_[selected_track_] & (1 << s)) != 0;

            if (active) {
                display.fillRect(x, y, compact_layout::kPadBoxW, compact_layout::kPadBoxH, DisplayDriver::kColorCyan);
                char p_str[4];
                snprintf(p_str, sizeof(p_str), "%02u", s + 1);
                FontRenderer::drawString(display, x + 2, y + 8, p_str, DisplayDriver::kColorBlack, DisplayDriver::kColorCyan, FontType::Font5x7);
            } else {
                display.drawRect(x, y, compact_layout::kPadBoxW, compact_layout::kPadBoxH, DisplayDriver::kColorMidGray);
                char p_str[4];
                snprintf(p_str, sizeof(p_str), "%02u", s + 1);
                FontRenderer::drawString(display, x + 2, y + 8, p_str, DisplayDriver::kColorLightGray, DisplayDriver::kColorBlack, FontType::Font5x7);
            }

            if (has_lock) {
                display.fillRect(x + 12, y + 2, 3, 3, DisplayDriver::kColorAmber);
            }

            if (s == current_step_ && is_playing_) {
                display.fillRect(x + 2, y + 26, 13, 3, DisplayDriver::kColorYellow);
            }
        }

        // Bottom Help hint
        FontRenderer::drawString(display, 2, 118, "PADS: TOGGLE STEP | REC: MOTION", DisplayDriver::kColorYellow, DisplayDriver::kColorBlack, FontType::Font5x7);
        return;
    }

    // ── 284x76 Ultra-Widescreen Panoramic Drum Sequencer Layout ──

    // 1. Top Status Header (y=1..11)
    char pat_str[24];
    if (chain_enabled_ && chain_length_ > 0) {
        snprintf(pat_str, sizeof(pat_str), "CHN %u/%u [P%02u]", chain_index_ + 1, chain_length_, pattern_num_);
    } else {
        snprintf(pat_str, sizeof(pat_str), "SEQ P%02u", pattern_num_);
    }
    FontRenderer::drawString(display, 4, 2, pat_str, DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);

    // Transport Status
    if (is_recording_) {
        display.fillRect(68, 2, 38, 9, DisplayDriver::kColorRed);
        FontRenderer::drawString(display, 70, 3, "REC *", DisplayDriver::kColorWhite, DisplayDriver::kColorRed, FontType::Font5x7);
    } else if (is_playing_) {
        display.fillRect(68, 2, 38, 9, DisplayDriver::kColorDimGreen);
        FontRenderer::drawString(display, 70, 3, "PLAY >", DisplayDriver::kColorWhite, DisplayDriver::kColorDimGreen, FontType::Font5x7);
    } else {
        display.fillRect(68, 2, 38, 9, DisplayDriver::kColorDarkGray);
        FontRenderer::drawString(display, 70, 3, "STOP .", DisplayDriver::kColorLightGray, DisplayDriver::kColorDarkGray, FontType::Font5x7);
    }

    char tempo_str[32];
    snprintf(tempo_str, sizeof(tempo_str), "%.0fBPM  SW:%u%%", bpm_, swing_);
    FontRenderer::drawString(display, 110, 2, tempo_str, DisplayDriver::kColorAmber, DisplayDriver::kColorBlack, FontType::Font5x7);

    char page_str[32];
    snprintf(page_str, sizeof(page_str), "TRK:%s [PG%u: %u-%u]", 
             track_names_[selected_track_], step_page_ + 1, step_page_ * 8 + 1, step_page_ * 8 + 8);
    FontRenderer::drawString(display, 186, 2, page_str, DisplayDriver::kColorCyan, DisplayDriver::kColorBlack, FontType::Font5x7);

    display.drawHLine(0, wide_layout::kHeaderDividerY, dw, DisplayDriver::kColorMidGray);

    // 2. Multi-Track Drum Grid (y=14..54)
    for (uint8_t t = 0; t < 4; ++t) {
        int y = wide_layout::kTrackStartRowY + t * wide_layout::kTrackRowH;
        bool is_sel = (t == selected_track_);
        bool is_mute = track_mutes_[t];

        // Track Label Banner (x=2..24)
        if (is_sel) {
            display.fillRect(2, y, 22, 9, DisplayDriver::kColorCyan);
            FontRenderer::drawString(display, 4, y + 1, track_names_[t], DisplayDriver::kColorBlack, DisplayDriver::kColorCyan, FontType::Font5x7);
        } else {
            uint16_t lbl_col = is_mute ? DisplayDriver::kColorRed : DisplayDriver::kColorWhite;
            FontRenderer::drawString(display, 4, y + 1, track_names_[t], lbl_col, DisplayDriver::kColorBlack, FontType::Font5x7);
        }

        if (is_mute) {
            display.drawHLine(2, y + 4, 22, DisplayDriver::kColorRed);
        }

        // 16 Step Boxes (x=28..278) grouped in 4-step blocks
        for (uint8_t s = 0; s < 16; ++s) {
            int group = s / 4;
            int x = 28 + s * 14 + group * 4;
            bool active = (track_masks_[t] & (1 << s)) != 0;
            bool has_lock = (track_plock_masks_[t] & (1 << s)) != 0;

            uint16_t trig_col;
            if (active) {
                if (is_mute) {
                    trig_col = DisplayDriver::kColorDarkGray;
                } else if (is_sel) {
                    trig_col = DisplayDriver::kColorYellow;
                } else {
                    switch (t) {
                        case 0: trig_col = DisplayDriver::kColorAmber; break;   // BD
                        case 1: trig_col = DisplayDriver::kColorCyan; break;    // SD
                        case 2: trig_col = DisplayDriver::kColorGreen; break;   // CH
                        case 3: trig_col = DisplayDriver::kColorLightGray; break;// OH
                        default: trig_col = DisplayDriver::kColorGreen; break;
                    }
                }
                display.fillRect(x, y, wide_layout::kStepBoxW, wide_layout::kStepBoxH, trig_col);
            } else {
                // Dim outline; accent quarter-notes (0, 4, 8, 12)
                uint16_t border_col = (s % 4 == 0) ? DisplayDriver::kColorMidGray : DisplayDriver::kColorDarkGray;
                display.drawRect(x, y, wide_layout::kStepBoxW, wide_layout::kStepBoxH, border_col);
            }

            // Draw Parameter Lock Indicator Pip (Amber dot at top-right corner of step)
            if (has_lock) {
                display.fillRect(x + 8, y + 1, 3, 2, DisplayDriver::kColorAmber);
            }

            // Draw Playhead Cursor column indicator on active step
            if (s == current_step_ && is_playing_) {
                display.drawRect(x - 1, y - 1, 14, 10, DisplayDriver::kColorWhite);
            }
        }
    }

    display.drawHLine(0, wide_layout::kGridDividerY, dw, DisplayDriver::kColorDarkGray);

    // 3. Bottom Dynamic Pad Matrix & Navigation Help (y=58..75)
    uint8_t base_step = step_page_ * 8;
    for (uint8_t p = 0; p < 8; ++p) {
        uint8_t s = base_step + p;
        int x = 4 + p * wide_layout::kPadPitchX;
        int y = wide_layout::kPadMatrixY;
        bool active = (track_masks_[selected_track_] & (1 << s)) != 0;
        bool has_lock = (track_plock_masks_[selected_track_] & (1 << s)) != 0;

        uint16_t pad_border = active ? DisplayDriver::kColorYellow : DisplayDriver::kColorDarkGray;
        uint16_t pad_bg = active ? DisplayDriver::kColorDimGreen : DisplayDriver::kColorBlack;

        display.fillRect(x, y, wide_layout::kPadBoxW, wide_layout::kPadBoxH, pad_bg);
        display.drawRect(x, y, wide_layout::kPadBoxW, wide_layout::kPadBoxH, pad_border);

        char p_txt[6];
        snprintf(p_txt, sizeof(p_txt), "P%u", p + 1);
        FontRenderer::drawString(display, x + 3, y + 4, p_txt, active ? DisplayDriver::kColorYellow : DisplayDriver::kColorLightGray, pad_bg, FontType::Font5x7);

        // Parameter lock marker on pad
        if (has_lock) {
            display.fillRect(x + 14, y + 2, 4, 3, DisplayDriver::kColorAmber);
        }

        // Highlight currently playing step dot under the pad
        if (s == current_step_ && is_playing_) {
            display.fillRect(x + 5, y + 12, 10, 2, DisplayDriver::kColorWhite);
        }
    }

    // Right Action shortcuts
    FontRenderer::drawString(display, wide_layout::kShortcutsX, 60, "REC:MOTION-REC", is_recording_ ? DisplayDriver::kColorRed : DisplayDriver::kColorYellow, DisplayDriver::kColorBlack, FontType::Font5x7);
    FontRenderer::drawString(display, wide_layout::kShortcutsX, 68, "B5:PG B6:MUTE", DisplayDriver::kColorLightGray, DisplayDriver::kColorBlack, FontType::Font5x7);
}

} // namespace smk
