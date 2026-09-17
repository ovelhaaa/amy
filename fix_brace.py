with open('smk-s3/components/ui/screens/home_screen.cpp', 'r') as f:
    content = f.read()

target = """        char voice_buf[16];
        snprintf(voice_buf, sizeof(voice_buf), "V: %u/%u", active_voices_, max_voices_ > 0 ? max_voices_ : 12);
        int16_t voice_w = FontRenderer::stringWidth(voice_buf, FontType::Font3x5, 1);
        FontRenderer::drawString(display, dw - kMargin - voice_w, footer_y + 14, voice_buf, ColorTextSecondary, ColorBackground, FontType::Font3x5, 1);
    } else if (dw <= 160) {"""

replacement = """        char voice_buf[16];
        snprintf(voice_buf, sizeof(voice_buf), "V: %u/%u", active_voices_, max_voices_ > 0 ? max_voices_ : 12);
        int16_t voice_w = FontRenderer::stringWidth(voice_buf, FontType::Font3x5, 1);
        FontRenderer::drawString(display, dw - kMargin - voice_w, footer_y + 14, voice_buf, ColorTextSecondary, ColorBackground, FontType::Font3x5, 1);
    } else if (dw <= 160) {"""

# The brace is actually present in '    } else if' so let's see why there is an error about 'namespace smk'
