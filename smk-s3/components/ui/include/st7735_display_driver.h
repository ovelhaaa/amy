#pragma once

#include "display_driver.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include <cstdint>

#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

namespace smk {

struct ST7735Config {
    int mosi_pin;
    int sclk_pin;
    int cs_pin;
    int dc_pin;
    int rst_pin;
    int bl_pin;
    uint16_t x_offset{0};
    uint16_t y_offset{0};
    spi_host_device_t spi_host{SPI2_HOST};
    int clock_speed_hz{15 * 1000 * 1000};
};

class ST7735DisplayDriver : public DisplayDriver {
public:
    static constexpr int16_t kScreenWidth = 160;
    static constexpr int16_t kScreenHeight = 128;

    explicit ST7735DisplayDriver(const ST7735Config& config);
    ~ST7735DisplayDriver() override;

    bool begin() override;
    void setBrightness(uint8_t value) override;
    void fillScreen(uint16_t color) override;
    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override;
    void flush() override;
    void flushRegion(int16_t x, int16_t y, int16_t w, int16_t h) override;

    int16_t width() const override { return kScreenWidth; }
    int16_t height() const override { return kScreenHeight; }

private:
    void sendCmd(uint8_t cmd, const uint8_t* data = nullptr, size_t len = 0);
    void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    void initSt7735();

    ST7735Config config_;
    esp_lcd_panel_io_handle_t io_handle_{nullptr};
    uint16_t* framebuffer_{nullptr};
    uint8_t brightness_{255};
};

} // namespace smk
