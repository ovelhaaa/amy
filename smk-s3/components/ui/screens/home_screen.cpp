#include "home_screen.h"
#include "font_renderer.h"
#include <cstdio>
#include <cstring>

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

void HomeScreen::update() {
    // Refresh animation states if any
}

void HomeScreen::render(DisplayDriver& display) {
    display.fillScreen(DisplayDriver::kColorBlack);

    // Header Line 1: Patch info, Mode, BPM, USB & MIDI status
    char header_str[64];
    snprintf(header_str, sizeof(header_str), "%03u %-14.14s [%s] %.1fBPM", 
             patch_number_, patch_name_, synth_mode_, bpm_);
    FontRenderer::drawString(display, 2, 2, header_str, DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);

    // USB Status indicator
    uint16_t usb_col = usb_connected_ ? DisplayDriver::kColorGreen : DisplayDriver::kColorRed;
    display.fillRect(245, 3, 5, 5, usb_col);
    FontRenderer::drawString(display, 252, 2, "USB", DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);

    // MIDI Activity dot
    if (midi_active_) {
        display.fillRect(275, 3, 5, 5, DisplayDriver::kColorYellow);
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
