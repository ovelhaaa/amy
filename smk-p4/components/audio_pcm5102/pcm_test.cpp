#include "pcm_test.h"
#include <algorithm>
#include <cmath>

namespace smk::p4::audio {
namespace {
int16_t sample(float value) {
    value = std::max(-1.0f, std::min(1.0f, value));
    return static_cast<int16_t>(std::lround(value * (value < 0 ? 32768.0f : 32767.0f)));
}
}
bool interleave(const float* left, const float* right, int16_t* output,
                size_t frames, size_t output_samples) {
    if (!output) return false;
    // Do not trust external lengths to create an unbounded clear.
    const size_t available = std::min(output_samples, kBlockSamples);
    std::fill_n(output, available, 0);
    if (!left || frames != kBlockFrames || output_samples != kBlockSamples) return false;
    if (!right) right = left; // Explicit mono duplication.
    for (size_t i = 0; i < kBlockFrames; ++i) {
        if (!std::isfinite(left[i]) || !std::isfinite(right[i])) return false;
    }
    for (size_t i = 0; i < kBlockFrames; ++i) {
        output[2*i] = sample(left[i]);
        output[2*i+1] = sample(right[i]);
    }
    return true;
}
bool TestTone::configure(float frequency_hz, float amplitude) {
    if (!std::isfinite(frequency_hz) || frequency_hz < 20 || frequency_hz > 20000 ||
        !std::isfinite(amplitude) || amplitude < 0 || amplitude > 0.1f) return false;
    frequency_hz_ = frequency_hz;
    amplitude_ = amplitude;
    phase_ = 0;
    frame_ = 0;
    return true;
}
bool TestTone::render(int16_t* output, size_t frames, bool sequence) {
    if (!output || frames != kBlockFrames) return false;
    constexpr double tau = 6.2831853071795864769;
    for (size_t i = 0; i < frames; ++i) {
        const unsigned segment = frame_ / kSegmentFrames;
        const uint32_t position = frame_ % kSegmentFrames;
        const float gain = static_cast<float>(std::min({position, kSegmentFrames-1-position, kFadeFrames})) / kFadeFrames;
        const int16_t value = sample(static_cast<float>(std::sin(phase_)) * amplitude_ * gain);
        output[2*i] = sequence && (segment == 1 || segment == 3) ? value : 0;
        output[2*i+1] = sequence && (segment == 2 || segment == 3) ? value : 0;
        phase_ += tau * frequency_hz_ / kSampleRateHz;
        if (phase_ >= tau) phase_ -= tau;
        if (++frame_ == 5 * kSegmentFrames) frame_ = 0;
    }
    return true;
}
}
