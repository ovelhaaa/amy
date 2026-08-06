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
    // Draw label at top
    FontRenderer::drawString(display, x_, y_, label_, label_color_, DisplayDriver::kColorBlack, FontType::Font5x7);

    // Numeric value
    char val_str[8];
    snprintf(val_str, sizeof(val_str), "%02d", value_);
    FontRenderer::drawString(display, x_, y_ + 9, val_str, DisplayDriver::kColorYellow, DisplayDriver::kColorBlack, FontType::Font5x7);

    // Vertical gauge bar
    int16_t bar_y = y_ + 18;
    int16_t bar_h = h_ - 18;
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

} // namespace smk
