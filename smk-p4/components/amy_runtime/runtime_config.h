#pragma once
#include <cstddef>
#include <cstdint>
#include "patch_library.h"
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#define SMKP4_VOICES CONFIG_SMKP4_AMY_VOICES
#ifdef CONFIG_SMKP4_FULL_FACTORY_LIBRARY
#define SMKP4_FULL_FACTORY_LIBRARY 1
#endif
#ifdef CONFIG_SMKP4_HARMONY
#define SMKP4_HARMONY 1
#endif
#ifdef CONFIG_SMKP4_UNIFIED_ENGINE
#define SMKP4_UNIFIED_ENGINE 1
#endif
#ifdef CONFIG_SMKP4_SOUND_LIBRARY
#define SMKP4_SOUND_LIBRARY 1
#endif
#elif !defined(SMKP4_VOICES)
#define SMKP4_VOICES 4
#endif
namespace smk::p4::amy {
constexpr unsigned kVoices = SMKP4_VOICES, kCommandCapacity = 32;
// Burst storage after the32-slot queue lost25 events on the bench. The owner applies
// at most32 current-generation USB commands per block. Legacy profiles stay32.
#ifdef SMKP4_SOUND_LIBRARY
constexpr unsigned kUsbCommandCapacity = 64;
#else
constexpr unsigned kUsbCommandCapacity = kCommandCapacity;
#endif
static_assert(kVoices == 4 || kVoices == 6 || kVoices == 8, "Supported musical voice profiles: 4, 6, 8");
constexpr unsigned kOwnerStackBytes = 12288;
#ifdef SMKP4_UNIFIED_ENGINE
constexpr unsigned kOwnerPriority = 4; // Below independent DMA output owner.
constexpr unsigned kOutputPriority = 6, kOutputStackBytes = 4096, kPcmQueueBlocks = 3;
#else
constexpr unsigned kOwnerPriority = 5;
#endif
// Conservative admission, not a measured P4 allocation size. Preserve the
// original four-voice reserve and allow extra patch/delta/oscillator storage.
// Runtime adds the expanded delta blocks using the actual engine ABI size.
constexpr size_t kBootBaseFreeInternalBytes = 196608 + (kVoices - 4) * 24576;
constexpr unsigned kDeltaBlockSize = 2048; // Pinned AMY DELTA_BLOCK_SIZE.
constexpr size_t kBootLargestInternalBytes = 196608; // Preserve the baseline contiguous reserve.
// Whole-engine budget: measured x86-64 peak <286650 B (larger ABI), plus
// >20 KiB for allocator bookkeeping/margin. Keep historical guards as floors.
constexpr size_t kLibraryEngineHeapBudgetBytes = 300 * 1024;
constexpr size_t kPostBootFreeInternalBytes = 65536;
constexpr size_t kPostBootLargestInternalBytes = 32768;
constexpr uint32_t kSampleRateHz = 48000, kBlockFrames = 256;
constexpr uint32_t kLeaseMs = 10000;
using Patch = library::Patch;
// Fixed boot-resident FM banks. Candidate profile is opt-in; no parsing/reload
// in service, no shared-core changes, no reduction of musical capacity.
#ifdef SMKP4_FULL_FACTORY_LIBRARY
constexpr Patch kPatches[] = {
#include "factory_supported.inc"
};
static_assert(kVoices==8, "Factory qualification requires eight musical voices");
#ifndef SMKP4_UNIFIED_ENGINE
#error Full factory library requires the reusable engine
#endif
#else
constexpr Patch kPatches[] = {library::kCandidates[0], library::kCandidates[1]
#if defined(SMKP4_SOUND_LIBRARY) || defined(SMKP4_UNIFIED_ENGINE)
    , library::kCandidates[2], library::kCandidates[3]
#endif
#ifdef SMKP4_UNIFIED_ENGINE
    , library::kCandidates[4], library::kCandidates[5], library::kCandidates[7],
    library::kCandidates[8], library::kCandidates[9], library::kCandidates[10],
    library::kCandidates[11], library::kCandidates[12], library::kCandidates[13],
    library::kCandidates[14], library::kCandidates[15]
#endif
};
#endif
constexpr size_t kPatchCount = sizeof(kPatches) / sizeof(kPatches[0]);
constexpr unsigned kOscillatorsPerVoice = 8;
constexpr bool residentCostsMatch() {
    for(const auto& patch:kPatches)
        if(patch.oscillators!=kOscillatorsPerVoice || patch.family!=library::SynthFamily::FM) return false;
    return true;
}
#ifndef SMKP4_UNIFIED_ENGINE
static_assert(residentCostsMatch(), "Resident library must match the exclusive FM adapter");
#endif
#ifdef SMKP4_UNIFIED_ENGINE
constexpr unsigned kAmyVoices = kVoices;
#else
constexpr unsigned kAmyVoices = kVoices * kPatchCount;
#endif
constexpr unsigned kSynthCount = kAmyVoices + 1; // Instrument ID zero is reserved.
constexpr unsigned kOscillators = kAmyVoices * kOscillatorsPerVoice;
// Bound simultaneous Touch retriggers, USB per-channel tuning and the next
// vibrato update, plus a full-bank safety command. Both producer service budgets are32; USB storage may hold64.
// A Touch retrigger emits Off+On (four fields/osc). At least one slot belongs
// to Touch in that case, leaving at most N-1 USB voices for every queued CC.
constexpr unsigned kDeltaReserve = kOscillatorsPerVoice *
    (
#ifdef SMKP4_HARMONY
     16 * kCommandCapacity + // Four-note Touch Off+On per input event.
#else
     4 * kCommandCapacity +
#endif
     (kCommandCapacity + 1) * (kVoices - 1) + 2 * kAmyVoices);
static_assert(kSynthCount < UINT8_MAX && kOscillators < UINT16_MAX);
static_assert(kVoices < kCommandCapacity);
inline int patchIndex(uint16_t id) {
    for (size_t i=0;i<kPatchCount;++i) if(kPatches[i].id==id) return static_cast<int>(i);
    return -1;
}
}
