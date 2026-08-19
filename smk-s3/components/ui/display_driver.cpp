#include "display_driver.h"
#include <algorithm>

namespace smk {

void DisplayDriver::drawHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    if (y < 0 || y >= kHeight || x >= kWidth || w <= 0) return;
    int16_t x2 = std::min((int16_t)(x + w - 1), (int16_t)(kWidth - 1));
    int16_t x1 = std::max(x, (int16_t)0);
    for (int16_t ix = x1; ix <= x2; ++ix) {
        drawPixel(ix, y, color);
    }
}

void DisplayDriver::drawVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    if (x < 0 || x >= kWidth || y >= kHeight || h <= 0) return;
    int16_t y2 = std::min((int16_t)(y + h - 1), (int16_t)(kHeight - 1));
    int16_t y1 = std::max(y, (int16_t)0);
    for (int16_t iy = y1; iy <= y2; ++iy) {
        drawPixel(x, iy, color);
    }
}

void DisplayDriver::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;

    while (true) {
        drawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void DisplayDriver::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    drawHLine(x, y, w, color);
    drawHLine(x, y + h - 1, w, color);
    drawVLine(x, y, h, color);
    drawVLine(x + w - 1, y, h, color);
}

void DisplayDriver::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    int16_t y2 = std::min((int16_t)(y + h - 1), (int16_t)(kHeight - 1));
    int16_t y1 = std::max(y, (int16_t)0);
    for (int16_t iy = y1; iy <= y2; ++iy) {
        drawHLine(x, iy, w, color);
    }
}

void DisplayDriver::drawBitmap(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t* pixels) {
    if (!pixels || w <= 0 || h <= 0) return;
    for (int16_t iy = 0; iy < h; ++iy) {
        for (int16_t ix = 0; ix < w; ++ix) {
            int16_t px = x + ix;
            int16_t py = y + iy;
            if (px >= 0 && px < kWidth && py >= 0 && py < kHeight) {
                drawPixel(px, py, pixels[iy * w + ix]);
            }
        }
    }
}

} // namespace smk
