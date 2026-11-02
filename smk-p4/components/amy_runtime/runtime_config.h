#pragma once
#include <cstddef>
#include <cstdint>
namespace smk::p4::amy {
constexpr unsigned kVoices = 4, kOscillators = 64, kCommandCapacity = 32;
constexpr unsigned kOwnerStackBytes = 12288, kOwnerPriority = 5;
constexpr uint32_t kSampleRateHz = 48000, kBlockFrames = 256;
constexpr uint32_t kLeaseMs = 10000;
struct Patch { uint16_t id; const char* name; };
// src/patches.h: eight oscillators per FM voice. Both banks allocated at boot;
// only one is playable at a time. Switching never materializes a patch in render.
constexpr Patch kPatches[] = {{128, "DX7 BRASS   1"}, {135, "DX7 PIANO   1"}};
constexpr size_t kPatchCount = sizeof(kPatches) / sizeof(kPatches[0]);
inline int patchIndex(uint16_t id) {
    for (size_t i=0;i<kPatchCount;++i) if(kPatches[i].id==id) return static_cast<int>(i);
    return -1;
}
}
