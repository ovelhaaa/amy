#include "pad_screen.h"
#include "font_renderer.h"
#include "esp_timer.h"
#include <cstdio>
#include <cstring>

namespace smk {

PadScreen::PadScreen() {}

void PadScreen::setBankMode(PadBankMode mode, const char* name) {
    bank_mode_ = mode;
    if (name) snprintf(kit_name_, sizeof(kit_name_), "%s", name);

    switch (bank_mode_) {
        case PadBankMode::Drums:
            snprintf(pad_labels_[0], 12, "KICK 808");
            snprintf(pad_labels_[1], 12, "SNARE DRY");
            snprintf(pad_labels_[2], 12, "CHAT HI");
            snprintf(pad_labels_[3], 12, "OHAT OPEN");
            snprintf(pad_labels_[4], 12, "CLAP HAND");
            snprintf(pad_labels_[5], 12, "TOM LOW");
            snprintf(pad_labels_[6], 12, "CYMBAL");
            snprintf(pad_labels_[7], 12, "COWBELL");
            break;
        case PadBankMode::Chords:
            snprintf(pad_labels_[0], 12, "Cmaj7");
            snprintf(pad_labels_[1], 12, "Dm7");
            snprintf(pad_labels_[2], 12, "Em7");
            snprintf(pad_labels_[3], 12, "Fmaj7");
            snprintf(pad_labels_[4], 12, "G7");
            snprintf(pad_labels_[5], 12, "Am7");
            snprintf(pad_labels_[6], 12, "Bm7b5");
            snprintf(pad_labels_[7], 12, "Cmaj9");
            break;
        case PadBankMode::PatternLaunch:
            for (int i = 0; i < 8; ++i) snprintf(pad_labels_[i], 12, "PAT %02d", i + 1);
            break;
        case PadBankMode::PerformanceFx:
            snprintf(pad_labels_[0], 12, "FILTER TH");
            snprintf(pad_labels_[1], 12, "STUTTER");
            snprintf(pad_labels_[2], 12, "TAPE STOP");
            snprintf(pad_labels_[3], 12, "DLY THROW");
            snprintf(pad_labels_[4], 12, "RVB FREEZ");
            snprintf(pad_labels_[5], 12, "BEAT REPT");
            snprintf(pad_labels_[6], 12, "MUTE DRUM");
            snprintf(pad_labels_[7], 12, "TRANSITION");
            break;
    }
}

void PadScreen::triggerPadHit(uint8_t pad_idx, uint8_t velocity) {
    if (pad_idx >= 8) return;
    last_hit_pad_ = pad_idx;
    last_hit_vel_ = velocity;
    last_hit_time_ms_ = (uint32_t)(esp_timer_get_time() / 1000);
}

void PadScreen::update() {}

static const char* bankModeToString(PadBankMode mode) {
    switch (mode) {
        case PadBankMode::Drums: return "BANK A [DRUMS]";
        case PadBankMode::Chords: return "BANK B [CHORD MEMORY]";
        case PadBankMode::PatternLaunch: return "BANK C [PATTERNS]";
        case PadBankMode::PerformanceFx: return "BANK D [PERF FX]";
        default: return "BANK A";
    }
}

void PadScreen::render(DisplayDriver& display) {
    display.fillScreen(DisplayDriver::kColorBlack);

    // Title line
    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), "PADS: %s - %s", bankModeToString(bank_mode_), kit_name_);
    FontRenderer::drawString(display, 2, 2, title_buf, DisplayDriver::kColorCyan, DisplayDriver::kColorBlack, FontType::Font5x7);
    display.drawHLine(0, 11, DisplayDriver::kWidth, DisplayDriver::kColorMidGray);

    // Render 8 Pad boxes in 2 rows of 4 pads
    // Row 1: Pads 1-4
    // Row 2: Pads 5-8
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    bool hit_recent = (now - last_hit_time_ms_) < 200;

    int pad_w = 66;
    int pad_h = 24;

    for (uint8_t i = 0; i < 8; ++i) {
        int col = i % 4;
        int row = i / 4;

        int x = 2 + col * (pad_w + 4);
        int y = 14 + row * (pad_h + 3);

        bool is_hit = hit_recent && (last_hit_pad_ == i);
        uint16_t border_col = is_hit ? DisplayDriver::kColorYellow : DisplayDriver::kColorMidGray;
        uint16_t bg_col = is_hit ? DisplayDriver::kColorDimGreen : DisplayDriver::kColorBlack;

        display.fillRect(x, y, pad_w, pad_h, bg_col);
        display.drawRect(x, y, pad_w, pad_h, border_col);

        // Pad Label
        char p_label[16];
        snprintf(p_label, sizeof(p_label), "P%d:%s", i + 1, pad_labels_[i]);
        FontRenderer::drawString(display, x + 3, y + 8, p_label, DisplayDriver::kColorWhite, bg_col, FontType::Font5x7);
    }

    // Status bar at bottom
    char hit_buf[48];
    if (hit_recent) {
        snprintf(hit_buf, sizeof(hit_buf), "LAST HIT: PAD %d (%s) VEL: %d", last_hit_pad_ + 1, pad_labels_[last_hit_pad_], last_hit_vel_);
    } else {
        snprintf(hit_buf, sizeof(hit_buf), "READY | PRESS PAD-B TO SWITCH BANKS");
    }
    FontRenderer::drawString(display, 2, 66, hit_buf, DisplayDriver::kColorYellow, DisplayDriver::kColorBlack, FontType::Font5x7);
}

} // namespace smk
