#pragma once
#include <cstdint>

namespace smk::config {

// Shared by the application and adapter; no board or RTOS dependencies.
constexpr uint16_t kMaxOscillators = 120;
constexpr uint8_t  kDefaultPatchNumber = 0;
constexpr uint8_t  kDefaultVoiceCount = 8;
constexpr uint16_t kDrumPatchNumber = 258;
constexpr uint8_t kDrumOscillatorBudget = 32; // Patch 258; checked with real AMY.
constexpr uint8_t kPerformanceMaxVoicesPerLayer = 32; // Upstream per-instrument limit.
constexpr uint16_t kSynthCommandCapacity = 64;
constexpr uint16_t kSynthReleaseReserve = 8;
constexpr uint16_t kSynthCommandsPerBlock = 16;
constexpr uint16_t kSynthMessageBytes = 256;
constexpr uint8_t kSynthPcmBlocks = 2;
constexpr uint8_t kNoteIdentityCapacity = 64;
constexpr uint8_t kNoteFanOut = 4; // Existing chord pads already need four outputs.
constexpr uint8_t kNoteSourceInstances = 16;

} // namespace smk::config
