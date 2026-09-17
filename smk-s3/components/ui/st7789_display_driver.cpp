#include "st7789_display_driver.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_attr.h"
#include <cstring>
#include <cstdlib>
#include <algorithm>

static const char* TAG = "ST7789_LCD";

namespace smk {

static inline uint16_t colorToNative(uint16_t color) {
    return static_cast<uint16_t>((color >> 8) | (color << 8));
}

ST7789DisplayDriver::ST7789DisplayDriver(const ST7789Config& config)
    : config_(config) {
}

ST7789DisplayDriver::~ST7789DisplayDriver() {
    if (backlight_initialized_) {
        setBrightness(0);
    }
    if (panel_handle_) {
        esp_lcd_panel_disp_on_off(panel_handle_, false);
        esp_lcd_panel_del(panel_handle_);
        panel_handle_ = nullptr;
    }
    if (io_handle_) {
        esp_lcd_panel_io_del(io_handle_);
        io_handle_ = nullptr;
    }
    if (spi_bus_initialized_) {
        spi_bus_free(config_.spi_host);
        spi_bus_initialized_ = false;
    }
    if (transfer_buffer_) {
        free(transfer_buffer_);
        transfer_buffer_ = nullptr;
    }
    if (framebuffer_) {
        free(framebuffer_);
        framebuffer_ = nullptr;
    }
}

bool ST7789DisplayDriver::begin() {
    ESP_LOGI(TAG, "Initializing ST7789 Panel via ESP-IDF esp_lcd (%dx%d)",
             config_.width, config_.height);

    if (config_.width <= 0 || config_.height <= 0) {
        ESP_LOGE(TAG, "Invalid panel geometry (%dx%d)", config_.width, config_.height);
        return false;
    }

    // Keep the full UI surface out of internal RAM. The audio engine and I2S
    // DMA retain internal memory; only a bounded row window below needs DMA.
    const size_t fb_size = static_cast<size_t>(config_.width) * config_.height * sizeof(uint16_t);
    framebuffer_ = static_cast<uint16_t*>(
        heap_caps_malloc(fb_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!framebuffer_) {
        ESP_LOGE(TAG, "Failed to allocate UI framebuffer in PSRAM (%zu bytes)", fb_size);
        return false;
    }

    const int16_t transfer_rows = std::min(config_.height, kTransferRows);
    transfer_buffer_pixels_ = static_cast<size_t>(config_.width) * transfer_rows;
    transfer_buffer_ = static_cast<uint16_t*>(heap_caps_malloc(
        transfer_buffer_pixels_ * sizeof(uint16_t),
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (!transfer_buffer_) {
        ESP_LOGE(TAG, "Failed to allocate LCD DMA window (%zu bytes)",
                 transfer_buffer_pixels_ * sizeof(uint16_t));
        return false;
    }

    transfer_done_sem_ = xSemaphoreCreateBinaryStatic(&transfer_done_storage_);
    if (!transfer_done_sem_) {
        ESP_LOGE(TAG, "Failed to create LCD transfer semaphore");
        return false;
    }

    // Configure the backlight off. It is enabled only after a valid black frame
    // has reached GRAM, avoiding random pixels during boot or failed init.
    if (config_.bl_pin >= 0) {
        ledc_timer_config_t ledc_timer = {};
        ledc_timer.speed_mode       = LEDC_LOW_SPEED_MODE;
        ledc_timer.duty_resolution  = LEDC_TIMER_8_BIT;
        ledc_timer.timer_num        = LEDC_TIMER_0;
        ledc_timer.freq_hz          = 5000;
        ledc_timer.clk_cfg          = LEDC_AUTO_CLK;
        if (ledc_timer_config(&ledc_timer) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure backlight timer");
            return false;
        }

        ledc_channel_config_t ledc_channel = {};
        ledc_channel.gpio_num       = config_.bl_pin;
        ledc_channel.speed_mode     = LEDC_LOW_SPEED_MODE;
        ledc_channel.channel        = LEDC_CHANNEL_0;
        ledc_channel.intr_type      = LEDC_INTR_DISABLE;
        ledc_channel.timer_sel      = LEDC_TIMER_0;
        ledc_channel.duty           = config_.bl_active_low ? 255 : 0;
        ledc_channel.hpoint         = 0;
        if (ledc_channel_config(&ledc_channel) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure backlight channel");
            return false;
        }
        backlight_initialized_ = true;
    }

    // SPI bus configuration
    spi_bus_config_t buscfg = {};
    buscfg.sclk_io_num = config_.sclk_pin;
    buscfg.mosi_io_num = config_.mosi_pin;
    buscfg.miso_io_num = -1;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = transfer_buffer_pixels_ * sizeof(uint16_t);

    esp_err_t error = spi_bus_initialize(config_.spi_host, &buscfg, SPI_DMA_CH_AUTO);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus");
        return false;
    }
    spi_bus_initialized_ = true;

    // ESP-IDF LCD Panel IO configuration
    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.dc_gpio_num = (gpio_num_t)config_.dc_pin;
    io_config.cs_gpio_num = (gpio_num_t)config_.cs_pin;
    io_config.pclk_hz = config_.clock_speed_hz;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    io_config.spi_mode = 0;
    io_config.trans_queue_depth = 1;
    io_config.on_color_trans_done = onColorTransferDone;
    io_config.user_ctx = this;

    error = esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)config_.spi_host, &io_config, &io_handle_);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create panel IO");
        return false;
    }

    // ST7789 Panel configuration
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = (gpio_num_t)config_.rst_pin;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
    panel_config.bits_per_pixel = 16;

    error = esp_lcd_new_panel_st7789(io_handle_, &panel_config, &panel_handle_);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ST7789 panel");
        return false;
    }

    if ((error = esp_lcd_panel_reset(panel_handle_)) != ESP_OK ||
        (error = esp_lcd_panel_init(panel_handle_)) != ESP_OK ||
        (error = esp_lcd_panel_invert_color(panel_handle_, config_.invert_color)) != ESP_OK ||
        (error = esp_lcd_panel_swap_xy(panel_handle_, config_.swap_xy)) != ESP_OK ||
        (error = esp_lcd_panel_mirror(panel_handle_, config_.mirror_x, config_.mirror_y)) != ESP_OK) {
        ESP_LOGE(TAG, "ST7789 initialization command failed: %s", esp_err_to_name(error));
        return false;
    }
    esp_lcd_panel_set_gap(panel_handle_, config_.x_offset, config_.y_offset);

    fillScreen(kColorBlack);
    if (!transferRegion(0, 0, config_.width, config_.height)) {
        ESP_LOGE(TAG, "Failed to clear ST7789 GRAM during initialization");
        return false;
    }
    clearDirty();

    if ((error = esp_lcd_panel_disp_on_off(panel_handle_, true)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable ST7789 panel: %s", esp_err_to_name(error));
        return false;
    }
    setBrightness(brightness_);
    ESP_LOGI(TAG,
             "ST7789 initialized (%dx%d, framebuffer=%zu bytes PSRAM, DMA window=%zu bytes internal)",
             config_.width, config_.height, fb_size,
             transfer_buffer_pixels_ * sizeof(uint16_t));
    return true;
}

void ST7789DisplayDriver::setBrightness(uint8_t value) {
    brightness_ = value;
    if (config_.bl_pin >= 0) {
        uint32_t duty = config_.bl_active_low ? (255 - value) : value;
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
}

void ST7789DisplayDriver::fillScreen(uint16_t color) {
    if (!framebuffer_) return;
    const uint16_t native_color = colorToNative(color);
    int32_t total = (int32_t)config_.width * config_.height;
    for (int32_t i = 0; i < total; ++i) {
        framebuffer_[i] = native_color;
    }
    invalidate();
}

void ST7789DisplayDriver::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (!framebuffer_ || x < 0 || x >= config_.width || y < 0 || y >= config_.height) return;
    framebuffer_[y * config_.width + x] = colorToNative(color);
    markDirty(x, y, 1, 1);
}

void ST7789DisplayDriver::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (!framebuffer_ || w <= 0 || h <= 0) return;
    int16_t x2 = std::min((int16_t)(x + w), config_.width);
    int16_t y2 = std::min((int16_t)(y + h), config_.height);
    int16_t x1 = std::max(x, (int16_t)0);
    int16_t y1 = std::max(y, (int16_t)0);
    if (x1 >= x2 || y1 >= y2) return;

    const uint16_t native_color = colorToNative(color);
    for (int16_t iy = y1; iy < y2; ++iy) {
        for (int16_t ix = x1; ix < x2; ++ix) {
            framebuffer_[iy * config_.width + ix] = native_color;
        }
    }
    markDirty(x1, y1, x2 - x1, y2 - y1);
}

void ST7789DisplayDriver::flush() {
    if (!is_dirty_) return;
    if (transferRegion(dirty_rect_.x, dirty_rect_.y, dirty_rect_.w, dirty_rect_.h)) {
        clearDirty();
    }
}

void ST7789DisplayDriver::flushRegion(int16_t x, int16_t y, int16_t w, int16_t h) {
    transferRegion(x, y, w, h);
}

bool ST7789DisplayDriver::transferRegion(int16_t x, int16_t y, int16_t w, int16_t h) {
    if (!panel_handle_ || !framebuffer_ || !transfer_buffer_ || !transfer_done_sem_ ||
        w <= 0 || h <= 0) {
        return false;
    }
    int16_t x1 = std::max((int16_t)0, x);
    int16_t y1 = std::max((int16_t)0, y);
    int16_t x2 = std::min((int16_t)config_.width, (int16_t)(x + w));
    int16_t y2 = std::min((int16_t)config_.height, (int16_t)(y + h));
    if (x1 >= x2 || y1 >= y2) return true;

    const int16_t region_width = x2 - x1;
    const size_t row_bytes = static_cast<size_t>(region_width) * sizeof(uint16_t);

    for (int16_t chunk_y = y1; chunk_y < y2;) {
        const int16_t rows = std::min<int16_t>(kTransferRows, y2 - chunk_y);
        const size_t chunk_pixels = static_cast<size_t>(region_width) * rows;
        if (chunk_pixels > transfer_buffer_pixels_) {
            reportTransferFailure("DMA window bounds", ESP_ERR_INVALID_SIZE);
            return false;
        }

        for (int16_t row = 0; row < rows; ++row) {
            const uint16_t* source = &framebuffer_[
                static_cast<size_t>(chunk_y + row) * config_.width + x1];
            std::memcpy(&transfer_buffer_[static_cast<size_t>(row) * region_width],
                        source, row_bytes);
        }

        while (xSemaphoreTake(transfer_done_sem_, 0) == pdTRUE) {
        }

        const esp_err_t error = esp_lcd_panel_draw_bitmap(
            panel_handle_, x1, chunk_y, x2, chunk_y + rows, transfer_buffer_);
        if (error != ESP_OK) {
            reportTransferFailure("draw bitmap", error);
            return false;
        }
        if (xSemaphoreTake(transfer_done_sem_, kTransferTimeoutTicks) != pdTRUE) {
            reportTransferFailure("DMA timeout", ESP_ERR_TIMEOUT);
            return false;
        }
        chunk_y += rows;
    }
    return true;
}

bool IRAM_ATTR ST7789DisplayDriver::onColorTransferDone(
    esp_lcd_panel_io_handle_t panel_io,
    esp_lcd_panel_io_event_data_t* event_data,
    void* user_ctx) {
    (void)panel_io;
    (void)event_data;
    auto* self = static_cast<ST7789DisplayDriver*>(user_ctx);
    if (!self || !self->transfer_done_sem_) return false;

    BaseType_t higher_priority_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(self->transfer_done_sem_, &higher_priority_task_woken);
    return higher_priority_task_woken == pdTRUE;
}

void ST7789DisplayDriver::reportTransferFailure(const char* operation, esp_err_t error) {
    ++transfer_failures_;
    if (transfer_failures_ == 1 || (transfer_failures_ & (transfer_failures_ - 1)) == 0) {
        ESP_LOGE(TAG, "%s failed: %s (count=%lu)", operation, esp_err_to_name(error),
                 static_cast<unsigned long>(transfer_failures_));
    }
}

void ST7789DisplayDriver::setOffsets(uint16_t x_offset, uint16_t y_offset) {
    config_.x_offset = x_offset;
    config_.y_offset = y_offset;
    if (panel_handle_) {
        esp_lcd_panel_set_gap(panel_handle_, x_offset, y_offset);
        invalidate();
        flush();
    }
}

void ST7789DisplayDriver::setOrientation(bool swap_xy, bool mirror_x, bool mirror_y) {
    config_.swap_xy = swap_xy;
    config_.mirror_x = mirror_x;
    config_.mirror_y = mirror_y;
    if (panel_handle_) {
        esp_lcd_panel_swap_xy(panel_handle_, swap_xy);
        esp_lcd_panel_mirror(panel_handle_, mirror_x, mirror_y);
        invalidate();
        flush();
    }
}

void ST7789DisplayDriver::setInvert(bool invert) {
    config_.invert_color = invert;
    if (panel_handle_) {
        esp_lcd_panel_invert_color(panel_handle_, invert);
        invalidate();
        flush();
    }
}

} // namespace smk
