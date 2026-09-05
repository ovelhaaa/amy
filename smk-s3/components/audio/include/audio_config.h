#pragma once
#include <cstdint>

namespace smk::config {
constexpr uint32_t kSampleRateHz = 48000;
constexpr uint16_t kBlockSize = 256;
constexpr uint8_t kDmaBufferCount = 4;
constexpr uint16_t kDmaBufferFrames = 256;
constexpr uint32_t kAudioWriteTimeoutMs = 20;
constexpr uint32_t kAudioFadeInFrames = kSampleRateHz / 50; // 20 ms
} // namespace smk::config
