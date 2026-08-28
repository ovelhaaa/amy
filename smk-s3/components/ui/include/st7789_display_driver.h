#pragma once

#include "display_driver.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include <cstdint>

namespace smk {

struct ST7789Config {
    int mosi_pin;
    int sclk_pin;
    int cs_pin;
    int dc_pin;
    int rst_pin;
    int bl_pin;
    int16_t width{284};
    int16_t height{76};
    uint16_t x_offset{0};
    uint16_t y_offset{0};
    spi_host_device_t spi_host{SPI2_HOST};
    int clock_speed_hz{40 * 1000 * 1000};
};

class ST7789DisplayDriver : public DisplayDriver {
public:
    explicit ST7789DisplayDriver(const ST7789Config& config);
    ~ST7789DisplayDriver() override;

    bool begin() override;
    void setBrightness(uint8_t value) override;
    void fillScreen(uint16_t color) override;
    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
    void flush() override;
    void flushRegion(int16_t x, int16_t y, int16_t w, int16_t h) override;

    int16_t width() const override { return config_.width; }
    int16_t height() const override { return config_.height; }

private:
    ST7789Config config_;
    esp_lcd_panel_io_handle_t io_handle_{nullptr};
    esp_lcd_panel_handle_t panel_handle_{nullptr};
    uint16_t* framebuffer_{nullptr};
    uint8_t brightness_{255};
};

} // namespace smk
