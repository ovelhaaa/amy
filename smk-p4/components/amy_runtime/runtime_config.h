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
constexpr size_t kBootLargestInternalBytes = 196608 + (kVoices - 4) * 24576;
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
static_assert(kSynthCount < UINT8_MAX && kOscillators < UINT16_MAX);
static_assert(kVoices < kCommandCapacity);
inline int patchIndex(uint16_t id) {
    for (size_t i=0;i<kPatchCount;++i) if(kPatches[i].id==id) return static_cast<int>(i);
    return -1;
}
}
