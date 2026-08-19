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
    for (int i = 0; i < 8; ++i) {
        gauges_[i].setValue(macro_values_[i]);
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
}

void HomeScreen::setMacroValues(const uint8_t values[8]) {
    if (!values) return;
    for (int i = 0; i < 8; ++i) {
        macro_values_[i] = values[i];
        gauges_[i].setValue(values[i]);
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
    // Refresh animation states if any
}

void HomeScreen::render(DisplayDriver& display) {
    display.fillScreen(DisplayDriver::kColorBlack);

    // Header Line 1: Patch info, Mode, BPM
    char header_str[48];
    snprintf(header_str, sizeof(header_str), "%03u %-12.12s [%s] %.0fBPM", 
             patch_number_, patch_name_, synth_mode_, bpm_);
    FontRenderer::drawString(display, 2, 2, header_str, DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);

    // Live Oscilloscope Box (Center Header: x=140, y=2, w=50, h=18)
    display.drawRect(140, 2, 50, 18, DisplayDriver::kColorMidGray);
    int center_y = 11;
    int prev_y = center_y;
    for (int x = 0; x < 48; ++x) {
        int wave_y = center_y;
        if (active_voices_ > 0 || midi_active_) {
            float angle = (scope_phase_ * 0.3f) + (x * 0.35f);
            float amp = (active_voices_ > 0) ? 6.0f : 2.0f;
            wave_y = center_y + static_cast<int>(sinf(angle) * amp);
        }
        display.drawLine(141 + x, prev_y, 141 + x + 1, wave_y, DisplayDriver::kColorGreen);
        prev_y = wave_y;
    }
    scope_phase_++;

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
    FontRenderer::drawString(display, 2, 14, knob_bank_, DisplayDriver::kColorCyan, DisplayDriver::kColorBlack, FontType::Font5x7);
    display.drawHLine(0, 23, DisplayDriver::kWidth, DisplayDriver::kColorMidGray);

    // Render 8 Bar Gauges for Macros
    for (int i = 0; i < 8; ++i) {
        gauges_[i].draw(display);
    }
}

} // namespace smk
