#include "../components/audio_pcm5102/pcm5102_output.h"
#include <cassert>
using namespace smk::p4::audio;
namespace {
esp_err_t create_error = ESP_OK, init_error = ESP_OK, preload_error = ESP_OK,
          enable_error = ESP_OK, write_error = ESP_OK, disable_error = ESP_OK;
bool partial = false;
unsigned preloads = 0, deletes = 0, writes = 0;
}
esp_err_t i2s_new_channel(const i2s_chan_config_t* config, i2s_chan_handle_t* handle, void*) {
    assert(config->role == I2S_ROLE_MASTER);
    assert(config->dma_desc_num == 4 && config->dma_frame_num == 256);
    assert(config->auto_clear_after_cb);
    if (create_error == ESP_OK) *handle = reinterpret_cast<void*>(1);
    return create_error;
}
esp_err_t i2s_del_channel(i2s_chan_handle_t) { ++deletes; return ESP_OK; }
esp_err_t i2s_channel_init_std_mode(i2s_chan_handle_t, const i2s_std_config_t* config) {
    assert(config->clk_cfg.sample_rate_hz == 48000);
    assert(config->slot_cfg.data_bit_width == 16 && config->slot_cfg.slot_mode == 2);
    assert(config->gpio_cfg.bclk == 50 && config->gpio_cfg.ws == 51 && config->gpio_cfg.dout == 52);
    assert(config->gpio_cfg.mclk == I2S_GPIO_UNUSED && config->gpio_cfg.din == I2S_GPIO_UNUSED);
    return init_error;
}
esp_err_t i2s_channel_preload_data(i2s_chan_handle_t, const void* source, size_t bytes, size_t* loaded) {
    assert(bytes == kBlockSamples*sizeof(int16_t));
    for (size_t i = 0; i < bytes; ++i) assert(static_cast<const uint8_t*>(source)[i] == 0);
    ++preloads;
    *loaded = bytes;
    return preload_error;
}
esp_err_t i2s_channel_enable(i2s_chan_handle_t) { return enable_error; }
esp_err_t i2s_channel_disable(i2s_chan_handle_t) { return disable_error; }
esp_err_t i2s_channel_write(i2s_chan_handle_t, const void*, size_t bytes, size_t* written, uint32_t timeout) {
    assert(timeout == kWriteTimeoutMs && bytes == kBlockSamples*sizeof(int16_t));
    ++writes;
    *written = partial ? bytes-2 : bytes;
    return write_error;
}
int main() {
    int16_t pcm[kBlockSamples]{};
    {
        Pcm5102Output output;
        assert(output.start() == ESP_ERR_INVALID_STATE);
        assert(output.write(pcm, kBlockFrames) == ESP_ERR_INVALID_STATE);
        assert(output.begin(-1, 51, 52) == ESP_ERR_INVALID_ARG);
        assert(output.begin(50, 50, 52) == ESP_ERR_INVALID_ARG);
        create_error = ESP_FAIL;
        assert(output.begin(50, 51, 52) == ESP_FAIL);
        create_error = ESP_OK;
        init_error = ESP_FAIL;
        assert(output.begin(50, 51, 52) == ESP_FAIL && deletes == 1);
        init_error = ESP_OK;
        assert(output.begin(50, 51, 52) == ESP_OK);
        assert(output.begin(50, 51, 52) == ESP_ERR_INVALID_STATE);
        preload_error = ESP_FAIL;
        assert(output.start() == ESP_FAIL);
        preload_error = ESP_OK;
        enable_error = ESP_FAIL;
        assert(output.start() == ESP_FAIL);
        enable_error = ESP_OK;
        const auto before = preloads;
        assert(output.start() == ESP_OK && preloads-before == 4);
        assert(output.start() == ESP_ERR_INVALID_STATE);
        assert(output.write(nullptr, kBlockFrames) == ESP_ERR_INVALID_ARG);
        assert(output.write(pcm, kBlockFrames-1) == ESP_ERR_INVALID_ARG && writes == 0);
        assert(output.write(pcm, kBlockFrames) == ESP_OK);
        write_error = ESP_ERR_TIMEOUT;
        assert(output.write(pcm, kBlockFrames) == ESP_ERR_TIMEOUT);
        write_error = ESP_OK;
        partial = true;
        assert(output.write(pcm, kBlockFrames) == ESP_FAIL);
        partial = false;
        disable_error = ESP_FAIL;
        assert(output.stop() == ESP_FAIL);
        disable_error = ESP_OK;
        assert(output.stop() == ESP_OK);
        assert(output.write(pcm, kBlockFrames) == ESP_ERR_INVALID_STATE);
        assert(output.start() == ESP_OK);
        assert(output.stop() == ESP_OK && output.stop() == ESP_OK);
    }
    assert(deletes == 2);
}
