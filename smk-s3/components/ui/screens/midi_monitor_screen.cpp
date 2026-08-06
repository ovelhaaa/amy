#include "midi_monitor_screen.h"
#include "font_renderer.h"
#include <cstdio>

namespace smk {

MidiMonitorScreen::MidiMonitorScreen() {
    for (size_t i = 0; i < kHistorySize; ++i) {
        history_[i].valid = false;
    }
}

void MidiMonitorScreen::addEvent(const SynthEvent& event) {
    history_[head_].event = event;
    history_[head_].valid = true;
    head_ = (head_ + 1) % kHistorySize;
}

void MidiMonitorScreen::setMidiLearnActive(bool active) {
    learn_active_ = active;
}

void MidiMonitorScreen::update() {
}

static const char* eventTypeToString(EventType type) {
    switch (type) {
        case EventType::NoteOn: return "NOTE ON ";
        case EventType::NoteOff: return "NOTE OFF";
        case EventType::PitchBend: return "PITCHBND";
        case EventType::Modulation: return "MODULATN";
        case EventType::ControlChange: return "CC CHG  ";
        case EventType::ProgramChange: return "PROG CHG";
        case EventType::AllNotesOff: return "ALL OFF ";
        case EventType::Panic: return "PANIC!  ";
        default: return "OTHER   ";
    }
}

void MidiMonitorScreen::render(DisplayDriver& display) {
    display.fillScreen(DisplayDriver::kColorBlack);

    // Title bar
    FontRenderer::drawString(display, 2, 2, "MIDI MONITOR [USB HOST: SMK25 V2]", DisplayDriver::kColorCyan, DisplayDriver::kColorBlack, FontType::Font5x7);
    const char* learn_str = learn_active_ ? "[LEARN: ON]" : "[LEARN: OFF]";
    uint16_t learn_col = learn_active_ ? DisplayDriver::kColorYellow : DisplayDriver::kColorMidGray;
    FontRenderer::drawString(display, 210, 2, learn_str, learn_col, DisplayDriver::kColorBlack, FontType::Font5x7);
    display.drawHLine(0, 11, DisplayDriver::kWidth, DisplayDriver::kColorMidGray);

    // Render history entries (up to 4 lines)
    int y = 16;
    for (size_t i = 0; i < kHistorySize; ++i) {
        size_t idx = (head_ + kHistorySize - 1 - i) % kHistorySize;
        if (!history_[idx].valid) continue;

        const auto& ev = history_[idx].event;
        char line_buf[64];
        snprintf(line_buf, sizeof(line_buf), "CH%02u | %s | ID:%03u | VAL:%05ld | TS:%08lums",
                 ev.channel + 1, eventTypeToString(ev.type), ev.id, (long)ev.value, (long)(ev.timestamp_us / 1000));
        
        uint16_t col = (i == 0) ? DisplayDriver::kColorWhite : DisplayDriver::kColorMidGray;
        FontRenderer::drawString(display, 2, y, line_buf, col, DisplayDriver::kColorBlack, FontType::Font5x7);
        y += 14;
    }
}

} // namespace smk
