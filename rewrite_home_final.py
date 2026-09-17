import re

with open('smk-s3/components/ui/screens/home_screen.cpp', 'r') as f:
    content = f.read()

square_logic = """
    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        using namespace theme;

        char patch_buf[40];
        snprintf(patch_buf, sizeof(patch_buf), "P%03u %.20s", patch_number_,
                 patch_name_[0] != '\\0' ? patch_name_ : "DEFAULT");

        char subtitle[32];
        const char knob_bank = observed_knob_bank_ == 1 ? 'A'
            : (observed_knob_bank_ == 2 ? 'B' : '?');
        const char pad_bank = observed_pad_bank_ == 1 ? 'A'
            : (observed_pad_bank_ == 2 ? 'B' : '?');
        snprintf(subtitle, sizeof(subtitle), "%s  K:%c P:%c",
                 synth_mode_[0] != '\\0' ? synth_mode_ : "POLY", knob_bank, pad_bank);

        const uint16_t theme_color = bank_view_ == HomeKnobBankView::BankB_Engine
            ? ColorBankB : ColorBankA;
        components::HeaderWidget::draw(display, patch_buf, subtitle, midi_active_,
                                       usb_connected_, theme_color);

        for (int i = 0; i < 8; ++i) {
            const int16_t col = i % 4;
            const int16_t row = i / 4;
            const int16_t x = kMargin + col * (kMacroTileWidth + kMacroTileGapX);
            const int16_t y = kHeaderHeight + kMargin
                + row * (kMacroTileHeight + kMacroTileGapY);
            const uint8_t value = bank_view_ == HomeKnobBankView::BankB_Engine
                ? engine_values_[i] : macro_values_[i];
            components::MacroTile::draw(display, x, y, gauges_[i].label(),
                                        getKnobGlyph(i), value, theme_color);
        }

        const int16_t scope_y = kHeaderHeight + kMargin
            + 2 * (kMacroTileHeight + kMacroTileGapY) + kMargin;
        constexpr int16_t scope_h = 56;
        OscilloscopeWidget scope(kMargin, scope_y, dw - kMargin * 2, scope_h);
        scope.setSamples(scope_samples_, scope_sample_count_);
        scope.setActive(active_voices_ > 0 || midi_active_);
        scope.setColors(ColorAccentPrimary, ColorSurfaceElev);
        scope.draw(display);

        const int16_t footer_y = scope_y + scope_h + kMargin;
        char bpm_val[8];
        snprintf(bpm_val, sizeof(bpm_val), "%u", static_cast<uint16_t>(bpm_));
        FontRenderer::drawString(display, kMargin, footer_y, bpm_val, ColorTextPrimary,
                                 ColorBackground, FontType::FontDisplay, 1);
        const int16_t bpm_w = FontRenderer::stringWidth(bpm_val, FontType::FontDisplay, 1);
        FontRenderer::drawString(display, kMargin + bpm_w + 4, footer_y + 14, "BPM",
                                 ColorTextMuted, ColorBackground, FontType::Font3x5, 1);

        char voice_buf[16];
        snprintf(voice_buf, sizeof(voice_buf), "V: %u/%u", active_voices_,
                 max_voices_ > 0 ? max_voices_ : 12);
        const int16_t voice_w = FontRenderer::stringWidth(voice_buf, FontType::Font3x5, 1);
        FontRenderer::drawString(display, dw - kMargin - voice_w, footer_y + 14,
                                 voice_buf, ColorTextSecondary, ColorBackground,
                                 FontType::Font3x5, 1);
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
    print("Skipping smk-s3/components/ui/screens/home_screen.cpp - could not find layout block")
    raise SystemExit(0)

new_content = content[:start_idx] + square_logic.strip() + '\n    ' + content[end_idx:]

with open('smk-s3/components/ui/screens/home_screen.cpp', 'w') as f:
    f.write(new_content)
