#pragma once
#include <cstdint>

namespace smk::config {

// Shared by the application and adapter; no board or RTOS dependencies.
constexpr uint16_t kMaxOscillators = 120;
constexpr uint8_t  kDefaultPatchNumber = 0;
constexpr uint8_t  kDefaultVoiceCount = 8;
constexpr uint16_t kDrumPatchNumber = 258;
constexpr uint16_t kSynthCommandCapacity = 64;
constexpr uint16_t kSynthReleaseReserve = 8;
constexpr uint16_t kSynthCommandsPerBlock = 16;
constexpr uint16_t kSynthMessageBytes = 256;
constexpr uint8_t kSynthPcmBlocks = 2;

} // namespace smk::config
