#pragma once
#include "patch_types.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace smk {

// ─────────────────────────────────────────────────────────────
// Sound & Musicality M2: family-aware musical macro destinations.
//
// MacroMapping::param_type is a uint8_t and is persisted inside the patch
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

// Classifies how a patch's persisted macro mappings are expressed. Legacy
// values 0..7 use the original absolute handler; the relative destinations use
// the deterministic baseline+contribution model. Mixed is only reachable from a
// hand-crafted patch and is called out explicitly instead of silently dropping
// the legacy routes.
enum class MacroMappingMode : uint8_t {
    None,          // No macro routes at all.
    LegacyOnly,    // Only param_type 0..7.
    RelativeOnly,  // Only the family-aware relative destinations.
    Mixed          // Both kinds in the same patch.
};

// Pure classifier over the persisted macro routes. Kept free-standing so the
// mixed/legacy decision is unit-testable without constructing a PatchManager.
inline MacroMappingMode classifyMacroMappings(const SynthPatch& patch) {
    bool saw_legacy = false;
    bool saw_relative = false;
    for (uint8_t i = 0; i < 8; ++i) {
        const MacroConfig& macro = patch.macros[i];
        for (uint8_t m = 0; m < macro.mapping_count && m < 4; ++m) {
            if (isLegacyMacroTarget(macro.mappings[m].param_type)) {
                saw_legacy = true;
            } else {
                saw_relative = true;
            }
        }
    }
    if (saw_legacy && saw_relative) return MacroMappingMode::Mixed;
    if (saw_legacy) return MacroMappingMode::LegacyOnly;
    if (saw_relative) return MacroMappingMode::RelativeOnly;
    return MacroMappingMode::None;
}

// ─────────────────────────────────────────────────────────────
// Sound & Musicality M2.1: runtime manual-control state.
//
// The detailed Bank B/C/D controls are the *source* of the sound, not the
// final applied state. This struct holds the current manual value of every
// control that a macro can also touch. Macros are then composed on top:
//
//   PATCH BASELINE -> MANUAL STATE -> MACRO CONTRIBUTIONS -> FINAL ENGINE STATE
//
// On patch load the manual state is initialized from the patch/FX state, so it
// starts equal to the patch baseline. Moving a detailed control updates the
// manual state and triggers a deterministic recompute. Reverting a macro to
// neutral restores exactly the manual state, never the factory baseline, and
// never drifts because the final applied value is never read back as manual.
//
// This state is runtime-only. The persisted patch stores the manual base for
// every control a macro can touch (v6), never the macro-processed result; only
// the macro *positions* live in SynthPatch::macros.
// ─────────────────────────────────────────────────────────────
struct ManualControlState {
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

    // FM manual controls. Mod/ratio are manual factors (1.0 == centered); the
    // macros further multiply or offset them. Feedback and algorithm are seeded
    // from the real loaded preset once it materializes, so a neutral macro
    // restores the preset's own feedback rather than assuming zero.
    float   fm_mod_factor = 1.0f;
    float   fm_ratio_factor = 1.0f;
    float   fm_detune_cents = 0.0f;
    float   fm_freq_mult = 1.0f; // discrete Bank B frequency multiplier (manual only)
    float   fm_feedback = 0.0f;  // absolute preset/manual DSP feedback
    uint8_t fm_algorithm = 1;    // manual only; macros never change it
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
