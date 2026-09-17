import re
with open('smk-s3/components/ui/screens/system_screen.cpp', 'r') as f:
    content = f.read()

square_logic = """
    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        using namespace theme;
        components::HeaderWidget::draw(display, "SYSTEM", "",
                                       false, false, ColorAccentSecondary);

        char cpu_value[32];
        char memory_value[32];
        char audio_value[32];
        char midi_value[32];
        char usb_value[32];
        char control_value[32];
        snprintf(cpu_value, sizeof(cpu_value), "%luMHz / %.1f%% / V%lu",
                 static_cast<unsigned long>(snap.cpu_freq_mhz), snap.render_load,
                 static_cast<unsigned long>(snap.active_voices));
        snprintf(memory_value, sizeof(memory_value), "%luK / %.1fM",
                 static_cast<unsigned long>(snap.free_internal_ram / 1024),
                 snap.free_psram / (1024.0f * 1024.0f));
        snprintf(audio_value, sizeof(audio_value), "UND %lu / MAX %.1fms",
                 static_cast<unsigned long>(snap.audio_underruns),
                 snap.max_render_us / 1000.0f);
        snprintf(midi_value, sizeof(midi_value), "ERR %lu / Q %lu",
                 static_cast<unsigned long>(snap.midi_parse_errors),
                 static_cast<unsigned long>(snap.event_queue_overflows));
        snprintf(usb_value, sizeof(usb_value), "%s / DC %lu",
                 snap.usb_connected ? "CONNECTED" : "NO DEVICE",
                 static_cast<unsigned long>(snap.usb_disconnects));
        snprintf(control_value, sizeof(control_value), "%.10s / SW %u / %s",
                 vel_curve_, swing_, limiter_ ? "LIM" : "NO LIM");

        const char* item_names_[] = {
            "ENGINE", "MEMORY", "AUDIO", "MIDI", "USB", "CONTROL"
        };
        const char* item_values_[] = {
            cpu_value, memory_value, audio_value, midi_value, usb_value, control_value
        };
        const size_t row_count = sizeof(item_names_) / sizeof(item_names_[0]);

        constexpr int16_t box_x = kMargin;
        const int16_t box_y = kHeaderHeight + kMargin;
        const int16_t box_w = dw - 2 * kMargin;
        constexpr int16_t row_h = 30;
        display.fillChamferRect(box_x, box_y, box_w,
                                static_cast<int16_t>(row_count * row_h + 8),
                                4, ColorSurfaceElev);

        for (size_t i = 0; i < row_count; ++i) {
            const int16_t y = box_y + 8 + static_cast<int16_t>(i * row_h);
            FontRenderer::drawString(display, box_x + 8, y, item_names_[i],
                                     ColorTextSecondary, ColorSurfaceElev,
                                     FontType::Font3x5, 1);
            FontRenderer::drawString(display, box_x + 72, y, item_values_[i],
                                     ColorTextPrimary, ColorSurfaceElev,
                                     FontType::Font5x7, 1);
            if (i + 1 < row_count) {
                display.drawHLine(box_x + 8, y + 20, box_w - 16, ColorDivider);
            }
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
    raise RuntimeError("Could not find layout markers in smk-s3/components/ui/screens/system_screen.cpp")

new_content = content[:start_idx] + square_logic.strip() + '\n    ' + content[end_idx:]

with open('smk-s3/components/ui/screens/system_screen.cpp', 'w') as f:
    f.write(new_content)
