#pragma once
#include "audio_config.h"

namespace smk::p4::audio {
// Exact one-block contract. Invalid input clears the available destination.
bool interleave(const float* left, const float* right, int16_t* output,
                size_t frames, size_t output_samples);
class TestTone {
public:
    bool configure(float frequency_hz, float amplitude);
    bool render(int16_t* output, size_t frames, bool sequence);
private:
    float frequency_hz_ = 440.0f, amplitude_ = kTestAmplitude;
    double phase_ = 0.0;
    uint32_t frame_ = 0;
};
}
