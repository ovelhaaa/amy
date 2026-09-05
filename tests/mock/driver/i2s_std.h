#pragma once
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <vector>

using esp_err_t = int;
using gpio_num_t = int;
using i2s_chan_handle_t = void*;
constexpr int ESP_OK = 0;
constexpr int ESP_FAIL = -1;
constexpr int I2S_NUM_AUTO = -1;
constexpr int I2S_GPIO_UNUSED = -1;
constexpr int I2S_ROLE_MASTER = 1;
constexpr int I2S_DATA_BIT_WIDTH_16BIT = 16;
constexpr int I2S_SLOT_MODE_STEREO = 2;
struct i2s_chan_config_t { int dma_desc_num; int dma_frame_num; };
struct MockClockConfig { uint32_t sample_rate_hz; };
struct MockSlotConfig { int bits; int channels; };
struct MockGpioConfig {
    int mclk, bclk, ws, dout, din;
    struct { bool mclk_inv, bclk_inv, ws_inv; } invert_flags;
};
struct i2s_std_config_t { MockClockConfig clk_cfg; MockSlotConfig slot_cfg; MockGpioConfig gpio_cfg; };
#define I2S_CHANNEL_DEFAULT_CONFIG(number, role) i2s_chan_config_t{}
#define I2S_STD_CLK_DEFAULT_CONFIG(rate) MockClockConfig{rate}
#define I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(bits, channels) MockSlotConfig{bits, channels}

namespace mock_i2s {
struct State {
    bool created = false, enabled = false;
    bool fail_create = false, fail_init = false, fail_enable = false;
    bool short_preload = false, short_write = false, fail_write = false;
    size_t preload_bytes = 0;
    int deletes = 0, disables = 0, writes = 0, preloads = 0;
    int mute_level = 0;
    uint32_t last_timeout_ms = 0;
    i2s_chan_config_t channel{};
    i2s_std_config_t standard{};
};
inline State state;
}
inline esp_err_t i2s_new_channel(const i2s_chan_config_t* config, i2s_chan_handle_t* tx, void*) {
    if (mock_i2s::state.fail_create) return ESP_FAIL;
    mock_i2s::state.created = true;
    mock_i2s::state.channel = *config;
    *tx = reinterpret_cast<void*>(1);
    return ESP_OK;
}
inline esp_err_t i2s_del_channel(i2s_chan_handle_t) { ++mock_i2s::state.deletes; return ESP_OK; }
inline esp_err_t i2s_channel_init_std_mode(i2s_chan_handle_t, const i2s_std_config_t* config) {
    mock_i2s::state.standard = *config;
    return mock_i2s::state.fail_init ? ESP_FAIL : ESP_OK;
}
inline esp_err_t i2s_channel_preload_data(i2s_chan_handle_t, const void* data, size_t size, size_t* loaded) {
    auto& state = mock_i2s::state;
    assert(!state.enabled && state.mute_level == 0);
    for (size_t i = 0; i < size; ++i) assert(static_cast<const uint8_t*>(data)[i] == 0);
    *loaded = state.short_preload ? size / 2 : size;
    state.preload_bytes += *loaded;
    ++state.preloads;
    return ESP_OK;
}
inline esp_err_t i2s_channel_enable(i2s_chan_handle_t) {
    auto& state = mock_i2s::state;
    assert(state.preload_bytes == static_cast<size_t>(state.channel.dma_desc_num * state.channel.dma_frame_num * 4));
    if (state.fail_enable) return ESP_FAIL;
    state.enabled = true;
    return ESP_OK;
}
inline esp_err_t i2s_channel_disable(i2s_chan_handle_t) {
    mock_i2s::state.enabled = false;
    ++mock_i2s::state.disables;
    return ESP_OK;
}
inline esp_err_t i2s_channel_write(i2s_chan_handle_t, const void*, size_t size, size_t* written, uint32_t timeout_ms) {
    auto& state = mock_i2s::state;
    assert(state.enabled);
    ++state.writes;
    state.last_timeout_ms = timeout_ms;
    *written = state.short_write ? size / 2 : size;
    return state.fail_write ? ESP_FAIL : ESP_OK;
}
