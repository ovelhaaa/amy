#pragma once
#include "patch_types.h"
#include <cstdint>

namespace smk {

// ─────────────────────────────────────────────────────────────
// Runtime synthesis family classification.
//
// PatchFamily is derived, never persisted: the patch format (v5) is unchanged
// and SynthPatch has no family field. Classification is driven primarily by
// wave_type; engine_patch ranges are only an auxiliary fallback for values
// outside the known wave enum, so control routing does not depend on magic
// preset-number ranges.
// ─────────────────────────────────────────────────────────────
enum class PatchFamily : uint8_t {
    Subtractive,
    FM,
    PCM,
    KarplusStrong,
    Noise,
    Generic
};

inline PatchFamily classifyPatch(const SynthPatch& patch) {
    switch (static_cast<SmkWaveType>(patch.wave_type)) {
        case SmkWaveType::Algo:          return PatchFamily::FM;
        case SmkWaveType::Pcm:           return PatchFamily::PCM;
        case SmkWaveType::KarplusStrong: return PatchFamily::KarplusStrong;
        case SmkWaveType::Noise:         return PatchFamily::Noise;
        case SmkWaveType::Sine:
        case SmkWaveType::Pulse:
        case SmkWaveType::SawDown:
        case SmkWaveType::SawUp:
        case SmkWaveType::Triangle:
            return PatchFamily::Subtractive;
        default:
            break;
    }

    // Auxiliary fallback only: wave_type is outside the known enum.
    if (patch.engine_patch >= 128 && patch.engine_patch < 256) return PatchFamily::FM;
    if (patch.engine_patch >= 256) return PatchFamily::PCM;
    return PatchFamily::Generic;
}

// A generic synth-level AMY ADSR applies to every non-FM family. FM (ALGO/DX7)
// owns per-operator envelopes, so a generic ADSR would overwrite the operators.
inline bool supportsGenericAmpEnvelope(PatchFamily family) {
    return family != PatchFamily::FM;
}

// The subtractive oscillator controls (osc mix/detune/sub/noise) address
// base/+1/+2/+3. In an FM voice those slots are the FM control osc and its
// operators, so they are guarded exactly like the generic amp envelope.
inline bool supportsSubtractiveOscControls(PatchFamily family) {
    return family != PatchFamily::FM;
}

inline bool supportsFmControls(PatchFamily family) {
    return family == PatchFamily::FM;
}

inline const char* patchFamilyName(PatchFamily family) {
    switch (family) {
        case PatchFamily::Subtractive: return "SUBTRACTIVE";
        case PatchFamily::FM:          return "FM";
        case PatchFamily::PCM:         return "PCM";
        case PatchFamily::KarplusStrong: return "KARPLUS";
        case PatchFamily::Noise:       return "NOISE";
        case PatchFamily::Generic:     return "GENERIC";
        default:                       return "UNKNOWN";
    }
}

} // namespace smk
