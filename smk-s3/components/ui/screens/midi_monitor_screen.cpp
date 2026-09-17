#include "ui_theme.h"
#include "ui_components.h"
#include "midi_monitor_screen.h"
#include "display_layout.h"
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
    int16_t dw = display.width();
    int16_t dh = display.height();

    // Title bar
    FontRenderer::drawString(display, 2, 2, "MIDI MONITOR", DisplayDriver::kColorCyan, DisplayDriver::kColorBlack, FontType::Font5x7);
    const char* learn_str = learn_active_ ? "[LRN:ON]" : "[LRN:OFF]";
    uint16_t learn_col = learn_active_ ? DisplayDriver::kColorYellow : DisplayDriver::kColorMidGray;
    int16_t lx = dw - 2 - FontRenderer::stringWidth(learn_str, FontType::Font5x7);
    FontRenderer::drawString(display, lx, 2, learn_str, learn_col, DisplayDriver::kColorBlack, FontType::Font5x7);
    display.drawHLine(0, 11, dw, DisplayDriver::kColorMidGray);

    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        using namespace theme;
        components::HeaderWidget::draw(display, "MIDI MONITOR",
                                       learn_active_ ? "LEARN ON" : "LEARN OFF",
                                       false, false, ColorAccentSecondary);

        const int16_t start_y = kHeaderHeight + kMargin;
        display.fillChamferRect(kMargin, start_y, dw - 2 * kMargin,
                                dh - start_y - kMargin, 4, ColorSurfaceElev);

        size_t rendered = 0;
        for (size_t i = 0; i < kHistorySize; ++i) {
            const size_t idx = (head_ + kHistorySize - 1 - i) % kHistorySize;
            if (!history_[idx].valid) continue;
            const auto& event = history_[idx].event;
            const int16_t y = start_y + 8 + static_cast<int16_t>(rendered * 38);

            char primary[32];
            snprintf(primary, sizeof(primary), "%s CH %02u",
                     eventTypeToString(event.type), event.channel + 1);
            FontRenderer::drawString(display, kMargin + 8, y, primary,
                                     rendered == 0 ? ColorTextPrimary : ColorTextSecondary,
                                     ColorSurfaceElev, FontType::Font5x7, 1);

            char detail[48];
            snprintf(detail, sizeof(detail), "ID %03u  VAL %05ld",
                     event.id, static_cast<long>(event.value));
            FontRenderer::drawString(display, kMargin + 8, y + 13, detail,
                                     ColorTextMuted, ColorSurfaceElev,
                                     FontType::Font3x5, 1);
            ++rendered;
        }

        if (rendered == 0) {
            FontRenderer::drawString(display, 42, 108, "WAITING FOR MIDI...",
                                     ColorTextMuted, ColorSurfaceElev,
                                     FontType::Font5x7, 1);
        }
        return;
    }
    // Render history entries
    int y = 16;
    size_t max_lines = (dh <= 128) ? 8 : 4;
    for (size_t i = 0; i < kHistorySize && i < max_lines; ++i) {
        size_t idx = (head_ + kHistorySize - 1 - i) % kHistorySize;
        if (!history_[idx].valid) continue;

        const auto& ev = history_[idx].event;
        char line_buf[64];
        if (dw <= 160) {
            snprintf(line_buf, sizeof(line_buf), "C%02u %s #%03u V:%03ld",
                     ev.channel + 1, eventTypeToString(ev.type), ev.id, (long)ev.value);
        } else {
            snprintf(line_buf, sizeof(line_buf), "CH%02u | %s | ID:%03u | VAL:%05ld | TS:%08lums",
                     ev.channel + 1, eventTypeToString(ev.type), ev.id, (long)ev.value, (long)(ev.timestamp_us / 1000));
        }
        
        uint16_t col = (i == 0) ? DisplayDriver::kColorWhite : DisplayDriver::kColorMidGray;
        FontRenderer::drawString(display, 2, y, line_buf, col, DisplayDriver::kColorBlack, FontType::Font5x7);
        y += 13;
    }
}

} // namespace smk
