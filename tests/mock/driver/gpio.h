#pragma once
#include "i2s_std.h"
constexpr int GPIO_INTR_DISABLE = 0;
constexpr int GPIO_MODE_OUTPUT = 1;
constexpr int GPIO_PULLDOWN_DISABLE = 0;
constexpr int GPIO_PULLUP_DISABLE = 0;
struct gpio_config_t {
    int intr_type, mode;
    uint64_t pin_bit_mask;
    int pull_down_en, pull_up_en;
};
inline esp_err_t gpio_config(const gpio_config_t*) { return ESP_OK; }
inline esp_err_t gpio_set_level(gpio_num_t, int level) {
    mock_i2s::state.mute_level = level;
    return ESP_OK;
}
