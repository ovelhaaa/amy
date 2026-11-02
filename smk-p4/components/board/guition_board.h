#pragma once
#include "driver/i2c_master.h"

// Profile recovered from the user's working P4SDM project, same physical unit.
namespace guition {
constexpr gpio_num_t SDA = GPIO_NUM_7;
constexpr gpio_num_t SCL = GPIO_NUM_8;
constexpr gpio_num_t LCD_RESET = GPIO_NUM_5;
constexpr gpio_num_t LCD_BACKLIGHT = GPIO_NUM_23;
constexpr gpio_num_t TOUCH_RESET = GPIO_NUM_3;
esp_err_t i2c_begin();
i2c_master_bus_handle_t i2c_bus();
}
