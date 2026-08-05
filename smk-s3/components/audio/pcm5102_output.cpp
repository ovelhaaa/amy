#include "pcm5102_output.h"
#include <esp_log.h>
#include <cstring>
#include <cstdlib>
#include <freertos/FreeRTOS.h>

namespace smk {




static const char* TAG = "PCM5102";

PCM5102Output::PCM5102Output(int bclk_pin, int lrclk_pin, int data_pin)
    : _bclk_pin(bclk_pin), _lrclk_pin(lrclk_pin), _data_pin(data_pin),
      _tx_handle(nullptr), _underruns(0), _frames_written(0),
      _conversion_buffer(nullptr), _conversion_buffer_size(0) {
}

PCM5102Output::~PCM5102Output() {
    stop();
    if (_tx_handle != nullptr) {
        i2s_del_channel(_tx_handle);
        _tx_handle = nullptr;
    }
    if (_conversion_buffer != nullptr) {
        free(_conversion_buffer);
        _conversion_buffer = nullptr;
    }
}

bool PCM5102Output::begin() {
    ESP_LOGI(TAG, "Initializing I2S for PCM5102A");
    
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    if (i2s_new_channel(&chan_cfg, &_tx_handle, nullptr) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel");
        return false;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(48000),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
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
    size_t required_bytes = 256 * 2 * sizeof(int32_t); // Example buffer size
    int32_t* zero_buf = (int32_t*)calloc(1, required_bytes);
    if (zero_buf) {
        size_t bytes_written = 0;
        i2s_channel_write(_tx_handle, zero_buf, required_bytes, &bytes_written, 100);
        free(zero_buf);
    }

    ESP_LOGI(TAG, "I2S Channel enabled");
    return true;
}

bool PCM5102Output::stop() {
    if (_tx_handle == nullptr) return false;
    
    if (i2s_channel_disable(_tx_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disable I2S channel");
        return false;
    }
    return true;
}

bool PCM5102Output::write(const int16_t* interleaved_stereo, size_t frames) {
    if (_tx_handle == nullptr) return false;
    
    size_t required_bytes = frames * 2 * sizeof(int32_t);
    if (_conversion_buffer_size < required_bytes) {
        if (_conversion_buffer != nullptr) free(_conversion_buffer);
        _conversion_buffer = (int32_t*)malloc(required_bytes);
        if (!_conversion_buffer) return false;
        _conversion_buffer_size = required_bytes;
    }
    
    for (size_t i = 0; i < frames * 2; ++i) {
        _conversion_buffer[i] = ((int32_t)interleaved_stereo[i]) << 16;
    }
    
    size_t bytes_written = 0;
    if (i2s_channel_write(_tx_handle, _conversion_buffer, required_bytes, &bytes_written, portMAX_DELAY) != ESP_OK) {
        return false;
    }
    
    if (bytes_written < required_bytes) {
        _underruns++;
    }
    _frames_written += bytes_written / (2 * sizeof(int32_t));
    
    return true;
}


} // namespace smk
