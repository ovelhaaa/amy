#pragma once

#include <cstdint>
#include <cstddef>
#include <algorithm>

namespace smk {

/**
 * ═══════════════════════════════════════════════════════════════════════════
 * COLOR PALETTE & SEMANTIC LEGEND (RGB565)
 * ═══════════════════════════════════════════════════════════════════════════
 * Primary UI Roles:
 *  - kColorBlack     (0x0000) : Background canvas, empty areas.
 *  - kColorWhite     (0xFFFF) : High-contrast primary text, focused cursor.
 *  - kColorLightGray (0xC618) : Secondary text, hints, inactive step indicators.
 *  - kColorMidGray   (0x7BEF) : Outer borders, panel dividers, quarter-note accents.
 *  - kColorDarkGray  (0x31A6) : Header banners, inactive pad backgrounds, frame boxes.
 *
 * Bank & Channel Semantics:
 *  - kColorCyan      (0x07FF) : Bank A (Macros), Selected Track focus, SD (Snare).
 *  - kColorAmber     (0xFBE0) : Bank B (Engine), BD (Bass Drum), Parameter-Lock pips.
 *  - kColorGreen     (0x07E0) : CH (Closed Hat), Waveform scope, Active states.
 *  - kColorDimGreen  (0x03E0) : Active pad background fill.
 *
 * Operational State Semantics:
 *  - kColorGreen     (0x07E0) : PLAY state, Limiter active, Audio/USB OK.
 *  - kColorRed       (0xF800) : RECORD state, Track Muted, USB Disconnected, Audio Underrun.
 *  - kColorYellow    (0xFFE0) : Playhead position, Active triggered pad, Parameter Takeover.
 *  - kColorOrange    (0xFD20) : Warnings, Transient alerts.
 *  - kColorMagenta   (0xF81F) : Special diagnostic flags / unassigned MIDI channels.
 *  - kColorBlue      (0x001F) : Auxiliary indicators.
 * ═══════════════════════════════════════════════════════════════════════════
 */

struct Rect {
    int16_t x{0};
    int16_t y{0};
    int16_t w{0};
    int16_t h{0};

    constexpr bool isEmpty() const { return w <= 0 || h <= 0; }

    void merge(int16_t ox, int16_t oy, int16_t ow, int16_t oh) {
        if (ow <= 0 || oh <= 0) return;
        if (isEmpty()) {
            x = ox;
            y = oy;
            w = ow;
            h = oh;
            return;
        }
        int16_t x2 = std::max((int16_t)(x + w), (int16_t)(ox + ow));
        int16_t y2 = std::max((int16_t)(y + h), (int16_t)(oy + oh));
        x = std::min(x, ox);
        y = std::min(y, oy);
        w = x2 - x;
        h = y2 - y;
    }
};

class DisplayDriver {
public:
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

    // Pure virtual geometry contract - explicit dimensions per driver instance
    virtual int16_t width() const = 0;
    virtual int16_t height() const = 0;

    // Dirty rectangle tracking
    void markDirty(int16_t x, int16_t y, int16_t w, int16_t h);
    void invalidate();
    void clearDirty();
    bool isDirty() const { return is_dirty_; }
    Rect dirtyRect() const { return dirty_rect_; }

protected:
    Rect dirty_rect_{};
    bool is_dirty_{false};
};

} // namespace smk
