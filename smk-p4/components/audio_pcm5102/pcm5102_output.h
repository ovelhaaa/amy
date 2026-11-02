#pragma once
#include "audio_config.h"
#include "driver/i2s_std.h"

namespace smk::p4::audio {
// Single task owns all lifecycle and write calls. No application state/AMY access.
class Pcm5102Output {
public:
    ~Pcm5102Output();
    Pcm5102Output() = default;
    Pcm5102Output(const Pcm5102Output&) = delete;
    Pcm5102Output& operator=(const Pcm5102Output&) = delete;
    esp_err_t begin(int bclk, int ws, int data);
    esp_err_t start();
    esp_err_t write(const int16_t* pcm, size_t frames);
    esp_err_t stop();
private:
    i2s_chan_handle_t channel_ = nullptr;
    bool started_ = false;
};
}
