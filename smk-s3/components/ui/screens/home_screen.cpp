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
    constexpr int16_t kHeaderH = 22;
    constexpr int16_t kScopeX = 3;
    constexpr int16_t kScopeY = 25;
    constexpr int16_t kScopeW = 154;
    constexpr int16_t kScopeH = 30;
    constexpr int16_t kVoiceY = 57;
    constexpr int16_t kGaugeY = 66;
    constexpr int16_t kGaugeW = 18;
    constexpr int16_t kGaugeH = 59;
}
namespace wide_layout {
    constexpr int16_t kHeaderDividerY = 23;
    constexpr int16_t kScopeX = 140;
    constexpr int16_t kScopeY = 2;
    constexpr int16_t kScopeW = 50;
    constexpr int16_t kScopeH = 18;
    constexpr int16_t kVoiceX = 195;
    constexpr int16_t kVoiceY = 4;
    constexpr int16_t kUsbX = 242;
    constexpr int16_t kUsbY = 3;
    constexpr int16_t kMidiX = 272;
    constexpr int16_t kMidiY = 3;
}
} // anonymous namespace

void HomeScreen::render(DisplayDriver& display) {
    display.fillScreen(DisplayDriver::kColorBlack);

    int16_t dw = display.width();

    if (dw <= 160) {
        // ── 160x128 (1.8" Display) High-Visibility UI Layout ──

        // Top Header Banner (Dark Gray background, y=0..22)
        display.fillRect(0, 0, 160, compact_layout::kHeaderH, DisplayDriver::kColorDarkGray);
        
        // Line 1: Patch number & FULL patch name (up to 20 chars) in high contrast white
        const char* name = (patch_name_[0] != '\0') ? patch_name_ : "DEFAULT PATCH";
        char p_str[36];
        snprintf(p_str, sizeof(p_str), "P%03u %-18.18s", patch_number_, name);
        FontRenderer::drawString(display, 3, 3, p_str, DisplayDriver::kColorWhite, DisplayDriver::kColorDarkGray, FontType::Font5x7);

        // Status Indicators on top right (USB & MIDI LEDs)
        uint16_t usb_col = usb_connected_ ? DisplayDriver::kColorGreen : DisplayDriver::kColorRed;
        display.fillRect(144, 3, 6, 6, usb_col);
        if (midi_active_) {
            display.fillRect(152, 3, 6, 6, DisplayDriver::kColorYellow);
        }

        // Line 2: Synth Mode, BPM, Knob Bank
        char b_str[36];
        const char* s_mode = (synth_mode_[0] != '\0') ? synth_mode_ : "SYNTH";
        snprintf(b_str, sizeof(b_str), "[%s] %.0fBPM %-14.14s", s_mode, bpm_ > 0 ? bpm_ : 120.0f, knob_bank_);
        FontRenderer::drawString(display, 3, 12, b_str, DisplayDriver::kColorAmber, DisplayDriver::kColorDarkGray, FontType::Font5x7);

        display.drawHLine(0, compact_layout::kHeaderH, 160, DisplayDriver::kColorMidGray);

        // Live Audio Oscilloscope
        OscilloscopeWidget scope(compact_layout::kScopeX, compact_layout::kScopeY,
                                 compact_layout::kScopeW, compact_layout::kScopeH);
        scope.setSamples(scope_samples_, scope_sample_count_);
        scope.setActive(active_voices_ > 0 || midi_active_);
        scope.draw(display);

        // 8 Polyphony Voice Indicator Dots (y=57)
        for (uint8_t v = 0; v < max_voices_ && v < 8; ++v) {
            uint16_t voice_col = (v < active_voices_) ? DisplayDriver::kColorCyan : DisplayDriver::kColorDarkGray;
            display.fillRect(4 + v * 8, compact_layout::kVoiceY, 6, 4, voice_col);
        }

        display.drawHLine(0, 64, dw, DisplayDriver::kColorMidGray);

        // 8 Gauges adaptively positioned across 160px (y=66..126)
        for (int i = 0; i < 8; ++i) {
            int16_t gx = 2 + i * 19;
            BarGauge g(gx, compact_layout::kGaugeY, compact_layout::kGaugeW, compact_layout::kGaugeH, gauges_[i].label());
            if (bank_view_ == HomeKnobBankView::BankB_Engine) {
                g.setValue(engine_values_[i]);
                g.setColors(DisplayDriver::kColorAmber, DisplayDriver::kColorWhite);
            } else {
                g.setValue(macro_values_[i]);
                g.setColors(DisplayDriver::kColorCyan, DisplayDriver::kColorWhite);
            }
            g.draw(display);
        }
        return;
    }

    // ── 284x76 Panoramic Layout ──
    char header_str[48];
    snprintf(header_str, sizeof(header_str), "%03u %-12.12s [%s] %.0fBPM", 
             patch_number_, patch_name_, synth_mode_, bpm_);
    FontRenderer::drawString(display, 2, 2, header_str, DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);

    // Live Audio Oscilloscope Box (Center Header: x=140, y=2, w=50, h=18)
    OscilloscopeWidget scope_pan(wide_layout::kScopeX, wide_layout::kScopeY,
                                 wide_layout::kScopeW, wide_layout::kScopeH);
    scope_pan.setSamples(scope_samples_, scope_sample_count_);
    scope_pan.setActive(active_voices_ > 0 || midi_active_);
    scope_pan.draw(display);

    // Voice Activity Meter (x=195..235)
    for (uint8_t v = 0; v < max_voices_ && v < 8; ++v) {
        uint16_t voice_col = (v < active_voices_) ? DisplayDriver::kColorCyan : DisplayDriver::kColorDarkGray;
        display.fillRect(wide_layout::kVoiceX + v * 5, wide_layout::kVoiceY, 4, 14, voice_col);
    }

    // USB & MIDI Indicators (x=242..280)
    uint16_t usb_col = usb_connected_ ? DisplayDriver::kColorGreen : DisplayDriver::kColorRed;
    display.fillRect(wide_layout::kUsbX, wide_layout::kUsbY, 5, 5, usb_col);
    FontRenderer::drawString(display, 249, 2, "USB", DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);

    if (midi_active_) {
        display.fillRect(wide_layout::kMidiX, wide_layout::kMidiY, 5, 5, DisplayDriver::kColorYellow);
    }

    // Header Line 2: Active Knob Bank
    uint16_t bank_col = (bank_view_ == HomeKnobBankView::BankB_Engine) ? DisplayDriver::kColorAmber : DisplayDriver::kColorCyan;
    FontRenderer::drawString(display, 2, 14, knob_bank_, bank_col, DisplayDriver::kColorBlack, FontType::Font5x7);
    display.drawHLine(0, wide_layout::kHeaderDividerY, display.width(), DisplayDriver::kColorMidGray);

    // Render 8 Bar Gauges for Macros
    for (int i = 0; i < 8; ++i) {
        gauges_[i].draw(display);
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
