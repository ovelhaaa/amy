#pragma once

#include "display_driver.h"
#include <cstdint>
#include <cstddef>

namespace smk {

enum class FontType : uint8_t {
    Font3x5,
    Font5x7,
    Font8x12
};

class FontRenderer {
public:
    static void drawChar(DisplayDriver& display, int16_t x, int16_t y, char c, 
                         uint16_t fg_color, uint16_t bg_color = 0x0000, 
                         FontType font = FontType::Font5x7, uint8_t scale = 1);

    static void drawString(DisplayDriver& display, int16_t x, int16_t y, const char* str, 
                           uint16_t fg_color, uint16_t bg_color = 0x0000, 
                           FontType font = FontType::Font5x7, uint8_t scale = 1);

    static int16_t stringWidth(const char* str, FontType font = FontType::Font5x7, uint8_t scale = 1);
    static int16_t fontHeight(FontType font = FontType::Font5x7, uint8_t scale = 1);
};

} // namespace smk
