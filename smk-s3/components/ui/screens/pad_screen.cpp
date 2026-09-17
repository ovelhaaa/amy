#include "ui_theme.h"
#include "ui_components.h"
#include "pad_screen.h"
#include "display_layout.h"
#include "font_renderer.h"
#include "esp_timer.h"
#include <cstdio>
#include <cstring>

namespace smk {

PadScreen::PadScreen() {}

void PadScreen::setObservedInputBank(uint8_t bank) {
    observed_input_bank_ = bank <= 2 ? bank : 0;
}

void PadScreen::setBankMode(PadBankMode mode, const char* name) {
    bank_mode_ = mode;
    if (name) snprintf(kit_name_, sizeof(kit_name_), "%s", name);

    switch (bank_mode_) {
        case PadBankMode::Drums:
            snprintf(pad_labels_[0], 12, "KICK");
            snprintf(pad_labels_[1], 12, "RIMS");
            snprintf(pad_labels_[2], 12, "SNARE");
            snprintf(pad_labels_[3], 12, "CLAP");
            snprintf(pad_labels_[4], 12, "E-SNARE");
            snprintf(pad_labels_[5], 12, "LOW-TOM");
            snprintf(pad_labels_[6], 12, "CLOSED-HH");
            snprintf(pad_labels_[7], 12, "HIGH-TOM");
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

static const char* inputBankToString(uint8_t bank) {
    switch (bank) {
        case 1: return "PAD A";
        case 2: return "PAD B [SHORTCUTS]";
        default: return "PADS [SOURCE ?]";
    }
}

static const char* labelForInputBank(uint8_t bank, uint8_t pad_idx, const char* default_label) {
    static constexpr const char* kPadBLabels[8] = {
        "PREV", "NEXT", "HOME", "ARP",
        "PAGE<", "PAGE>", "LEARN", "SAVE*"
    };
    return bank == 2 ? kPadBLabels[pad_idx] : default_label;
}

void PadScreen::render(DisplayDriver& display) {
    display.fillScreen(DisplayDriver::kColorBlack);
    int16_t dw = display.width();
    int16_t dh = display.height();

    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    bool hiif (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
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
            if (!label || label[0] == '\0') label = "---";

            int16_t label_w = FontRenderer::stringWidth(label, FontType::Font5x7, 1);
            FontRenderer::drawString(display, x + (pad_w - label_w) / 2, y + pad_h - 14, label, is_hit ? ColorTextPrimary : ColorTextSecondary, bg_color, FontType::Font5x7, 1);

            // Velocity feedback
            if (is_hit) {
                int16_t vel_h = (last_velocities_[i] * (pad_h - 24)) / 127;
                display.fillRect(x + 12, y + pad_h - 18 - vel_h, pad_w - 24, vel_h, theme_color);
            }
        }
    } else if (dw <= 160) {#include "ui_components.h"
#include "pad_screen.h"
#include "display_layout.h"
#include "font_renderer.h"
#include "esp_timer.h"
#include <cstdio>
#include <cstring>

namespace smk {

PadScreen::PadScreen() {}

void PadScreen::setObservedInputBank(uint8_t bank) {
    observed_input_bank_ = bank <= 2 ? bank : 0;
}

void PadScreen::setBankMode(PadBankMode mode, const char* name) {
    bank_mode_ = mode;
    if (name) snprintf(kit_name_, sizeof(kit_name_), "%s", name);

    switch (bank_mode_) {
        case PadBankMode::Drums:
            snprintf(pad_labels_[0], 12, "KICK");
            snprintf(pad_labels_[1], 12, "RIMS");
            snprintf(pad_labels_[2], 12, "SNARE");
            snprintf(pad_labels_[3], 12, "CLAP");
            snprintf(pad_labels_[4], 12, "E-SNARE");
            snprintf(pad_labels_[5], 12, "LOW-TOM");
            snprintf(pad_labels_[6], 12, "CLOSED-HH");
            snprintf(pad_labels_[7], 12, "HIGH-TOM");
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

static const char* inputBankToString(uint8_t bank) {
    switch (bank) {
        case 1: return "PAD A";
        case 2: return "PAD B [SHORTCUTS]";
        default: return "PADS [SOURCE ?]";
    }
}

static const char* labelForInputBank(uint8_t bank, uint8_t pad_idx, const char* default_label) {
    static constexpr const char* kPadBLabels[8] = {
        "PREV", "NEXT", "HOME", "ARP",
        "PAGE<", "PAGE>", "LEARN", "SAVE*"
    };
    return bank == 2 ? kPadBLabels[pad_idx] : default_label;
}

void PadScreen::render(DisplayDriver& display) {
    display.fillScreen(DisplayDriver::kColorBlack);
    int16_t dw = display.width();
    int16_t dh = display.height();

    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    bool hit_recent = (now - last_hit_time_ms_) < 200;

    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        display.fillRect(0, 0, dw, 30, DisplayDriver::kColorDarkGray);
        const char* bank_text = observed_input_bank_ == 2 ? "PAD B: SHORTCUTS"
            : (observed_input_bank_ == 1 ? bankModeToString(bank_mode_) : "PADS: SOURCE ?");
        char clipped_bank[19];
        snprintf(clipped_bank, sizeof(clipped_bank), "%.18s", bank_text);
        FontRenderer::drawString(display, 4, 4, clipped_bank, DisplayDriver::kColorCyan,
                                 DisplayDriver::kColorDarkGray, FontType::Font5x7, 2);

        for (uint8_t i = 0; i < 8; ++i) {
            const int16_t col = i % 2;
            const int16_t row = i / 2;
            const int16_t x = 4 + col * 118;
            const int16_t y = 36 + row * 42;
            constexpr int16_t pad_w = 114;
            constexpr int16_t pad_h = 36;
            const bool is_hit = hit_recent && last_hit_pad_ == i;
            const uint16_t bg = is_hit ? DisplayDriver::kColorDimGreen : DisplayDriver::kColorBlack;
            display.fillRect(x, y, pad_w, pad_h, bg);
            display.drawRect(x, y, pad_w, pad_h,
                             is_hit ? DisplayDriver::kColorYellow : DisplayDriver::kColorMidGray);
            char badge[5];
            snprintf(badge, sizeof(badge), "P%u", i + 1);
            FontRenderer::drawString(display, x + 5, y + 5, badge,
                                     is_hit ? DisplayDriver::kColorYellow : DisplayDriver::kColorCyan,
                                     bg, FontType::Font5x7);
            const char* label = labelForInputBank(observed_input_bank_, i, pad_labels_[i]);
            char clipped_label[16];
            snprintf(clipped_label, sizeof(clipped_label), "%.14s", label);
            FontRenderer::drawString(display, x + 28, y + 5, clipped_label,
                                     DisplayDriver::kColorWhite, bg, FontType::Font5x7);
            if (is_hit) {
                const int16_t velocity_w = static_cast<int16_t>((pad_w - 10) * last_hit_vel_ / 127U);
                display.fillRect(x + 5, y + 26, velocity_w, 5, DisplayDriver::kColorYellow);
            } else {
                display.drawHLine(x + 5, y + 28, pad_w - 10, DisplayDriver::kColorDarkGray);
            }
        }

        display.drawHLine(0, 208, dw, DisplayDriver::kColorDarkGray);
        char footer[48];
        if (hit_recent) {
            snprintf(footer, sizeof(footer), "HIT P%u  VEL %u", last_hit_pad_ + 1, last_hit_vel_);
        } else {
            snprintf(footer, sizeof(footer), "A/B SAO INFERIDOS PELO MIDI RECEBIDO");
        }
        FontRenderer::drawString(display, 4, 218, footer, DisplayDriver::kColorLightGray,
                                 DisplayDriver::kColorBlack, FontType::Font3x5);
        return;
    }

    // ── 160x128 Display Layout (2 Columns x 4 Rows, Full Pad Names) ──
    if (dw <= 160) {
        char title_buf[48];
        if (observed_input_bank_ == 2) {
            snprintf(title_buf, sizeof(title_buf), "PADS: PAD B [SHORTCUTS]");
        } else if (observed_input_bank_ == 1) {
            snprintf(title_buf, sizeof(title_buf), "PADS: %s", bankModeToString(bank_mode_));
        } else {
            snprintf(title_buf, sizeof(title_buf), "PADS: SOURCE ?");
        }
        FontRenderer::drawString(display, 2, 2, title_buf, DisplayDriver::kColorCyan, DisplayDriver::kColorBlack, FontType::Font5x7);
        display.drawHLine(0, 11, dw, DisplayDriver::kColorMidGray);

        int pad_w = 76;
        int pad_h = 23;

        for (uint8_t i = 0; i < 8; ++i) {
            int col = (i >= 4) ? 1 : 0; // Left column (0..3), Right column (4..7)
            int row = i % 4;

            int x = 3 + col * 78;
            int y = 14 + row * 25;

            bool is_hit = hit_recent && (last_hit_pad_ == i);
            uint16_t border_col = is_hit ? DisplayDriver::kColorYellow : DisplayDriver::kColorMidGray;
            uint16_t bg_col = is_hit ? DisplayDriver::kColorDimGreen : DisplayDriver::kColorBlack;

            display.fillRect(x, y, pad_w, pad_h, bg_col);
            display.drawRect(x, y, pad_w, pad_h, border_col);

            // Pad Number Badge
            char p_badge[6];
            snprintf(p_badge, sizeof(p_badge), "P%d", i + 1);
            FontRenderer::drawString(display, x + 3, y + 4, p_badge, is_hit ? DisplayDriver::kColorYellow : DisplayDriver::kColorCyan, bg_col, FontType::Font5x7);

            // Full pad label, matching the most recently observed physical bank.
            const char* pad_label = labelForInputBank(observed_input_bank_, i, pad_labels_[i]);
            FontRenderer::drawString(display, x + 20, y + 4, pad_label, DisplayDriver::kColorWhite, bg_col, FontType::Font5x7);

            // Velocity hit visual bar
            if (is_hit) {
                int bar_w = (last_hit_vel_ * (pad_w - 6)) / 127;
                display.fillRect(x + 3, y + 16, bar_w, 4, DisplayDriver::kColorYellow);
            }
        }

        // Bottom status bar
        char hit_buf[48];
        if (hit_recent) {
            snprintf(hit_buf, sizeof(hit_buf), "HIT P%d [%s] VEL:%d", last_hit_pad_ + 1,
                     labelForInputBank(observed_input_bank_, last_hit_pad_, pad_labels_[last_hit_pad_]), last_hit_vel_);
            FontRenderer::drawString(display, 2, 116, hit_buf, DisplayDriver::kColorYellow, DisplayDriver::kColorBlack, FontType::Font5x7);
        } else {
            FontRenderer::drawString(display, 2, 116, "PAD B: SHORTCUTS | P8 HOLD: SAVE", DisplayDriver::kColorLightGray, DisplayDriver::kColorBlack, FontType::Font5x7);
        }
        return;
    }

    // ── 284x76 Panoramic Layout (2 Rows x 4 Columns) ──
    char title_buf[64];
    if (observed_input_bank_ == 2) {
        snprintf(title_buf, sizeof(title_buf), "%s: MIDI ACTIONS", inputBankToString(observed_input_bank_));
    } else if (observed_input_bank_ == 1) {
        snprintf(title_buf, sizeof(title_buf), "%s: %s", inputBankToString(observed_input_bank_),
                 bankModeToString(bank_mode_));
    } else {
        snprintf(title_buf, sizeof(title_buf), "%s", inputBankToString(observed_input_bank_));
    }
    FontRenderer::drawString(display, 2, 2, title_buf, DisplayDriver::kColorCyan, DisplayDriver::kColorBlack, FontType::Font5x7);
    display.drawHLine(0, 11, dw, DisplayDriver::kColorMidGray);

    int pad_w = 67;
    int pad_h = 24;

    for (uint8_t i = 0; i < 8; ++i) {
        int col = i % 4;
        int row = i / 4;

        int x = 3 + col * 70;
        int y = 14 + row * 26;

        bool is_hit = hit_recent && (last_hit_pad_ == i);
        uint16_t border_col = is_hit ? DisplayDriver::kColorYellow : DisplayDriver::kColorMidGray;
        uint16_t bg_col = is_hit ? DisplayDriver::kColorDimGreen : DisplayDriver::kColorBlack;

        display.fillRect(x, y, pad_w, pad_h, bg_col);
        display.drawRect(x, y, pad_w, pad_h, border_col);

        char p_label[24];
        snprintf(p_label, sizeof(p_label), "P%d:%s", i + 1,
                 labelForInputBank(observed_input_bank_, i, pad_labels_[i]));
        FontRenderer::drawString(display, x + 3, y + 7, p_label, is_hit ? DisplayDriver::kColorYellow : DisplayDriver::kColorWhite, bg_col, FontType::Font5x7);

        if (is_hit) {
            int bar_w = (last_hit_vel_ * (pad_w - 6)) / 127;
            display.fillRect(x + 3, y + 18, bar_w, 3, DisplayDriver::kColorYellow);
        }
    }

    char hit_buf[48];
    if (hit_recent) {
        snprintf(hit_buf, sizeof(hit_buf), "HIT P%d [%s] VEL:%d", last_hit_pad_ + 1,
                 labelForInputBank(observed_input_bank_, last_hit_pad_, pad_labels_[last_hit_pad_]), last_hit_vel_);
    } else {
        snprintf(hit_buf, sizeof(hit_buf), "PAD B: SHORTCUTS | P8 HOLD: SAVE");
    }
    FontRenderer::drawString(display, 2, 66, hit_buf, DisplayDriver::kColorYellow, DisplayDriver::kColorBlack, FontType::Font5x7);
}

} // namespace smk
