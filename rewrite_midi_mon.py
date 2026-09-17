import re
with open('smk-s3/components/ui/screens/midi_monitor_screen.cpp', 'r') as f:
    content = f.read()

square_logic = """
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
"""

include_prefix = ""
if '#include "ui_theme.h"' not in content:
    include_prefix += '#include "ui_theme.h"\n'
if '#include "ui_components.h"' not in content:
    include_prefix += '#include "ui_components.h"\n'
if include_prefix:
    content = include_prefix + content

start_idx = content.find('if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {')
end_idx = content.find('} else if (dw <= 160) {')

if start_idx == -1 or end_idx == -1:
    raise RuntimeError("Could not find layout markers in smk-s3/components/ui/screens/midi_monitor_screen.cpp")

new_content = content[:start_idx] + square_logic.strip() + '\n    ' + content[end_idx:]

with open('smk-s3/components/ui/screens/midi_monitor_screen.cpp', 'w') as f:
    f.write(new_content)
