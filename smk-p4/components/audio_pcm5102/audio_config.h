#pragma once
#include <cstddef>
#include <cstdint>

namespace smk::p4::audio {
constexpr uint32_t kSampleRateHz = 48000;
constexpr size_t kBlockFrames = 256;
constexpr size_t kChannels = 2;
constexpr size_t kBlockSamples = kBlockFrames * kChannels;
constexpr unsigned kDmaDescriptors = 4;
constexpr uint32_t kWriteTimeoutMs = 20;
constexpr uint32_t kFadeFrames = 960; // 20 ms at each segment edge
constexpr uint32_t kSegmentFrames = 2 * kSampleRateHz;
constexpr float kTestAmplitude = 0.05f;
constexpr unsigned kTaskStackBytes = 4096;
constexpr unsigned kTaskPriority = 5; // Initial test only; affinity not forced.
}
