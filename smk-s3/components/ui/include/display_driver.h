#pragma once

#include <cstdint>
#include <cstddef>

namespace smk {

class DisplayDriver {
public:
    static constexpr int16_t kWidth = 284;
    static constexpr int16_t kHeight = 76;

    // RGB565 Color Constants
    static constexpr uint16_t kColorBlack     = 0x0000;
    static constexpr uint16_t kColorWhite     = 0xFFFF;
    static constexpr uint16_t kColorRed       = 0xF800;
    static constexpr uint16_t kColorGreen     = 0x07E0;
    static constexpr uint16_t kColorBlue      = 0x001F;
    static constexpr uint16_t kColorYellow    = 0xFFE0;
    static constexpr uint16_t kColorCyan      = 0x07FF;
    static constexpr uint16_t kColorMagenta   = 0xF81F;
    static constexpr uint16_t kColorOrange    = 0xFD20;
    static constexpr uint16_t kColorDarkGray  = 0x31A6;
    static constexpr uint16_t kColorMidGray   = 0x7BEF;
    static constexpr uint16_t kColorLightGray = 0xC618;
    static constexpr uint16_t kColorAmber     = 0xFBE0;
    static constexpr uint16_t kColorDimGreen  = 0x03E0;

    virtual ~DisplayDriver() = default;

    virtual bool begin() = 0;
    virtual void setBrightness(uint8_t value) = 0;
    virtual void fillScreen(uint16_t color) = 0;
    virtual void drawPixel(int16_t x, int16_t y, uint16_t color) = 0;

    virtual void drawHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
    virtual void drawVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
    virtual void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
    virtual void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    virtual void drawBitmap(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* pixels);

    virtual void flush() = 0;
    virtual void flushRegion(int16_t x, int16_t y, int16_t w, int16_t h) = 0;

    virtual int16_t width() const { return kWidth; }
    virtual int16_t height() const { return kHeight; }
};

} // namespace smk
