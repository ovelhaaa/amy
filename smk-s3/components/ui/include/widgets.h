#pragma once

#include "display_driver.h"
#include "font_renderer.h"
#include <cstdint>

namespace smk {

class Widget {
public:
    virtual ~Widget() = default;
    virtual void draw(DisplayDriver& display) = 0;
};

class Label : public Widget {
public:
    Label(int16_t x = 0, int16_t y = 0, const char* text = "", 
          uint16_t fg_color = DisplayDriver::kColorWhite, 
          uint16_t bg_color = DisplayDriver::kColorBlack,
          FontType font = FontType::Font5x7, uint8_t scale = 1);

    void setText(const char* text);
    void setPosition(int16_t x, int16_t y);
    void setColor(uint16_t fg, uint16_t bg = DisplayDriver::kColorBlack);
    void draw(DisplayDriver& display) override;

private:
    int16_t x_;
    int16_t y_;
    char text_[48];
    uint16_t fg_color_;
    uint16_t bg_color_;
    FontType font_;
    uint8_t scale_;
};

class ProgressBar : public Widget {
public:
    ProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, 
                uint16_t fill_color = DisplayDriver::kColorGreen,
                uint16_t border_color = DisplayDriver::kColorMidGray);

    void setValue(float norm_val); // 0.0f to 1.0f
    void setColors(uint16_t fill, uint16_t border);
    void draw(DisplayDriver& display) override;

private:
    int16_t x_, y_, w_, h_;
    float norm_val_{0.0f};
    uint16_t fill_color_;
    uint16_t border_color_;
};

class BarGauge : public Widget {
public:
    BarGauge(int16_t x, int16_t y, int16_t w, int16_t h, const char* label);

    void setValue(uint8_t value); // 0 to 127
    void setLabel(const char* label);
    void setColors(uint16_t bar_color, uint16_t label_color);
    void draw(DisplayDriver& display) override;

private:
    int16_t x_, y_, w_, h_;
    char label_[8];
    uint8_t value_{0};
    uint16_t bar_color_{DisplayDriver::kColorCyan};
    uint16_t label_color_{DisplayDriver::kColorWhite};
};

class BorderBox : public Widget {
public:
    BorderBox(int16_t x, int16_t y, int16_t w, int16_t h, const char* title = nullptr,
              uint16_t border_color = DisplayDriver::kColorMidGray);

    void setTitle(const char* title);
    void draw(DisplayDriver& display) override;

private:
    int16_t x_, y_, w_, h_;
    char title_[32];
    uint16_t border_color_;
};

class StatusIndicator : public Widget {
public:
    StatusIndicator(int16_t x, int16_t y, const char* label);

    void setActive(bool active);
    void setColors(uint16_t active_color, uint16_t inactive_color);
    void draw(DisplayDriver& display) override;

private:
    int16_t x_, y_;
    char label_[12];
    bool active_{false};
    uint16_t active_color_{DisplayDriver::kColorGreen};
    uint16_t inactive_color_{DisplayDriver::kColorDarkGray};
};

} // namespace smk
