#pragma once
#include "patch_types.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace smk {

// ─────────────────────────────────────────────────────────────
// Sound & Musicality M2: family-aware musical macro destinations.
//
// MacroMapping::param_type is a uint8_t and is persisted inside the v5 patch
// format. The legacy values 0..7 keep their exact previous meaning so existing
// patches keep loading and behaving as before. The new relative destinations
// start at 8 and are only emitted by the factory macro profiles generated at
// runtime; they are never required to interpret older data.
// ─────────────────────────────────────────────────────────────
enum class MacroTarget : uint8_t {
    LegacyCutoff      = 0, // absolute Hz
    LegacyResonance   = 1, // absolute DSP resonance
    LegacyBrightness  = 2, // absolute cutoff (subtractive) / mod index (FM)
    LegacyAttack      = 3, // absolute ms
    LegacyRelease     = 4, // absolute ms
    LegacyMotion      = 5, // chorus depth
    LegacySpace       = 6, // reverb mix
    LegacyDrive       = 7, // drive (subtractive) / FM feedback

    FilterCutoffRelative = 8,  // octave offset applied to the baseline cutoff
    FilterResRelative    = 9,  // multiplier applied to the baseline resonance
    FilterEnvRelative    = 10, // additive offset to the filter envelope amount
    AmpAttackRelative    = 11, // multiplier applied to the baseline attack
    AmpDecayRelative     = 12, // multiplier applied to the baseline decay
    AmpSustainRelative   = 13, // additive offset to the baseline sustain
    AmpReleaseRelative   = 14, // multiplier applied to the baseline release
    OscDetuneRelative    = 15, // additive cents offset
    ChorusDepth          = 16, // additive chorus depth offset
    ReverbMix            = 17, // additive reverb mix offset
    DelayMix             = 18, // additive delay mix offset
    DriveRelative        = 19, // additive drive offset relative to baseline
    MasterToneRelative   = 20, // additive master tilt-EQ offset
    FmModIndexRelative   = 21, // multiplier applied to per-operator mod levels
    FmRatioRelative      = 22, // multiplier applied to modulator ratios
    FmDetuneRelative     = 23, // additive cents offset on FM modulators
    FmFeedbackRelative   = 24, // additive feedback offset relative to baseline
};

inline bool isLegacyMacroTarget(uint8_t param_type) {
    return param_type <= static_cast<uint8_t>(MacroTarget::LegacyDrive);
}

// ─────────────────────────────────────────────────────────────
// Immutable per-session baseline captured when a patch is loaded. Macros are
// always recomputed as baseline + contributions, never as an incremental
// modification of the previous result, so moving a macro back to neutral or
// visiting macros in a different order cannot accumulate drift.
// ─────────────────────────────────────────────────────────────
struct MacroBaseline {
    float filter_cutoff = 1000.0f;
    float filter_res = 1.0f;
    float filter_env = 0.0f;

    float amp_attack = 10.0f;
    float amp_decay = 300.0f;
    float amp_sustain = 0.8f;
    float amp_release = 300.0f;

    float osc_detune = 0.0f;

    float chorus_depth = 0.0f;
    float delay_time_ms = 350.0f;
    float delay_feedback = 0.4f;
    float delay_mix = 0.0f;
    float reverb_size = 0.7f;
    float reverb_mix = 0.0f;

    float drive = 0.0f;
    float master_tone = 0.0f;

    // FM relative controls. These are not absolute operator levels: they are
    // factors/offsets on top of the timbre the loaded preset already produces.
    float   fm_mod_factor = 1.0f;
    float   fm_ratio_factor = 1.0f;
    float   fm_detune_cents = 0.0f;
    float   fm_feedback = 0.0f; // absolute DSP feedback value
    uint8_t fm_algorithm = 1;
    bool    fm_valid = false;
};

// Maps a macro position (0..1) to a bipolar deviation in [-1, +1] around the
// macro's neutral position. neutral == 0 and the extremes map to -1 / +1, so
// the neutral macro position always reproduces the baseline exactly.
inline float macroBipolar(float norm_val, float neutral_val) {
    norm_val = std::clamp(norm_val, 0.0f, 1.0f);
    neutral_val = std::clamp(neutral_val, 0.0f, 1.0f);
    const float span_up = 1.0f - neutral_val;
    const float span_dn = neutral_val;
    if (norm_val >= neutral_val) {
        return (span_up > 1e-4f) ? (norm_val - neutral_val) / span_up : 0.0f;
    }
    return (span_dn > 1e-4f) ? (norm_val - neutral_val) / span_dn : 0.0f;
}

// Bipolar magnitude shaping that preserves t == 0 (neutral), so the macro curve
// never shifts the neutral point.
inline float macroShapeBipolar(float t, uint8_t curve_type) {
    const float sign = (t < 0.0f) ? -1.0f : 1.0f;
    float a = std::fabs(t);
    if (curve_type == 1) {
        a = a * a; // exponential: fine near neutral
    } else if (curve_type == 2) {
        a = std::sqrt(a); // logarithmic: active near neutral
    }
    return sign * a;
}

// Factor targets: 1.0 at neutral, interpolating to the configured factor.
inline float macroFactor(float t, float min_factor, float max_factor) {
    if (t >= 0.0f) return 1.0f + (max_factor - 1.0f) * t;
    return 1.0f - (1.0f - min_factor) * (-t);
}

// Offset targets: 0.0 at neutral, interpolating to the configured offset.
inline float macroOffset(float t, float min_offset, float max_offset) {
    return (t >= 0.0f) ? (max_offset * t) : (min_offset * (-t));
}

} // namespace smk
