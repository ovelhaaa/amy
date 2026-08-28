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

void HomeScreen::render(DisplayDriver& display) {
    display.fillScreen(DisplayDriver::kColorBlack);

    int16_t dw = display.width();

    if (dw <= 160) {
        // ── 160x128 (1.8" Display) High-Visibility UI Layout ──

        // Top Header Banner (Dark Gray background, y=0..22)
        display.fillRect(0, 0, 160, 22, DisplayDriver::kColorDarkGray);
        
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

        display.drawHLine(0, 22, 160, DisplayDriver::kColorMidGray);

        // Live Audio Oscilloscope Box (y=25..55, w=154, h=30)
        display.drawRect(3, 25, 154, 30, DisplayDriver::kColorMidGray);
        int center_y = 40;
        int prev_y = center_y;

        // Auto-gain peak detection
        int32_t max_val = 0;
        for (size_t i = 0; i < scope_sample_count_; ++i) {
            int32_t a = (scope_samples_[i] >= 0) ? scope_samples_[i] : -scope_samples_[i];
            if (a > max_val) max_val = a;
        }

        if (scope_sample_count_ > 1 && (max_val > 50 || active_voices_ > 0 || midi_active_)) {
            int32_t scale_divisor = (max_val > 2000) ? max_val : 2000;
            for (int x = 0; x < 152; ++x) {
                size_t s_idx = (x * (scope_sample_count_ - 1)) / 152;
                int32_t val = scope_samples_[s_idx];
                int wave_y = center_y - (int)((val * 13) / scale_divisor);
                if (wave_y < 27) wave_y = 27;
                if (wave_y > 53) wave_y = 53;
                if (x == 0) prev_y = wave_y;
                display.drawLine(4 + x, prev_y, 4 + x + 1, wave_y, DisplayDriver::kColorGreen);
                prev_y = wave_y;
            }
        } else {
            display.drawHLine(4, center_y, 152, DisplayDriver::kColorGreen);
        }

        // 8 Polyphony Voice Indicator Dots (y=57)
        for (uint8_t v = 0; v < max_voices_ && v < 8; ++v) {
            uint16_t voice_col = (v < active_voices_) ? DisplayDriver::kColorCyan : DisplayDriver::kColorDarkGray;
            display.fillRect(4 + v * 8, 57, 6, 4, voice_col);
        }

        display.drawHLine(0, 64, dw, DisplayDriver::kColorMidGray);

        // 8 Macro Gauges adaptively positioned across 160px (y=66..126)
        for (int i = 0; i < 8; ++i) {
            int16_t gx = 2 + i * 19;
            BarGauge g(gx, 66, 18, 59, gauges_[i].label());
            g.setValue(macro_values_[i]);
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
    display.drawRect(140, 2, 50, 18, DisplayDriver::kColorMidGray);
    int center_y = 11;
    int prev_y = center_y;

    int32_t max_val_pan = 0;
    for (size_t i = 0; i < scope_sample_count_; ++i) {
        int32_t a = (scope_samples_[i] >= 0) ? scope_samples_[i] : -scope_samples_[i];
        if (a > max_val_pan) max_val_pan = a;
    }

    if (scope_sample_count_ > 1 && (max_val_pan > 50 || active_voices_ > 0 || midi_active_)) {
        int32_t scale_divisor = (max_val_pan > 2000) ? max_val_pan : 2000;
        for (int x = 0; x < 48; ++x) {
            size_t s_idx = (x * (scope_sample_count_ - 1)) / 48;
            int32_t val = scope_samples_[s_idx];
            int wave_y = center_y - (int)((val * 7) / scale_divisor);
            if (wave_y < 4) wave_y = 4;
            if (wave_y > 18) wave_y = 18;
            if (x == 0) prev_y = wave_y;
            display.drawLine(141 + x, prev_y, 141 + x + 1, wave_y, DisplayDriver::kColorGreen);
            prev_y = wave_y;
        }
    } else {
        display.drawHLine(141, center_y, 48, DisplayDriver::kColorGreen);
    }

    // Voice Activity Meter (x=195..235)
    for (uint8_t v = 0; v < max_voices_ && v < 8; ++v) {
        uint16_t voice_col = (v < active_voices_) ? DisplayDriver::kColorCyan : DisplayDriver::kColorDarkGray;
        display.fillRect(195 + v * 5, 4, 4, 14, voice_col);
    }

    // USB & MIDI Indicators (x=242..280)
    uint16_t usb_col = usb_connected_ ? DisplayDriver::kColorGreen : DisplayDriver::kColorRed;
    display.fillRect(242, 3, 5, 5, usb_col);
    FontRenderer::drawString(display, 249, 2, "USB", DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);

    if (midi_active_) {
        display.fillRect(272, 3, 5, 5, DisplayDriver::kColorYellow);
    }

    // Header Line 2: Active Knob Bank
    uint16_t bank_col = (bank_view_ == HomeKnobBankView::BankB_Engine) ? DisplayDriver::kColorAmber : DisplayDriver::kColorCyan;
    FontRenderer::drawString(display, 2, 14, knob_bank_, bank_col, DisplayDriver::kColorBlack, FontType::Font5x7);
    display.drawHLine(0, 23, display.width(), DisplayDriver::kColorMidGray);

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
