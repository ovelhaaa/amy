#include "widgets.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace smk {

// --- Label ---
Label::Label(int16_t x, int16_t y, const char* text, uint16_t fg_color, uint16_t bg_color, FontType font, uint8_t scale)
    : x_(x), y_(y), fg_color_(fg_color), bg_color_(bg_color), font_(font), scale_(scale) {
    setText(text);
}

void Label::setText(const char* text) {
    if (text) {
        snprintf(text_, sizeof(text_), "%s", text);
    } else {
        text_[0] = '\0';
    }
}

void Label::setPosition(int16_t x, int16_t y) {
    x_ = x; y_ = y;
}

void Label::setColor(uint16_t fg, uint16_t bg) {
    fg_color_ = fg; bg_color_ = bg;
}

void Label::draw(DisplayDriver& display) {
    FontRenderer::drawString(display, x_, y_, text_, fg_color_, bg_color_, font_, scale_);
}


// --- ProgressBar ---
ProgressBar::ProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t fill_color, uint16_t border_color)
    : x_(x), y_(y), w_(w), h_(h), fill_color_(fill_color), border_color_(border_color) {}

void ProgressBar::setValue(float norm_val) {
    norm_val_ = std::max(0.0f, std::min(1.0f, norm_val));
}

void ProgressBar::setColors(uint16_t fill, uint16_t border) {
    fill_color_ = fill; border_color_ = border;
}

void ProgressBar::draw(DisplayDriver& display) {
    display.drawRect(x_, y_, w_, h_, border_color_);
    int16_t fill_w = (int16_t)((w_ - 2) * norm_val_);
    if (fill_w > 0) {
        display.fillRect(x_ + 1, y_ + 1, fill_w, h_ - 2, fill_color_);
    }
    int16_t empty_w = (w_ - 2) - fill_w;
    if (empty_w > 0) {
        display.fillRect(x_ + 1 + fill_w, y_ + 1, empty_w, h_ - 2, DisplayDriver::kColorBlack);
    }
}


// --- BarGauge ---
BarGauge::BarGauge(int16_t x, int16_t y, int16_t w, int16_t h, const char* label)
    : x_(x), y_(y), w_(w), h_(h) {
    setLabel(label);
}

void BarGauge::setValue(uint8_t value) {
    value_ = std::min(value, (uint8_t)127);
}

void BarGauge::setLabel(const char* label) {
    if (label) snprintf(label_, sizeof(label_), "%s", label);
    else label_[0] = '\0';
}

void BarGauge::setColors(uint16_t bar_color, uint16_t label_color) {
    bar_color_ = bar_color; label_color_ = label_color;
}

void BarGauge::draw(DisplayDriver& display) {
    // Select compact 3x5 font for narrow gauges to prevent overlap
    FontType font = (w_ <= 24) ? FontType::Font3x5 : FontType::Font5x7;
    int16_t char_step = (font == FontType::Font3x5) ? 4 : 6;

    // Center the label horizontally within the gauge width
    int16_t lbl_len = (int16_t)strlen(label_);
    int16_t lbl_w = lbl_len > 0 ? (lbl_len * char_step - 1) : 0;
    int16_t lbl_x = x_ + (w_ - lbl_w) / 2;
    if (lbl_x < x_) lbl_x = x_;

    // Draw label at top
    FontRenderer::drawString(display, lbl_x, y_, label_, label_color_, DisplayDriver::kColorBlack, font);

    // Center numeric value
    char val_str[8];
    snprintf(val_str, sizeof(val_str), "%u", value_);
    int16_t val_len = (int16_t)strlen(val_str);
    int16_t val_w = val_len > 0 ? (val_len * char_step - 1) : 0;
    int16_t val_x = x_ + (w_ - val_w) / 2;
    if (val_x < x_) val_x = x_;

    int16_t num_y = (font == FontType::Font3x5) ? (y_ + 7) : (y_ + 9);
    FontRenderer::drawString(display, val_x, num_y, val_str, DisplayDriver::kColorYellow, DisplayDriver::kColorBlack, font);

    // Vertical gauge bar
    int16_t bar_y = (font == FontType::Font3x5) ? (y_ + 14) : (y_ + 18);
    int16_t bar_h = h_ - ((font == FontType::Font3x5) ? 14 : 18);
    display.drawRect(x_, bar_y, w_, bar_h, DisplayDriver::kColorMidGray);

    float norm = (float)value_ / 127.0f;
    int16_t fill_h = (int16_t)((bar_h - 2) * norm);
    int16_t empty_h = (bar_h - 2) - fill_h;

    if (empty_h > 0) {
        display.fillRect(x_ + 1, bar_y + 1, w_ - 2, empty_h, DisplayDriver::kColorBlack);
    }
    if (fill_h > 0) {
        display.fillRect(x_ + 1, bar_y + 1 + empty_h, w_ - 2, fill_h, bar_color_);
    }
}


// --- BorderBox ---
BorderBox::BorderBox(int16_t x, int16_t y, int16_t w, int16_t h, const char* title, uint16_t border_color)
    : x_(x), y_(y), w_(w), h_(h), border_color_(border_color) {
    setTitle(title);
}

void BorderBox::setTitle(const char* title) {
    if (title) snprintf(title_, sizeof(title_), "%s", title);
    else title_[0] = '\0';
}

void BorderBox::draw(DisplayDriver& display) {
    display.drawRect(x_, y_, w_, h_, border_color_);
    if (title_[0] != '\0') {
        int16_t tw = FontRenderer::stringWidth(title_, FontType::Font5x7);
        display.fillRect(x_ + 6, y_, tw + 4, 1, DisplayDriver::kColorBlack);
        FontRenderer::drawString(display, x_ + 8, y_ - 3, title_, DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);
    }
}


// --- StatusIndicator ---
StatusIndicator::StatusIndicator(int16_t x, int16_t y, const char* label)
    : x_(x), y_(y) {
    if (label) snprintf(label_, sizeof(label_), "%s", label);
    else label_[0] = '\0';
}

void StatusIndicator::setActive(bool active) {
    active_ = active;
}

void StatusIndicator::setColors(uint16_t active_color, uint16_t inactive_color) {
    active_color_ = active_color; inactive_color_ = inactive_color;
}

void StatusIndicator::draw(DisplayDriver& display) {
    uint16_t dot_color = active_ ? active_color_ : inactive_color_;
    display.fillRect(x_, y_ + 1, 5, 5, dot_color);
    if (label_[0] != '\0') {
        FontRenderer::drawString(display, x_ + 7, y_, label_, DisplayDriver::kColorWhite, DisplayDriver::kColorBlack, FontType::Font5x7);
    }
}


// --- OscilloscopeWidget ---
OscilloscopeWidget::OscilloscopeWidget(int16_t x, int16_t y, int16_t w, int16_t h,
                                       uint16_t wave_color, uint16_t border_color)
    : x_(x), y_(y), w_(w), h_(h), wave_color_(wave_color), border_color_(border_color) {}

void OscilloscopeWidget::setPosition(int16_t x, int16_t y, int16_t w, int16_t h) {
    x_ = x; y_ = y; w_ = w; h_ = h;
}

void OscilloscopeWidget::setSamples(const int16_t* samples, size_t count) {
    if (!samples || count == 0) {
        sample_count_ = 0;
        return;
    }
    size_t n = std::min(count, sizeof(samples_) / sizeof(samples_[0]));
    for (size_t i = 0; i < n; ++i) {
        samples_[i] = samples[i];
    }
    sample_count_ = n;
}

void OscilloscopeWidget::setActive(bool active) {
    active_ = active;
}

void OscilloscopeWidget::setColors(uint16_t wave_color, uint16_t border_color) {
    wave_color_ = wave_color;
    border_color_ = border_color;
}

void OscilloscopeWidget::draw(DisplayDriver& display) {
    if (w_ <= 2 || h_ <= 2) return;
    display.drawRect(x_, y_, w_, h_, border_color_);

    int16_t inner_w = w_ - 2;
    int16_t inner_h = h_ - 2;
    int16_t center_y = y_ + 1 + inner_h / 2;
    int16_t max_amp = inner_h / 2;

    // Peak detection for auto-gain
    int32_t max_val = 0;
    for (size_t i = 0; i < sample_count_; ++i) {
        int32_t a = (samples_[i] >= 0) ? samples_[i] : -samples_[i];
        if (a > max_val) max_val = a;
    }

    if (sample_count_ > 1 && (max_val > 50 || active_)) {
        int32_t scale_divisor = (max_val > 2000) ? max_val : 2000;
        int16_t prev_y = center_y;
        for (int16_t ix = 0; ix < inner_w; ++ix) {
            size_t s_idx = (sample_count_ > 1) ? (size_t)((ix * (sample_count_ - 1)) / inner_w) : 0;
            int32_t val = samples_[s_idx];
            int16_t wave_y = center_y - (int16_t)((val * max_amp) / scale_divisor);
            if (wave_y < y_ + 1) wave_y = y_ + 1;
            if (wave_y > y_ + h_ - 2) wave_y = y_ + h_ - 2;
            if (ix == 0) prev_y = wave_y;
            display.drawLine(x_ + 1 + ix, prev_y, x_ + 1 + ix + 1, wave_y, wave_color_);
            prev_y = wave_y;
        }
    } else {
        display.drawHLine(x_ + 1, center_y, inner_w, wave_color_);
    }
}

// --- GlyphRenderer ---
void GlyphRenderer::drawGlyph(DisplayDriver& display, int16_t x, int16_t y, ParametricGlyph glyph, uint16_t color) {
    switch (glyph) {
        case ParametricGlyph::Lowpass:
            display.drawHLine(x, y, 9, color);
            display.drawLine(x + 8, y, x + 15, y + 8, color);
            break;
        case ParametricGlyph::Resonance:
            display.drawHLine(x, y + 8, 5, color);
            display.drawLine(x + 4, y + 8, x + 8, y, color);
            display.drawLine(x + 8, y, x + 11, y + 8, color);
            display.drawHLine(x + 11, y + 8, 5, color);
            break;
        case ParametricGlyph::Attack:
            display.drawLine(x, y + 8, x + 15, y, color);
            break;
        case ParametricGlyph::Release:
            display.drawLine(x, y, x + 5, y + 5, color);
            display.drawLine(x + 5, y + 5, x + 15, y + 8, color);
            break;
        case ParametricGlyph::SineWave:
            display.drawLine(x, y + 4, x + 4, y, color);
            display.drawLine(x + 4, y, x + 8, y + 4, color);
            display.drawLine(x + 8, y + 4, x + 12, y + 8, color);
            display.drawLine(x + 12, y + 8, x + 15, y + 4, color);
            break;
        case ParametricGlyph::DelayTaps:
            display.drawVLine(x + 2, y, 9, color);
            display.drawVLine(x + 7, y + 3, 6, color);
            display.drawVLine(x + 12, y + 5, 4, color);
            break;
        case ParametricGlyph::ReverbCloud:
            display.drawPixel(x + 2, y + 4, color);
            display.drawPixel(x + 6, y + 2, color);
            display.drawPixel(x + 10, y + 5, color);
            display.drawPixel(x + 14, y + 3, color);
            display.drawPixel(x + 8, y + 7, color);
            break;
        case ParametricGlyph::DriveSaturation:
            display.drawLine(x, y + 8, x + 4, y + 6, color);
            display.drawLine(x + 4, y + 6, x + 11, y + 2, color);
            display.drawLine(x + 11, y + 2, x + 15, y, color);
            break;
        case ParametricGlyph::GenericBipolar:
            display.drawHLine(x, y + 4, 16, color);
            display.drawVLine(x + 8, y + 1, 7, color);
            break;
        case ParametricGlyph::GenericLevel:
        default:
            display.drawHLine(x, y + 4, 16, color);
            break;
    }
}

} // namespace smk
