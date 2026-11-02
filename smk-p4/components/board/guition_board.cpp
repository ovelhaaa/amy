#include "guition_board.h"

namespace guition {
static i2c_master_bus_handle_t bus = nullptr;
esp_err_t i2c_begin() {
    if (bus) return ESP_OK;
    i2c_master_bus_config_t config{};
    config.i2c_port = I2C_NUM_0;
    config.sda_io_num = SDA;
    config.scl_io_num = SCL;
    config.clk_source = I2C_CLK_SRC_DEFAULT;
    config.glitch_ignore_cnt = 7;
    config.flags.enable_internal_pullup = true;
    return i2c_new_master_bus(&config, &bus);
}
i2c_master_bus_handle_t i2c_bus() { return bus; }
}
