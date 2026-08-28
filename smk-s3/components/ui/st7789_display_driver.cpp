#include "st7789_display_driver.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include <cstring>
#include <cstdlib>
#include <algorithm>

static const char* TAG = "ST7789_LCD";

namespace smk {

ST7789DisplayDriver::ST7789DisplayDriver(const ST7789Config& config)
    : config_(config) {
}

ST7789DisplayDriver::~ST7789DisplayDriver() {
    if (panel_handle_) {
        esp_lcd_panel_del(panel_handle_);
    }
    if (io_handle_) {
        esp_lcd_panel_io_del(io_handle_);
    }
    if (framebuffer_) {
        free(framebuffer_);
    }
}

bool ST7789DisplayDriver::begin() {
    ESP_LOGI(TAG, "Initializing ST7789 Panel via ESP-IDF esp_lcd (284x76)");

    // Backlight LEDC PWM configuration
    if (config_.bl_pin >= 0) {
        ledc_timer_config_t ledc_timer = {};
        ledc_timer.speed_mode       = LEDC_LOW_SPEED_MODE;
        ledc_timer.duty_resolution  = LEDC_TIMER_8_BIT;
        ledc_timer.timer_num        = LEDC_TIMER_0;
        ledc_timer.freq_hz          = 5000;
        ledc_timer.clk_cfg          = LEDC_AUTO_CLK;
        ledc_timer_config(&ledc_timer);

        ledc_channel_config_t ledc_channel = {};
        ledc_channel.gpio_num       = config_.bl_pin;
        ledc_channel.speed_mode     = LEDC_LOW_SPEED_MODE;
        ledc_channel.channel        = LEDC_CHANNEL_0;
        ledc_channel.intr_type      = LEDC_INTR_DISABLE;
        ledc_channel.timer_sel      = LEDC_TIMER_0;
        ledc_channel.duty           = 0; // Start at 0 (minimum/dark)
        ledc_channel.hpoint         = 0;
        ledc_channel_config(&ledc_channel);
    }

    // SPI bus configuration
    spi_bus_config_t buscfg = {};
    buscfg.sclk_io_num = config_.sclk_pin;
    buscfg.mosi_io_num = config_.mosi_pin;
    buscfg.miso_io_num = -1;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = kWidth * kHeight * sizeof(uint16_t);

    if (spi_bus_initialize(config_.spi_host, &buscfg, SPI_DMA_CH_AUTO) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus");
        return false;
    }

    // ESP-IDF LCD Panel IO configuration
    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.dc_gpio_num = (gpio_num_t)config_.dc_pin;
    io_config.cs_gpio_num = (gpio_num_t)config_.cs_pin;
    io_config.pclk_hz = config_.clock_speed_hz;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    io_config.spi_mode = 0;
    io_config.trans_queue_depth = 10;

    if (esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)config_.spi_host, &io_config, &io_handle_) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create panel IO");
        return false;
    }

    // ST7789 Panel configuration
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = (gpio_num_t)config_.rst_pin;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
    panel_config.bits_per_pixel = 16;

    if (esp_lcd_new_panel_st7789(io_handle_, &panel_config, &panel_handle_) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ST7789 panel");
        return false;
    }

    esp_lcd_panel_reset(panel_handle_);
    esp_lcd_panel_init(panel_handle_);
    esp_lcd_panel_invert_color(panel_handle_, true);
    esp_lcd_panel_set_gap(panel_handle_, config_.x_offset, config_.y_offset);
    esp_lcd_panel_disp_on_off(panel_handle_, true);

    // Allocate Framebuffer in PSRAM or Internal RAM
    size_t fb_size = kWidth * kHeight * sizeof(uint16_t);
    framebuffer_ = (uint16_t*)heap_caps_malloc(fb_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!framebuffer_) {
        framebuffer_ = (uint16_t*)heap_caps_malloc(fb_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }

    if (!framebuffer_) {
        ESP_LOGE(TAG, "Failed to allocate UI Framebuffer (%zu bytes)", fb_size);
        return false;
    }

    fillScreen(kColorBlack);
    ESP_LOGI(TAG, "ST7789 esp_lcd panel initialized successfully");
    return true;
}

void ST7789DisplayDriver::setBrightness(uint8_t value) {
    brightness_ = value;
    if (config_.bl_pin >= 0) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, value);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
}

void ST7789DisplayDriver::fillScreen(uint16_t color) {
    if (!framebuffer_) return;
    for (int i = 0; i < kWidth * kHeight; ++i) {
        framebuffer_[i] = color;
    }
}

void ST7789DisplayDriver::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (!framebuffer_ || x < 0 || x >= kWidth || y < 0 || y >= kHeight) return;
    framebuffer_[y * kWidth + x] = color;
}

void ST7789DisplayDriver::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (!framebuffer_ || w <= 0 || h <= 0) return;
    int16_t x2 = std::min((int16_t)(x + w), kWidth);
    int16_t y2 = std::min((int16_t)(y + h), kHeight);
    int16_t x1 = std::max(x, (int16_t)0);
    int16_t y1 = std::max(y, (int16_t)0);

    for (int16_t iy = y1; iy < y2; ++iy) {
        for (int16_t ix = x1; ix < x2; ++ix) {
            framebuffer_[iy * kWidth + ix] = color;
        }
    }
}

void ST7789DisplayDriver::flush() {
    flushRegion(0, 0, kWidth, kHeight);
}

void ST7789DisplayDriver::flushRegion(int16_t x, int16_t y, int16_t w, int16_t h) {
    if (!panel_handle_ || !framebuffer_ || w <= 0 || h <= 0) return;
    esp_lcd_panel_draw_bitmap(panel_handle_, x, y, x + w, y + h, framebuffer_);
}

} // namespace smk
