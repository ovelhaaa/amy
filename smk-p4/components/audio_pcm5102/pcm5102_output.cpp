#include "pcm5102_output.h"
#include "driver/gpio.h"

namespace smk::p4::audio {
Pcm5102Output::~Pcm5102Output() {
    if (channel_) {
        stop();
        i2s_del_channel(channel_);
    }
}
esp_err_t Pcm5102Output::begin(int bclk, int ws, int data) {
    if (channel_) return ESP_ERR_INVALID_STATE;
    if (!GPIO_IS_VALID_OUTPUT_GPIO(bclk) || !GPIO_IS_VALID_OUTPUT_GPIO(ws) ||
        !GPIO_IS_VALID_OUTPUT_GPIO(data) || bclk == ws || bclk == data || ws == data)
        return ESP_ERR_INVALID_ARG;
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    channel.dma_desc_num = kDmaDescriptors;
    channel.dma_frame_num = kBlockFrames;
    channel.auto_clear_after_cb = true; // Unrefilled descriptors emit silence, never stale notes.
    esp_err_t error = i2s_new_channel(&channel, &channel_, nullptr);
    if (error != ESP_OK) return error;
    i2s_std_config_t standard{};
    standard.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(kSampleRateHz);
    standard.slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
    standard.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    standard.gpio_cfg.bclk = static_cast<gpio_num_t>(bclk);
    standard.gpio_cfg.ws = static_cast<gpio_num_t>(ws);
    standard.gpio_cfg.dout = static_cast<gpio_num_t>(data);
    standard.gpio_cfg.din = I2S_GPIO_UNUSED;
    error = i2s_channel_init_std_mode(channel_, &standard);
    if (error != ESP_OK) {
        i2s_del_channel(channel_);
        channel_ = nullptr;
    }
    return error;
}
esp_err_t Pcm5102Output::start() {
    if (!channel_ || started_) return ESP_ERR_INVALID_STATE;
    const int16_t silence[kBlockSamples]{};
    for (unsigned i = 0; i < kDmaDescriptors; ++i) {
        size_t loaded = 0;
        const esp_err_t error = i2s_channel_preload_data(channel_, silence, sizeof(silence), &loaded);
        if (error != ESP_OK) return error;
        if (loaded != sizeof(silence)) return ESP_FAIL;
    }
    const esp_err_t error = i2s_channel_enable(channel_);
    started_ = error == ESP_OK;
    return error;
}
esp_err_t Pcm5102Output::write(const int16_t* pcm, size_t frames) {
    if (!channel_ || !started_) return ESP_ERR_INVALID_STATE;
    if (!pcm || frames != kBlockFrames) return ESP_ERR_INVALID_ARG;
    size_t written = 0;
    const size_t bytes = kBlockSamples * sizeof(int16_t);
    // DMA sink is the sole bounded wait (20ms); no locks or other I/O here.
    const esp_err_t error = i2s_channel_write(channel_, pcm, bytes, &written, kWriteTimeoutMs);
    if (error != ESP_OK) return error;
    return written == bytes ? ESP_OK : ESP_FAIL;
}
esp_err_t Pcm5102Output::stop() {
    if (!channel_ || !started_) return ESP_OK;
    const esp_err_t error = i2s_channel_disable(channel_);
    if (error == ESP_OK) started_ = false;
    return error;
}
}
