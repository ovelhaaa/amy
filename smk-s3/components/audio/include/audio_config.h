#pragma once
#include <cstdint>
#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif

namespace smk::config {
constexpr uint32_t kSampleRateHz = 48000;
#ifdef CONFIG_SMKS3_AMY_BLOCK_SIZE_128
constexpr uint16_t kBlockSize = 128;
#else
constexpr uint16_t kBlockSize = 256;
#endif
constexpr uint8_t kDmaBufferCount = 4;
constexpr uint16_t kDmaBufferFrames = kBlockSize;
constexpr uint32_t kAudioWriteTimeoutMs = 20;
constexpr uint32_t kAudioFadeInFrames = kSampleRateHz / 50; // 20 ms
} // namespace smk::config
