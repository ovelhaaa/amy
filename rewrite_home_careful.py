import re

with open('smk-s3/components/ui/screens/home_screen.cpp', 'r') as f:
    content = f.read()

square_logic = """
    if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {
        using namespace theme;

        char patch_buf[40];
        snprintf(patch_buf, sizeof(patch_buf), "P%03u %.20s", patch_number_, patch_name_[0] != '\\0' ? patch_name_ : "DEFAULT");

        char subtitle[32];
        const char knob_bank = observed_knob_bank_ == 1 ? 'A' : (observed_knob_bank_ == 2 ? 'B' : '?');
        const char pad_bank = observed_pad_bank_ == 1 ? 'A' : (observed_pad_bank_ == 2 ? 'B' : '?');
        snprintf(subtitle, sizeof(subtitle), "%s  K:%c P:%c", synth_mode_[0] != '\\0' ? synth_mode_ : "POLY", knob_bank, pad_bank);

        uint16_t theme_color = bank_view_ == HomeKnobBankView::BankB_Engine ? ColorBankB : ColorBankA;

        components::HeaderWidget::draw(display, patch_buf, subtitle, midi_active_, usb_connected_, theme_color);

        // Macros
        for (int i = 0; i < 8; ++i) {
            int16_t col = i % 4;
            int16_t row = i / 4;
            int16_t x = kMargin + col * (kMacroTileWidth + kMacroTileGapX);
            int16_t y = kHeaderHeight + kMargin + row * (kMacroTileHeight + kMacroTileGapY);

            const char* label = gauges_[i].label();
            uint8_t value = bank_view_ == HomeKnobBankView::BankB_Engine ? engine_values_[i] : macro_values_[i];

            components::MacroTile::draw(display, x, y, label, getKnobGlyph(i), value, theme_color);
        }

        // Scope
        int16_t scope_y = kHeaderHeight + kMargin + 2 * (kMacroTileHeight + kMacroTileGapY) + kMargin;
        int16_t scope_h = 56;
        scope_.setPosition(kMargin, scope_y, dw - kMargin * 2, scope_h);
        scope_.setColors(ColorAccentPrimary, ColorSurfaceElev);
        scope_.draw(display);

        // Footer: Tempo & Voices
        int16_t footer_y = scope_y + scope_h + kMargin;

        char bpm_val[8];
        snprintf(bpm_val, sizeof(bpm_val), "%u", static_cast<uint16_t>(bpm_));
        FontRenderer::drawString(display, kMargin, footer_y, bpm_val, ColorTextPrimary, ColorBackground, FontType::FontDisplay, 1);

        int16_t bpm_w = FontRenderer::stringWidth(bpm_val, FontType::FontDisplay, 1);
        FontRenderer::drawString(display, kMargin + bpm_w + 4, footer_y + 14, "BPM", ColorTextMuted, ColorBackground, FontType::Font3x5, 1);

        char voice_buf[16];
        snprintf(voice_buf, sizeof(voice_buf), "V: %u/%u", active_voices_, max_voices_ > 0 ? max_voices_ : 12);
        int16_t voice_w = FontRenderer::stringWidth(voice_buf, FontType::Font3x5, 1);
        FontRenderer::drawString(display, dw - kMargin - voice_w, footer_y + 14, voice_buf, ColorTextSecondary, ColorBackground, FontType::Font3x5, 1);
"""

start_idx = content.find('if (classifyDisplayLayout(dw, dh) == DisplayLayoutClass::Square) {')
end_idx = content.find('} else if (dw <= 160) {')

if '#include "ui_theme.h"' not in content:
    content = '#include "ui_theme.h"\n#include "ui_components.h"\n' + content

new_content = content[:start_idx] + square_logic.strip() + '\n    } else if (dw <= 160) {' + content[end_idx + 23:]
new_content = new_content.replace('scope_.', 'scope.')

with open('smk-s3/components/ui/screens/home_screen.cpp', 'w') as f:
    f.write(new_content)
