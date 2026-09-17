#include "home_screen.h"
#include "display_layout.h"
#include "font_renderer.h"
#include <cstdio>
#include <cstring>

#include <cmath>

namespace smk {

HomeScreen::HomeScreen()
    : gauges_{
        BarGauge(2, 26, 32, 48, "CHAR"),
        BarGauge(37, 26, 32, 48, "BRTE"),
        BarGauge(72, 26, 32, 48, "MOTN"),
        BarGauge(107, 26, 32, 48, "SHAP"),
        BarGauge(142, 26, 32, 48, "ATK"),
        BarGauge(177, 26, 32, 48, "REL"),
        BarGauge(212, 26, 32, 48, "SPCE"),
        BarGauge(247, 26, 32, 48, "DRV")
    } {
}

void HomeScreen::onEnter() {
    setHomeKnobBankView(bank_view_);
}

void HomeScreen::setHomeKnobBankView(HomeKnobBankView view) {
    bank_view_ = view;
    static const char* kBankALabels[8] = { "CHAR", "BRTE", "MOTN", "SHAP", "ATK", "REL", "SPCE", "DRV" };
    static const char* kBankBLabels[8] = { "CUTOFF", "RES", "ENV", "DCAY", "CHOR", "DLAY", "REVB", "DRV" };

    if (bank_view_ == HomeKnobBankView::BankB_Engine) {
        snprintf(knob_bank_, sizeof(knob_bank_), "BANK B: ENGINE");
        for (int i = 0; i < 8; ++i) {
            gauges_[i].setLabel(kBankBLabels[i]);
            gauges_[i].setValue(engine_values_[i]);
            gauges_[i].setColors(DisplayDriver::kColorAmber, DisplayDriver::kColorWhite);
        }
    } else {
        snprintf(knob_bank_, sizeof(knob_bank_), "BANK A: MACROS");
        for (int i = 0; i < 8; ++i) {
            gauges_[i].setLabel(kBankALabels[i]);
            gauges_[i].setValue(macro_values_[i]);
            gauges_[i].setColors(DisplayDriver::kColorCyan, DisplayDriver::kColorWhite);
        }
    }
}

void HomeScreen::setPatchInfo(uint16_t number, const char* name, const char* mode) {
    patch_number_ = number;
    if (name) snprintf(patch_name_, sizeof(patch_name_), "%s", name);
    if (mode) snprintf(synth_mode_, sizeof(synth_mode_), "%s", mode);
}

void HomeScreen::setBpm(float bpm) {
    bpm_ = bpm;
}

void HomeScreen::setUsbConnected(bool connected) {
    usb_connected_ = connected;
}

void HomeScreen::setMidiActivity(bool active) {
    midi_active_ = active;
    if (active) {
        midi_activity_timer_ = 5; // ~150ms at 30 FPS
    }
}

void HomeScreen::setMacroValues(const uint8_t values[8]) {
    if (!values) return;
    for (int i = 0; i < 8; ++i) {
        macro_values_[i] = values[i];
    }
    if (bank_view_ == HomeKnobBankView::BankA_Macros) {
        for (int i = 0; i < 8; ++i) {
            gauges_[i].setValue(macro_values_[i]);
        }
    }
}

void HomeScreen::setEngineValues(const uint8_t values[8]) {
    if (!values) return;
    for (int i = 0; i < 8; ++i) {
        engine_values_[i] = values[i];
    }
    if (bank_view_ == HomeKnobBankView::BankB_Engine) {
        for (int i = 0; i < 8; ++i) {
            gauges_[i].setValue(engine_values_[i]);
        }
    }
}

void HomeScreen::setKnobBankLabel(const char* bank_name) {
    if (bank_name) snprintf(knob_bank_, sizeof(knob_bank_), "%s", bank_name);
}

void HomeScreen::setObservedKnobBank(uint8_t bank) {
    observed_knob_bank_ = bank <= 2 ? bank : 0;
}

void HomeScreen::setObservedPadBank(uint8_t bank) {
    observed_pad_bank_ = bank <= 2 ? bank : 0;
}

void HomeScreen::setActiveVoices(uint8_t active_count, uint8_t max_voices) {
    active_voices_ = active_count;
    max_voices_ = max_voices;
}

void HomeScreen::setCpuLoad(float load_percent) {
    cpu_load_ = load_percent;
}

void HomeScreen::update() {
    if (midi_activity_timer_ > 0) {
        midi_activity_timer_--;
        if (midi_activity_timer_ == 0) {
            midi_active_ = false;
        }
    }
}

namespace {
namespace compact_layout {
    constexpr int16_t kHeaderH = 18;
    constexpr int16_t kScopeX = 3;
    constexpr int16_t kScopeY = 19;
    constexpr int16_t kScopeW = 154;
    constexpr int16_t kScopeH = 24;
    constexpr int16_t kVoiceY = 45;
}
namespace wide_layout {
    constexpr int16_t kHeaderDividerY = 13;
    constexpr int16_t kRightMargin = 2;
    constexpr int16_t kFieldGap = 4;
    constexpr int16_t kBadgeW = 8;
    constexpr int16_t kVoiceW = 32;
    constexpr int16_t kTempoW = 40;
    constexpr int16_t kControlW = 48;
    constexpr int16_t kVoiceY = 3;
    constexpr int16_t kMidiY  = 3;
    constexpr int16_t kUsbY   = 3;
}

// A one-pixel overdraw makes the bank value bold without changing shared fonts.
void drawHeaderBank(DisplayDriver& display, int16_t x, char label, char value) {
    FontRenderer::drawChar(display, x, 4, label, DisplayDriver::kColorCyan,
                           DisplayDriver::kColorBlack, FontType::Font3x5);
    for (int16_t offset = 0; offset <= 1; ++offset) {
        FontRenderer::drawChar(display, x + 4 + offset, 3, value, DisplayDriver::kColorWhite,
                               DisplayDriver::kColorBlack, FontType::Font5x7);
    }
}

// Fit a header field before drawing: black glyph backgrounds are transparent,
// so drawing the next field cannot erase text that overflowed into its area.
void fitHeaderText(char* text, int16_t width, FontType font) {
    const int16_t pitch = FontRenderer::stringWidth("M", font);
    const size_t capacity = width > 0 ? static_cast<size_t>(width / pitch) : 0;
    if (strlen(text) > capacity) {
        if (capacity > 0) text[capacity - 1] = '~';
        text[capacity] = '\0';
    }
}

ParametricGlyph getKnobGlyph(int index) {
    switch (index) {
        case 0: return ParametricGlyph::Lowpass;
        case 1: return ParametricGlyph::Resonance;
        case 2: return ParametricGlyph::Attack;
        case 3: return ParametricGlyph::Release;
        case 4: return ParametricGlyph::SineWave;
        case 5: return ParametricGlyph::DelayTaps;
        case 6: return ParametricGlyph::ReverbCloud;
        case 7: return ParametricGlyph::DriveSaturation;
        default: return ParametricGlyph::GenericLevel;
    }
}
} // anonymous namespace

void HomeScreen::render(DisplayDriver& display) {
    display.fillScreen(DisplayDriver::kColorBlack);

    int16_t dw = display.width();
    int16_t dh = display.height();

    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        // 240x240: information is grouped vertically instead of stretching the
        // panoramic layout beyond the right edge.
        display.fillRect(0, 0, dw, 30, DisplayDriver::kColorDarkGray);

        char patch_buf[40];
        snprintf(patch_buf, sizeof(patch_buf), "P%03u %.20s", patch_number_,
                 patch_name_[0] != '\0' ? patch_name_ : "DEFAULT PATCH");
        // Account for the 2x font scale while reserving the status badges.
        fitHeaderText(patch_buf, (dw - 34) / 2, FontType::Font5x7);
        FontRenderer::drawString(display, 4, 3, patch_buf, DisplayDriver::kColorWhite,
                                 DisplayDriver::kColorDarkGray, FontType::Font5x7, 2);

        const uint16_t usb_color = usb_connected_ ? DisplayDriver::kColorGreen : DisplayDriver::kColorRed;
        display.fillRect(dw - 17, 4, 12, 9, usb_color);
        FontRenderer::drawString(display, dw - 14, 6, "U", DisplayDriver::kColorWhite,
                                 usb_color, FontType::Font3x5);
        display.fillRect(dw - 31, 4, 12, 9,
                         midi_active_ ? DisplayDriver::kColorYellow : DisplayDriver::kColorDarkGray);
        FontRenderer::drawString(display, dw - 28, 6, "M",
                                 midi_active_ ? DisplayDriver::kColorBlack : DisplayDriver::kColorMidGray,
                                 midi_active_ ? DisplayDriver::kColorYellow : DisplayDriver::kColorDarkGray,
                                 FontType::Font3x5);

        const char knob_bank = observed_knob_bank_ == 1 ? 'A' : (observed_knob_bank_ == 2 ? 'B' : '?');
        const char pad_bank = observed_pad_bank_ == 1 ? 'A' : (observed_pad_bank_ == 2 ? 'B' : '?');
        // Keep polyphony visible in the header while reserving the lower strip
        // for a glanceable tempo readout. Eight bounded bars avoid any extra
        // data acquisition or work in the audio path.
        char mode_buf[8];
        snprintf(mode_buf, sizeof(mode_buf), "%.6s", synth_mode_[0] != '\0' ? synth_mode_ : "POLY");
        FontRenderer::drawString(display, 4, 21, mode_buf, DisplayDriver::kColorAmber,
                                 DisplayDriver::kColorDarkGray, FontType::Font3x5);

        char voice_buf[16];
        snprintf(voice_buf, sizeof(voice_buf), "V%u/%u", active_voices_,
                 max_voices_ > 0 ? max_voices_ : 12);
        const int16_t voice_text_x = 8 + FontRenderer::stringWidth(mode_buf, FontType::Font3x5);
        FontRenderer::drawString(display, voice_text_x, 21, voice_buf,
                                 DisplayDriver::kColorLightGray, DisplayDriver::kColorDarkGray,
                                 FontType::Font3x5);
        const int16_t voice_bar_x = voice_text_x +
            FontRenderer::stringWidth(voice_buf, FontType::Font3x5) + 4;
        for (uint8_t voice = 0; voice < max_voices_ && voice < 8; ++voice) {
            display.fillRect(voice_bar_x + voice * 6, 22, 4, 4,
                             voice < active_voices_ ? DisplayDriver::kColorCyan
                                                    : DisplayDriver::kColorMidGray);
        }

        char bank_buf[16];
        snprintf(bank_buf, sizeof(bank_buf), "K:%c P:%c", knob_bank, pad_bank);
        const int16_t bank_x = dw - 4 - FontRenderer::stringWidth(bank_buf, FontType::Font3x5);
        FontRenderer::drawString(display, bank_x, 21, bank_buf, DisplayDriver::kColorLightGray,
                                 DisplayDriver::kColorDarkGray, FontType::Font3x5);

        const uint16_t theme_color = bank_view_ == HomeKnobBankView::BankB_Engine
            ? DisplayDriver::kColorAmber : DisplayDriver::kColorCyan;
        for (int i = 0; i < 8; ++i) {
            const int16_t col = i % 4;
            const int16_t row = i / 4;
            const int16_t x = 4 + col * 59;
            const int16_t y = 36 + row * 62;
            constexpr int16_t card_w = 55;
            constexpr int16_t card_h = 56;
            display.drawRect(x, y, card_w, card_h, DisplayDriver::kColorDarkGray);

            const char* label = gauges_[i].label();
            const int16_t label_w = FontRenderer::stringWidth(label, FontType::Font3x5);
            FontRenderer::drawString(display, x + (card_w - label_w) / 2, y + 4, label,
                                     theme_color, DisplayDriver::kColorBlack, FontType::Font3x5);
            GlyphRenderer::drawGlyph(display, x + 5, y + 15, getKnobGlyph(i), theme_color);

            const uint8_t value = bank_view_ == HomeKnobBankView::BankB_Engine
                ? engine_values_[i] : macro_values_[i];
            char value_buf[8];
            snprintf(value_buf, sizeof(value_buf), "%u", value);
            const int16_t value_w = FontRenderer::stringWidth(value_buf, FontType::Font5x7, 2);
            FontRenderer::drawString(display, x + card_w - value_w - 4, y + 17, value_buf,
                                     DisplayDriver::kColorWhite, DisplayDriver::kColorBlack,
                                     FontType::Font5x7, 2);
            display.drawRect(x + 4, y + 45, card_w - 8, 6, DisplayDriver::kColorMidGray);
            const int16_t fill_w = static_cast<int16_t>((card_w - 10) * value / 127U);
            if (fill_w > 0) {
                display.fillRect(x + 5, y + 46, fill_w, 4, theme_color);
            }
        }

        OscilloscopeWidget scope(4, 162, dw - 8, 45);
        scope.setSamples(scope_samples_, scope_sample_count_);
        scope.setActive(active_voices_ > 0 || midi_active_);
        scope.draw(display);

        display.drawHLine(0, 212, dw, DisplayDriver::kColorDarkGray);
        const float display_bpm = std::isfinite(bpm_) && bpm_ > 0
            ? fminf(bpm_, 999.0f) : 120.0f;
        char bpm_buf[8];
        snprintf(bpm_buf, sizeof(bpm_buf), "%.0f", display_bpm);
        constexpr uint8_t bpm_scale = 3;
        constexpr uint8_t unit_scale = 2;
        const int16_t bpm_w = FontRenderer::stringWidth(bpm_buf, FontType::Font5x7, bpm_scale);
        const int16_t unit_w = FontRenderer::stringWidth("BPM", FontType::Font3x5, unit_scale);
        const int16_t tempo_w = bpm_w + 5 + unit_w;
        const int16_t bpm_x = (dw - tempo_w) / 2;
        FontRenderer::drawString(display, bpm_x, 216, bpm_buf, DisplayDriver::kColorAmber,
                                 DisplayDriver::kColorBlack, FontType::Font5x7, bpm_scale);
        FontRenderer::drawString(display, bpm_x + bpm_w + 5, 226, "BPM",
                                 DisplayDriver::kColorLightGray, DisplayDriver::kColorBlack,
                                 FontType::Font3x5, unit_scale);

        char cpu_buf[16];
        snprintf(cpu_buf, sizeof(cpu_buf), "CPU %.0f%%", cpu_load_);
        const int16_t cpu_x = dw - 4 - FontRenderer::stringWidth(cpu_buf, FontType::Font3x5);
        FontRenderer::drawString(display, cpu_x, 234, cpu_buf, DisplayDriver::kColorMidGray,
                                 DisplayDriver::kColorBlack, FontType::Font3x5);
        return;
    }

    if (dw <= 160) {
        // ── 160x128 (1.8" Display) 2x4 Matrix Layout ──

        // 1. Header Banner (Dark Gray background, y=0..17)
        display.fillRect(0, 0, 160, compact_layout::kHeaderH, DisplayDriver::kColorDarkGray);
        
        // Line 1: Patch number & FULL patch name (up to 18 chars)
        const char* name = (patch_name_[0] != '\0') ? patch_name_ : "DEFAULT PATCH";
        char p_str[36];
        snprintf(p_str, sizeof(p_str), "P%03u %-14.14s", patch_number_, name);
        FontRenderer::drawString(display, 3, 2, p_str, DisplayDriver::kColorWhite, DisplayDriver::kColorDarkGray, FontType::Font5x7);

        // Status Indicators on top right (USB & MIDI LEDs)
        uint16_t usb_col = usb_connected_ ? DisplayDriver::kColorGreen : DisplayDriver::kColorRed;
        display.fillRect(144, 3, 5, 5, usb_col);
        if (midi_active_) {
            display.fillRect(151, 3, 5, 5, DisplayDriver::kColorYellow);
        }

        // Line 2: Synth Mode, BPM, Knob Bank
        char b_str[36];
        const char* s_mode = (synth_mode_[0] != '\0') ? synth_mode_ : "POLY";
        snprintf(b_str, sizeof(b_str), "[%s] %.0fBPM %-10.10s", s_mode, bpm_ > 0 ? bpm_ : 120.0f, knob_bank_);
        FontRenderer::drawString(display, 3, 10, b_str, DisplayDriver::kColorAmber, DisplayDriver::kColorDarkGray, FontType::Font5x7);

        display.drawHLine(0, compact_layout::kHeaderH, 160, DisplayDriver::kColorMidGray);

        // 2. Live Audio Oscilloscope (154x24 px)
        OscilloscopeWidget scope(compact_layout::kScopeX, compact_layout::kScopeY,
                                 compact_layout::kScopeW, compact_layout::kScopeH);
        scope.setSamples(scope_samples_, scope_sample_count_);
        scope.setActive(active_voices_ > 0 || midi_active_);
        scope.draw(display);

        // 3. 8 Polyphony Voice Indicator Dots (y=45)
        for (uint8_t v = 0; v < max_voices_ && v < 8; ++v) {
            uint16_t voice_col = (v < active_voices_) ? DisplayDriver::kColorCyan : DisplayDriver::kColorDarkGray;
            display.fillRect(4 + v * 7, compact_layout::kVoiceY, 5, 3, voice_col);
        }
        char v_str[16];
        snprintf(v_str, sizeof(v_str), "VOICES: %02u/%02u", active_voices_, max_voices_ > 0 ? max_voices_ : 12);
        FontRenderer::drawString(display, 64, 44, v_str, DisplayDriver::kColorLightGray, DisplayDriver::kColorBlack, FontType::Font3x5);

        // 4. 2x4 Knob Matrix (y=50..115)
        // Row 1: Knobs 0..3 (y=50, h=31)
        // Row 2: Knobs 4..7 (y=83, h=31)
        uint16_t theme_color = (bank_view_ == HomeKnobBankView::BankB_Engine) ? DisplayDriver::kColorAmber : DisplayDriver::kColorCyan;

        for (int i = 0; i < 8; ++i) {
            int row = i / 4;
            int col = i % 4;
            int16_t bx = 2 + col * 39;
            int16_t by = 50 + row * 33;
            int16_t bw = (col == 3) ? 38 : 37;
            int16_t bh = 31;

            // Box boundary
            display.fillRect(bx, by, bw, bh, DisplayDriver::kColorBlack);
            display.drawRect(bx, by, bw, bh, DisplayDriver::kColorDarkGray);

            // Label
            const char* lbl = gauges_[i].label();
            int16_t lw = FontRenderer::stringWidth(lbl, FontType::Font3x5);
            FontRenderer::drawString(display, bx + (bw - lw) / 2, by + 2, lbl, theme_color, DisplayDriver::kColorBlack, FontType::Font3x5);

            // Parametric Glyph
            GlyphRenderer::drawGlyph(display, bx + (bw - 16) / 2, by + 10, getKnobGlyph(i), theme_color);

            // Value text
            uint8_t val = (bank_view_ == HomeKnobBankView::BankB_Engine) ? engine_values_[i] : macro_values_[i];
            char vbuf[8];
            snprintf(vbuf, sizeof(vbuf), "%u", val);
            int16_t vw = FontRenderer::stringWidth(vbuf, FontType::Font5x7);
            FontRenderer::drawString(display, bx + (bw - vw) / 2, by + 21, vbuf, DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);
        }

        // 5. Footer Status Strip (y=117..127)
        display.drawHLine(0, 116, 160, DisplayDriver::kColorDarkGray);
        char foot_buf[48];
        snprintf(foot_buf, sizeof(foot_buf), "CPU:%.0f%% | UNDR:%u | %s", cpu_load_, 0, knob_bank_);
        FontRenderer::drawString(display, 3, 119, foot_buf, DisplayDriver::kColorLightGray, DisplayDriver::kColorBlack, FontType::Font3x5);
        return;
    }

    // ── 284x76 Panoramic Layout ──

    // 1. Header: patch, observed controller banks, tempo, voices, MIDI/USB.
    const int16_t usb_x = dw - wide_layout::kRightMargin - wide_layout::kBadgeW;
    const int16_t midi_x = usb_x - wide_layout::kFieldGap - wide_layout::kBadgeW;
    const int16_t voice_x = midi_x - wide_layout::kFieldGap - wide_layout::kVoiceW;
    const int16_t bpm_x = voice_x - wide_layout::kFieldGap - wide_layout::kTempoW;
    const int16_t control_x = bpm_x - wide_layout::kFieldGap - wide_layout::kControlW;

    char patch_buf[40];
    // Factory names already contain the formatted ID. Strip that exact prefix
    // only for display, retaining musical identifiers such as A11 and DX7.
    char number_prefix[8];
    snprintf(number_prefix, sizeof(number_prefix), "%03u ", patch_number_);
    const size_t prefix_length = strlen(number_prefix);
    const char* display_name = patch_name_;
    if (strncmp(display_name, number_prefix, prefix_length) == 0) {
        display_name += prefix_length;
    }
    snprintf(patch_buf, sizeof(patch_buf), "%03u %s", patch_number_, display_name);
    // Factory names are space-padded; padding must not trigger truncation.
    size_t patch_length = strlen(patch_buf);
    while (patch_length > 0 && patch_buf[patch_length - 1] == ' ') {
        patch_buf[--patch_length] = '\0';
    }
    fitHeaderText(patch_buf, control_x - wide_layout::kFieldGap - 3, FontType::Font5x7);
    FontRenderer::drawString(display, 3, 3, patch_buf, DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);

    const char knob_bank = observed_knob_bank_ == 1 ? 'A' : (observed_knob_bank_ == 2 ? 'B' : '?');
    const char pad_bank = observed_pad_bank_ == 1 ? 'A' : (observed_pad_bank_ == 2 ? 'B' : '?');
    char control_buf[16];
    snprintf(control_buf, sizeof(control_buf), "%.5s", synth_mode_);
    FontRenderer::drawString(display, control_x, 4, control_buf, DisplayDriver::kColorCyan, DisplayDriver::kColorBlack, FontType::Font3x5);
    drawHeaderBank(display, control_x + 24, 'K', knob_bank);
    drawHeaderBank(display, control_x + 37, 'P', pad_bank);

    char bpm_buf[16];
    // Three digits fit beside the small unit. Reject invalid display values and
    // bound the formatting width without modifying the actual transport tempo.
    const float display_bpm = std::isfinite(bpm_) && bpm_ > 0 ? fminf(bpm_, 999.0f) : 120.0f;
    snprintf(bpm_buf, sizeof(bpm_buf), "%.0f", display_bpm);
    // Doubled bitmap pixels give the number a bold, 10-pixel-high stroke.
    FontRenderer::drawString(display, bpm_x, 1, bpm_buf, DisplayDriver::kColorWhite,
                             DisplayDriver::kColorBlack, FontType::Font3x5, 2);
    const int16_t unit_x = bpm_x + FontRenderer::stringWidth(bpm_buf, FontType::Font3x5, 2) + 2;
    FontRenderer::drawString(display, unit_x, 6, "BPM", DisplayDriver::kColorAmber,
                             DisplayDriver::kColorBlack, FontType::Font3x5);

    // Voice activity: eight 3px bars with 1px spacing, before the badges.
    for (uint8_t v = 0; v < max_voices_ && v < 8; ++v) {
        uint16_t voice_col = (v < active_voices_) ? DisplayDriver::kColorCyan : DisplayDriver::kColorDarkGray;
        display.fillRect(voice_x + v * 4, wide_layout::kVoiceY, 3, 7, voice_col);
    }

    // MIDI and USB badges anchored to the right edge.
    if (midi_active_) {
        display.fillRect(midi_x, wide_layout::kMidiY, 8, 7, DisplayDriver::kColorYellow);
        FontRenderer::drawString(display, midi_x + 2, wide_layout::kMidiY + 1, "M", DisplayDriver::kColorBlack, DisplayDriver::kColorYellow, FontType::Font3x5);
    } else {
        display.fillRect(midi_x, wide_layout::kMidiY, 8, 7, DisplayDriver::kColorDarkGray);
        FontRenderer::drawString(display, midi_x + 2, wide_layout::kMidiY + 1, "M", DisplayDriver::kColorMidGray, DisplayDriver::kColorDarkGray, FontType::Font3x5);
    }

    uint16_t usb_col = usb_connected_ ? DisplayDriver::kColorGreen : DisplayDriver::kColorRed;
    display.fillRect(usb_x, wide_layout::kUsbY, 8, 7, usb_col);
    FontRenderer::drawString(display, usb_x + 2, wide_layout::kUsbY + 1, "U", DisplayDriver::kColorWhite, usb_col, FontType::Font3x5);

    display.drawHLine(0, wide_layout::kHeaderDividerY, display.width(), DisplayDriver::kColorMidGray);

    // 2. 8 Discrete Macro Channels (x=0..283, y=14..75, ~35px each)
    uint16_t theme_color = (bank_view_ == HomeKnobBankView::BankB_Engine) ? DisplayDriver::kColorAmber : DisplayDriver::kColorCyan;

    for (int i = 0; i < 8; ++i) {
        int16_t cx = 1 + i * 35;
        if (i > 0) {
            display.drawVLine(cx - 1, 14, 62, DisplayDriver::kColorDarkGray);
        }

        // Label at top of channel
        const char* lbl = gauges_[i].label();
        int16_t lw = FontRenderer::stringWidth(lbl, FontType::Font3x5);
        FontRenderer::drawString(display, cx + (34 - lw) / 2, 16, lbl, theme_color, DisplayDriver::kColorBlack, FontType::Font3x5);

        // Parametric Glyph
        GlyphRenderer::drawGlyph(display, cx + (34 - 16) / 2, 24, getKnobGlyph(i), theme_color);

        // Formatted Numeric Value
        uint8_t val = (bank_view_ == HomeKnobBankView::BankB_Engine) ? engine_values_[i] : macro_values_[i];
        char vbuf[8];
        snprintf(vbuf, sizeof(vbuf), "%u", val);
        int16_t vw = FontRenderer::stringWidth(vbuf, FontType::Font5x7);
        FontRenderer::drawString(display, cx + (34 - vw) / 2, 35, vbuf, DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);

        // Vertical Bar Gauge (y=45..71, h=26)
        int16_t bar_x = cx + 8;
        int16_t bar_y = 45;
        int16_t bar_w = 18;
        int16_t bar_h = 26;
        display.drawRect(bar_x, bar_y, bar_w, bar_h, DisplayDriver::kColorDarkGray);

        float norm = (float)val / 127.0f;
        int16_t fill_h = (int16_t)((bar_h - 2) * norm);
        int16_t empty_h = (bar_h - 2) - fill_h;

        if (empty_h > 0) {
            display.fillRect(bar_x + 1, bar_y + 1, bar_w - 2, empty_h, DisplayDriver::kColorBlack);
        }
        if (fill_h > 0) {
            display.fillRect(bar_x + 1, bar_y + 1 + empty_h, bar_w - 2, fill_h, theme_color);
        }
    }
}

void HomeScreen::setWaveformSamples(const int16_t* samples, size_t count) {
    if (!samples || count == 0) {
        scope_sample_count_ = 0;
        return;
    }
    size_t n = (count < sizeof(scope_samples_) / sizeof(scope_samples_[0])) ? count : sizeof(scope_samples_) / sizeof(scope_samples_[0]);
    for (size_t i = 0; i < n; ++i) {
        scope_samples_[i] = samples[i];
    }
    scope_sample_count_ = n;
}

} // namespace smk
