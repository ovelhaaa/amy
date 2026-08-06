#include "sequencer_screen.h"
#include "font_renderer.h"
#include <cstdio>

namespace smk {

SequencerScreen::SequencerScreen() {}

void SequencerScreen::setPatternNumber(uint8_t pat) { pattern_num_ = pat; }
void SequencerScreen::setBpm(float bpm) { bpm_ = bpm; }
void SequencerScreen::setSwing(uint8_t swing) { swing_ = swing; }
void SequencerScreen::setTrackName(const char* name) {
    if (name) snprintf(track_name_, sizeof(track_name_), "%s", name);
}

void SequencerScreen::setStepActive(uint8_t step, bool active) {
    if (step >= 16) return;
    if (active) step_mask_ |= (1 << step);
    else step_mask_ &= ~(1 << step);
}

void SequencerScreen::setCurrentStep(uint8_t step) {
    current_step_ = step % 16;
}

void SequencerScreen::update() {
    // Advance playback animation or step tick in demo mode
}

void SequencerScreen::render(DisplayDriver& display) {
    display.fillScreen(DisplayDriver::kColorBlack);

    // Title line
    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), "SEQ: PATTERN %02u [RUNNING]  BPM:%.1f  SWING:%u%%", 
             pattern_num_, bpm_, swing_);
    FontRenderer::drawString(display, 2, 2, title_buf, DisplayDriver::kColorCyan, DisplayDriver::kColorBlack, FontType::Font5x7);
    display.drawHLine(0, 11, DisplayDriver::kWidth, DisplayDriver::kColorMidGray);

    // Track label
    char trk_buf[32];
    snprintf(trk_buf, sizeof(trk_buf), "TRACK: %s", track_name_);
    FontRenderer::drawString(display, 2, 14, trk_buf, DisplayDriver::kColorYellow, DisplayDriver::kColorBlack, FontType::Font5x7);

    // Render 16 Steps Grid (4 groups of 4 steps)
    // Row 1: Step numbers (01..16)
    // Row 2: Triggers (filled box for active, dot for inactive)
    // Row 3: Playhead cursor (triangle ▲)

    int start_x = 2;
    int y_nums = 28;
    int y_trigs = 42;
    int y_curs = 56;

    for (uint8_t s = 0; s < 16; ++s) {
        int x = start_x + s * 17 + (s / 4) * 4; // Spacing between groups

        // Step number label
        char num_str[4];
        snprintf(num_str, sizeof(num_str), "%02u", s + 1);
        FontRenderer::drawString(display, x, y_nums, num_str, DisplayDriver::kColorLightGray, DisplayDriver::kColorBlack, FontType::Font5x7);

        // Step Trigger Box
        bool is_active = (step_mask_ & (1 << s)) != 0;
        uint16_t box_col = is_active ? DisplayDriver::kColorGreen : DisplayDriver::kColorDarkGray;
        if (is_active) {
            display.fillRect(x + 2, y_trigs, 10, 10, box_col);
        } else {
            display.drawRect(x + 2, y_trigs, 10, 10, box_col);
        }

        // Playhead cursor position
        if (s == current_step_) {
            FontRenderer::drawString(display, x + 4, y_curs, "^", DisplayDriver::kColorYellow, DisplayDriver::kColorBlack, FontType::Font5x7);
        }
    }
}

} // namespace smk
