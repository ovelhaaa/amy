#include "st7735_display_driver.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <cstdlib>
#include <algorithm>

static const char* TAG = "ST7735_LCD";

namespace smk {

static inline uint16_t colorToNative(uint16_t color) {
    return (color >> 8) | (color << 8);
}

ST7735DisplayDriver::ST7735DisplayDriver(const ST7735Config& config)
    : config_(config) {
}

ST7735DisplayDriver::~ST7735DisplayDriver() {
    if (io_handle_) {
        esp_lcd_panel_io_del(io_handle_);
        io_handle_ = nullptr;
    }
    if (framebuffer_) {
        free(framebuffer_);
        framebuffer_ = nullptr;
    }
}

void ST7735DisplayDriver::sendCmd(uint8_t cmd, const uint8_t* data, size_t len) {
    if (!io_handle_) return;
    esp_lcd_panel_io_tx_param(io_handle_, cmd, data, len);
}

void ST7735DisplayDriver::setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint16_t xa = x0 + config_.x_offset;
    uint16_t xb = x1 + config_.x_offset;
    uint16_t ya = y0 + config_.y_offset;
    uint16_t yb = y1 + config_.y_offset;

    uint8_t col_data[4] = {
        static_cast<uint8_t>(xa >> 8),
        static_cast<uint8_t>(xa & 0xFF),
        static_cast<uint8_t>(xb >> 8),
        static_cast<uint8_t>(xb & 0xFF)
    };
    sendCmd(0x2A, col_data, sizeof(col_data)); // CASET

    uint8_t row_data[4] = {
        static_cast<uint8_t>(ya >> 8),
        static_cast<uint8_t>(ya & 0xFF),
        static_cast<uint8_t>(yb >> 8),
        static_cast<uint8_t>(yb & 0xFF)
    };
    sendCmd(0x2B, row_data, sizeof(row_data)); // RASET
}

void ST7735DisplayDriver::initSt7735() {
    ESP_LOGI(TAG, "Sending ST7735S native initialization sequence (160x128 Landscape)");

    // Software Reset
    sendCmd(0x01); // SWRESET
    vTaskDelay(pdMS_TO_TICKS(150));

    // Sleep Out
    sendCmd(0x11); // SLPOUT
    vTaskDelay(pdMS_TO_TICKS(150));

    // Frame Rate Control 1 (Normal mode / Full colors)
    static const uint8_t frmctr1_data[] = {0x01, 0x2C, 0x2D};
    sendCmd(0xB1, frmctr1_data, sizeof(frmctr1_data));

    // Frame Rate Control 2 (Idle mode / 8-colors)
    static const uint8_t frmctr2_data[] = {0x01, 0x2C, 0x2D};
    sendCmd(0xB2, frmctr2_data, sizeof(frmctr2_data));

    // Frame Rate Control 3 (Partial mode / Full colors)
    static const uint8_t frmctr3_data[] = {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D};
    sendCmd(0xB3, frmctr3_data, sizeof(frmctr3_data));

    // Display Inversion Control (Line inversion)
    static const uint8_t invctr_data[] = {0x07};
    sendCmd(0xB4, invctr_data, sizeof(invctr_data));

    // Power Control 1
    static const uint8_t pwctr1_data[] = {0xA2, 0x02, 0x84};
    sendCmd(0xC0, pwctr1_data, sizeof(pwctr1_data));

    // Power Control 2
    static const uint8_t pwctr2_data[] = {0xC5};
    sendCmd(0xC1, pwctr2_data, sizeof(pwctr2_data));

    // Power Control 3 (Normal mode)
    static const uint8_t pwctr3_data[] = {0x0A, 0x00};
    sendCmd(0xC2, pwctr3_data, sizeof(pwctr3_data));

    // Power Control 4 (Idle mode)
    static const uint8_t pwctr4_data[] = {0x8A, 0x2A};
    sendCmd(0xC3, pwctr4_data, sizeof(pwctr4_data));

    // Power Control 5 (Partial mode)
    static const uint8_t pwctr5_data[] = {0x8A, 0xEE};
    sendCmd(0xC4, pwctr5_data, sizeof(pwctr5_data));

    // VCOM Control 1
    static const uint8_t vmctr1_data[] = {0x0E};
    sendCmd(0xC5, vmctr1_data, sizeof(vmctr1_data));

    // Display Inversion Off
    sendCmd(0x20); // INVOFF

    // Memory Access Control: 160x128 Landscape (MY=1, MX=0, MV=1, BGR=1 -> 0xA8)
    static const uint8_t madctl_data[] = {0xA8};
    sendCmd(0x36, madctl_data, sizeof(madctl_data));

    // Interface Pixel Format: 16-bit RGB565
    static const uint8_t colmod_data[] = {0x05};
    sendCmd(0x3A, colmod_data, sizeof(colmod_data));

    // Gamma '+' Polarity Setting
    static const uint8_t gmctrp1_data[] = {
        0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D,
        0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10
    };
    sendCmd(0xE0, gmctrp1_data, sizeof(gmctrp1_data));

    // Gamma '-' Polarity Setting
    static const uint8_t gmctrn1_data[] = {
        0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
        0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10
    };
    sendCmd(0xE1, gmctrn1_data, sizeof(gmctrn1_data));

    // Normal Display Mode On
    sendCmd(0x13); // NORON
    vTaskDelay(pdMS_TO_TICKS(10));

    // Display On
    sendCmd(0x29); // DISPON
    vTaskDelay(pdMS_TO_TICKS(100));
}

bool ST7735DisplayDriver::begin() {
    ESP_LOGI(TAG, "Initializing ST7735 Panel via ESP-IDF esp_lcd (160x128 Landscape)");

    // 1. Backlight LEDC PWM Config
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

    // 2. Hardware Reset GPIO
    if (config_.rst_pin >= 0) {
        gpio_config_t rst_gpio_config = {};
        rst_gpio_config.mode = GPIO_MODE_OUTPUT;
        rst_gpio_config.pin_bit_mask = 1ULL << config_.rst_pin;
        gpio_config(&rst_gpio_config);

        gpio_set_level((gpio_num_t)config_.rst_pin, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level((gpio_num_t)config_.rst_pin, 0);
        vTaskDelay(pdMS_TO_TICKS(20));
        gpio_set_level((gpio_num_t)config_.rst_pin, 1);
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    // 3. SPI Bus Config
    spi_bus_config_t buscfg = {};
    buscfg.sclk_io_num = config_.sclk_pin;
    buscfg.mosi_io_num = config_.mosi_pin;
    buscfg.miso_io_num = -1;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = kScreenWidth * kScreenHeight * sizeof(uint16_t);

    esp_err_t ret = spi_bus_initialize(config_.spi_host, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus (0x%x)", ret);
        return false;
    }

    // 4. Panel IO Config
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

    // 5. Send ST7735 native initialization sequence
    initSt7735();

    // 6. Allocate Framebuffer (40 KB)
    size_t fb_size = kScreenWidth * kScreenHeight * sizeof(uint16_t);
    framebuffer_ = (uint16_t*)heap_caps_malloc(fb_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!framebuffer_) {
        framebuffer_ = (uint16_t*)heap_caps_malloc(fb_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }

    if (!framebuffer_) {
        ESP_LOGE(TAG, "Failed to allocate 160x128 framebuffer");
        return false;
    }

    fillScreen(kColorBlack);
    flush();
    ESP_LOGI(TAG, "ST7735 panel initialized successfully");
    return true;
}

void ST7735DisplayDriver::setBrightness(uint8_t value) {
    brightness_ = value;
    if (config_.bl_pin >= 0) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, value);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
}

void ST7735DisplayDriver::fillScreen(uint16_t color) {
    if (!framebuffer_) return;
    uint16_t c = colorToNative(color);
    size_t count = kScreenWidth * kScreenHeight;
    for (size_t i = 0; i < count; ++i) {
        framebuffer_[i] = c;
    }
}

void ST7735DisplayDriver::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (!framebuffer_ || x < 0 || x >= kScreenWidth || y < 0 || y >= kScreenHeight) return;
    framebuffer_[y * kScreenWidth + x] = colorToNative(color);
}

void ST7735DisplayDriver::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (!framebuffer_ || w <= 0 || h <= 0) return;
    int16_t x2 = std::min((int16_t)(x + w), (int16_t)kScreenWidth);
    int16_t y2 = std::min((int16_t)(y + h), (int16_t)kScreenHeight);
    int16_t x1 = std::max(x, (int16_t)0);
    int16_t y1 = std::max(y, (int16_t)0);
    uint16_t c = colorToNative(color);

    for (int16_t iy = y1; iy < y2; ++iy) {
        for (int16_t ix = x1; ix < x2; ++ix) {
            framebuffer_[iy * kScreenWidth + ix] = c;
        }
    }
}

void ST7735DisplayDriver::flush() {
    if (!io_handle_ || !framebuffer_) return;
    setAddrWindow(0, 0, kScreenWidth - 1, kScreenHeight - 1);
    esp_lcd_panel_io_tx_color(io_handle_, 0x2C, framebuffer_, kScreenWidth * kScreenHeight * sizeof(uint16_t));
}

void ST7735DisplayDriver::flushRegion(int16_t x, int16_t y, int16_t w, int16_t h) {
    if (!io_handle_ || !framebuffer_ || w <= 0 || h <= 0) return;
    if (x == 0 && y == 0 && w == kScreenWidth && h == kScreenHeight) {
        flush();
        return;
    }
    int16_t x2 = std::min((int16_t)(x + w - 1), (int16_t)(kScreenWidth - 1));
    int16_t y2 = std::min((int16_t)(y + h - 1), (int16_t)(kScreenHeight - 1));
    int16_t x1 = std::max(x, (int16_t)0);
    int16_t y1 = std::max(y, (int16_t)0);
    if (x1 > x2 || y1 > y2) return;

    setAddrWindow(x1, y1, x2, y2);
    if (x1 == 0 && x2 == kScreenWidth - 1) {
        size_t offset = y1 * kScreenWidth;
        size_t count = (y2 - y1 + 1) * kScreenWidth * sizeof(uint16_t);
        esp_lcd_panel_io_tx_color(io_handle_, 0x2C, &framebuffer_[offset], count);
    } else {
        for (int16_t iy = y1; iy <= y2; ++iy) {
            size_t offset = iy * kScreenWidth + x1;
            size_t count = (x2 - x1 + 1) * sizeof(uint16_t);
            esp_lcd_panel_io_tx_color(io_handle_, 0x2C, &framebuffer_[offset], count);
        }
    }
}

} // namespace smk
