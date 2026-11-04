#pragma once
#include <cstddef>
#include <cstdint>
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#define SMKP4_VOICES CONFIG_SMKP4_AMY_VOICES
#elif !defined(SMKP4_VOICES)
#define SMKP4_VOICES 4
#endif
namespace smk::p4::amy {
constexpr unsigned kVoices = SMKP4_VOICES, kCommandCapacity = 32;
static_assert(kVoices == 4 || kVoices == 6 || kVoices == 8, "Supported musical voice profiles: 4, 6, 8");
constexpr unsigned kOwnerStackBytes = 12288, kOwnerPriority = 5;
// Conservative admission, not a measured P4 allocation size. Preserve the
// original four-voice reserve and allow extra patch/delta/oscillator storage.
// Runtime adds the expanded delta blocks using the actual engine ABI size.
constexpr size_t kBootBaseLargestInternalBytes = 196608 + (kVoices - 4) * 24576;
constexpr unsigned kDeltaBlockSize = 2048; // Pinned AMY DELTA_BLOCK_SIZE.
constexpr size_t kPostBootFreeInternalBytes = 65536;
constexpr size_t kPostBootLargestInternalBytes = 32768;
constexpr uint32_t kSampleRateHz = 48000, kBlockFrames = 256;
constexpr uint32_t kLeaseMs = 10000;
struct Patch { uint16_t id; const char* name; };
// src/patches.h: eight oscillators per FM voice. Both banks allocated at boot;
// only one is playable at a time. Switching never materializes a patch in render.
constexpr Patch kPatches[] = {{128, "DX7 BRASS   1"}, {135, "DX7 PIANO   1"}};
constexpr size_t kPatchCount = sizeof(kPatches) / sizeof(kPatches[0]);
constexpr unsigned kOscillatorsPerVoice = 8;
constexpr unsigned kAmyVoices = kVoices * kPatchCount;
constexpr unsigned kSynthCount = kAmyVoices + 1; // Instrument ID zero is reserved.
constexpr unsigned kOscillators = kAmyVoices * kOscillatorsPerVoice;
// Bound simultaneous Touch retriggers, USB per-channel tuning and the next
// vibrato update, plus a full-bank safety command. Both producer queues are 32.
// A Touch retrigger emits Off+On (four fields/osc). At least one slot belongs
// to Touch in that case, leaving at most N-1 USB voices for every queued CC.
constexpr unsigned kDeltaReserve = kOscillatorsPerVoice *
    (4 * kCommandCapacity + (kCommandCapacity + 1) * (kVoices - 1) + 2 * kAmyVoices);
static_assert(kSynthCount < UINT8_MAX && kOscillators < UINT16_MAX);
static_assert(kVoices < kCommandCapacity);
inline int patchIndex(uint16_t id) {
    for (size_t i=0;i<kPatchCount;++i) if(kPatches[i].id==id) return static_cast<int>(i);
    return -1;
}
}
