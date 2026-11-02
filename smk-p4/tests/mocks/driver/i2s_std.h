#pragma once
#include <cstddef>
#include <cstdint>
using esp_err_t = int;
using gpio_num_t = int;
using i2s_chan_handle_t = void*;
constexpr int ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_INVALID_ARG = 1,
              ESP_ERR_INVALID_STATE = 2, ESP_ERR_TIMEOUT = 3;
constexpr int I2S_NUM_AUTO = -1, I2S_ROLE_MASTER = 1, I2S_GPIO_UNUSED = -1,
              I2S_DATA_BIT_WIDTH_16BIT = 16, I2S_SLOT_MODE_STEREO = 2;
struct i2s_chan_config_t {
    int id, role;
    unsigned dma_desc_num = 0, dma_frame_num = 0;
    bool auto_clear_after_cb = false;
};
struct i2s_std_config_t {
    struct Clock { uint32_t sample_rate_hz; } clk_cfg;
    struct Slot { int data_bit_width, slot_mode; } slot_cfg;
    struct Gpio { int mclk, bclk, ws, dout, din; } gpio_cfg;
};
#define I2S_CHANNEL_DEFAULT_CONFIG(id, role) {id, role}
#define I2S_STD_CLK_DEFAULT_CONFIG(rate) {rate}
#define I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(bits, mode) {bits, mode}
esp_err_t i2s_new_channel(const i2s_chan_config_t*, i2s_chan_handle_t*, void*);
esp_err_t i2s_del_channel(i2s_chan_handle_t);
esp_err_t i2s_channel_init_std_mode(i2s_chan_handle_t, const i2s_std_config_t*);
esp_err_t i2s_channel_preload_data(i2s_chan_handle_t, const void*, size_t, size_t*);
esp_err_t i2s_channel_enable(i2s_chan_handle_t);
esp_err_t i2s_channel_disable(i2s_chan_handle_t);
esp_err_t i2s_channel_write(i2s_chan_handle_t, const void*, size_t, size_t*, uint32_t);
