#include "home_screen.h"
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
    constexpr int16_t kScopeX = 174;
    constexpr int16_t kScopeY = 2;
    constexpr int16_t kScopeW = 44;
    constexpr int16_t kScopeH = 9;
    constexpr int16_t kVoiceX = 222;
    constexpr int16_t kVoiceY = 3;
    constexpr int16_t kUsbX = 264;
    constexpr int16_t kUsbY = 3;
    constexpr int16_t kMidiX = 276;
    constexpr int16_t kMidiY = 3;
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

    // 1. Header Line 1: Patch info, Mode, BPM (y=0..12)
    char patch_buf[32];
    snprintf(patch_buf, sizeof(patch_buf), "%03u %-14.14s", patch_number_, patch_name_);
    FontRenderer::drawString(display, 3, 3, patch_buf, DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);

    char mode_buf[16];
    snprintf(mode_buf, sizeof(mode_buf), "[%s]", synth_mode_);
    FontRenderer::drawString(display, 96, 3, mode_buf, DisplayDriver::kColorCyan, DisplayDriver::kColorBlack, FontType::Font5x7);

    char bpm_buf[16];
    snprintf(bpm_buf, sizeof(bpm_buf), "%.1fBPM", bpm_ > 0 ? bpm_ : 120.0f);
    FontRenderer::drawString(display, 128, 3, bpm_buf, DisplayDriver::kColorAmber, DisplayDriver::kColorBlack, FontType::Font5x7);

    // Mini Live Audio Scope in Header (x=174, y=2, w=44, h=9)
    OscilloscopeWidget scope_pan(wide_layout::kScopeX, wide_layout::kScopeY,
                                 wide_layout::kScopeW, wide_layout::kScopeH);
    scope_pan.setSamples(scope_samples_, scope_sample_count_);
    scope_pan.setActive(active_voices_ > 0 || midi_active_);
    scope_pan.draw(display);

    // Voice Activity Meter (x=222..258)
    for (uint8_t v = 0; v < max_voices_ && v < 8; ++v) {
        uint16_t voice_col = (v < active_voices_) ? DisplayDriver::kColorCyan : DisplayDriver::kColorDarkGray;
        display.fillRect(wide_layout::kVoiceX + v * 5, wide_layout::kVoiceY, 3, 7, voice_col);
    }

    // USB & MIDI Indicators (x=264..282)
    uint16_t usb_col = usb_connected_ ? DisplayDriver::kColorGreen : DisplayDriver::kColorRed;
    display.fillRect(wide_layout::kUsbX, wide_layout::kUsbY, 5, 5, usb_col);
    FontRenderer::drawString(display, 270, 2, "U", DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font3x5);

    if (midi_active_) {
        display.fillRect(wide_layout::kMidiX, wide_layout::kMidiY, 5, 5, DisplayDriver::kColorYellow);
    }

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
