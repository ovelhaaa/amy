#include "pcm5102_output.h"
#include <esp_log.h>
#include <driver/gpio.h>
#include <cstring>
#include <cstdlib>
#include <freertos/FreeRTOS.h>

namespace smk {




static const char* TAG = "PCM5102";

PCM5102Output::PCM5102Output(int bclk_pin, int lrclk_pin, int data_pin, int mute_pin)
    : _bclk_pin(bclk_pin), _lrclk_pin(lrclk_pin), _data_pin(data_pin), _mute_pin(mute_pin),
      _tx_handle(nullptr), _underruns(0), _frames_written(0) {
}

PCM5102Output::~PCM5102Output() {
    stop();
    if (_tx_handle != nullptr) {
        i2s_del_channel(_tx_handle);
        _tx_handle = nullptr;
    }
}

bool PCM5102Output::begin() {
    ESP_LOGI(TAG, "Initializing I2S for PCM5102A (16-bit Stereo Zero-Copy)");
    
    if (_mute_pin >= 0) {
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << _mute_pin);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);
        gpio_set_level((gpio_num_t)_mute_pin, 0); // Muted during initialization
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 4;
    chan_cfg.dma_frame_num = 256;
    if (i2s_new_channel(&chan_cfg, &_tx_handle, nullptr) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel");
        return false;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(48000),
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)_bclk_pin,
            .ws = (gpio_num_t)_lrclk_pin,
            .dout = (gpio_num_t)_data_pin,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    if (i2s_channel_init_std_mode(_tx_handle, &std_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S std mode");
        return false;
    }
    
    return true;
}

bool PCM5102Output::start() {
    if (_tx_handle == nullptr) return false;
    
    if (i2s_channel_enable(_tx_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S channel");
        return false;
    }
    
    // Zero-fill buffer before starting to prevent pop
    size_t required_bytes = 256 * 2 * sizeof(int16_t);
    int16_t* zero_buf = (int16_t*)calloc(1, required_bytes);
    if (zero_buf) {
        size_t bytes_written = 0;
        i2s_channel_write(_tx_handle, zero_buf, required_bytes, &bytes_written, 100);
        free(zero_buf);
    }

    // Set Mute pin to HIGH (Unmute DAC)
    if (_mute_pin >= 0) {
        gpio_set_level((gpio_num_t)_mute_pin, 1);
        ESP_LOGI(TAG, "PCM5102 MUTE pin (GPIO %d) set to HIGH (Unmuted)", _mute_pin);
    }

    ESP_LOGI(TAG, "I2S Channel enabled");
    return true;
}

bool PCM5102Output::stop() {
    if (_mute_pin >= 0) {
        gpio_set_level((gpio_num_t)_mute_pin, 0); // Muted
    }
    if (_tx_handle == nullptr) return false;
    
    if (i2s_channel_disable(_tx_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disable I2S channel");
        return false;
    }
    return true;
}

bool PCM5102Output::write(const int16_t* interleaved_stereo, size_t frames) {
    if (_tx_handle == nullptr || interleaved_stereo == nullptr) return false;
    
    size_t required_bytes = frames * 2 * sizeof(int16_t);
    size_t bytes_written = 0;
    if (i2s_channel_write(_tx_handle, interleaved_stereo, required_bytes, &bytes_written, portMAX_DELAY) != ESP_OK) {
        return false;
    }
    
    if (bytes_written < required_bytes) {
        _underruns++;
    }
    _frames_written += bytes_written / (2 * sizeof(int16_t));
    
    return true;
}


} // namespace smk
