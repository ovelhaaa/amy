#pragma once
#include "patch_types.h"
#include "patch_family.h"
#include "control_mappings.h"
#include "soft_takeover.h"
#include "factory_patches.h"
#include <algorithm>
#include <cmath>

namespace smk {

// ─────────────────────────────────────────────────────────────
// FM operator control mappings. The forward form maps a normalized knob
// position (0..1) to the engine parameter; the inverse maps a baseline value
// back to the knob position, which is what soft takeover compares against.
// Keeping both directions together prevents the two from drifting apart.
// ─────────────────────────────────────────────────────────────
inline float fmModFactorFromNorm(float norm) {
    norm = std::clamp(norm, 0.0f, 1.0f);
    return (norm <= 0.5f) ? (norm * 2.0f) : (1.0f + (norm - 0.5f) * 6.0f);
}
inline float fmModNormFromFactor(float factor) {
    factor = std::clamp(factor, 0.0f, 4.0f);
    return (factor <= 1.0f) ? (factor * 0.5f) : (0.5f + (factor - 1.0f) / 6.0f);
}
inline float fmRatioFactorFromNorm(float norm) {
    return std::pow(2.0f, (std::clamp(norm, 0.0f, 1.0f) - 0.5f) * 2.0f);
}
inline float fmRatioNormFromFactor(float factor) {
    factor = std::max(factor, 0.001f);
    return std::clamp(0.5f + 0.5f * std::log2(factor), 0.0f, 1.0f);
}
inline float fmDetuneCentsFromNorm(float norm) {
    return (std::clamp(norm, 0.0f, 1.0f) - 0.5f) * 50.0f;
}
inline float fmDetuneNormFromCents(float cents) {
    return std::clamp(cents / 50.0f + 0.5f, 0.0f, 1.0f);
}
inline float fmFreqMultFromNorm(float norm) {
    return 1.0f + std::round(std::clamp(norm, 0.0f, 1.0f) * 7.0f);
}
inline float fmFreqMultNormFromMult(float mult) {
    return std::clamp((mult - 1.0f) / 7.0f, 0.0f, 1.0f);
}
inline float fmFeedbackFromNorm(float norm) {
    return std::clamp(norm, 0.0f, 1.0f) * 0.16f;
}
inline float fmFeedbackNormFromValue(float feedback) {
    return std::clamp(feedback / 0.16f, 0.0f, 1.0f);
}
inline uint8_t fmAlgorithmFromNorm(float norm) {
    const int a = std::clamp(static_cast<int>(std::round(std::clamp(norm, 0.0f, 1.0f) * 31.0f)), 0, 31);
    return static_cast<uint8_t>(1 + a);
}
inline float fmAlgorithmNormFromValue(uint8_t algorithm) {
    const uint8_t a = std::clamp<uint8_t>(algorithm, 1, 32);
    return static_cast<float>(a - 1) / 31.0f;
}

// Runtime-only state of the Bank B FM controls. Not persisted: it describes
// how far the user has moved each relative FM control from the patch baseline.
struct FmControlState {
    float   mod_factor   = 1.0f;  // 0x .. 4x, center 1x
    float   ratio_factor = 1.0f;  // 0.5x .. 2x continuous, center 1x
    float   detune_cents = 0.0f;  // -25 .. +25 cents
    float   freq_mult    = 1.0f;  // discrete 1x .. 8x
    float   feedback     = 0.0f;  // 0 .. 0.16 (DSP feedback scale)
    uint8_t algorithm    = 1;     // 1 .. 32
    bool    initialized  = false; // baseline pulled from the engine
};

class AmyAdapter;
class UIManager;
class ClockManager;
class Arpeggiator;
class StepSequencer;

class PatchManager {
public:
    PatchManager();

    bool begin(AmyAdapter* amy_adapter, UIManager* ui_manager);

    void setClockManager(ClockManager* clock_mgr) { clock_manager_ = clock_mgr; }
    void setArpeggiator(Arpeggiator* arp) { arpeggiator_ = arp; }
    void setStepSequencer(StepSequencer* seq) { sequencer_ = seq; }

    bool selectPatch(uint8_t patch_id);
    bool selectPatchByIndex(size_t index);
    void nextPatch();
    void previousPatch();

    KnobBank activeKnobBank() const { return active_bank_; }
    void nextKnobBank();
    void setKnobBank(KnobBank bank);

    /**
     * @brief Process input from a physical knob (0..7) based on active KnobBank.
     */
    void handleKnobInput(uint8_t knob_idx, float physical_val);

    /**
     * @brief Modify a macro value (0..7) and apply its mappings to the synth engine and UI.
     * @param macro_idx Index of the macro (0 to 7)
     * @param physical_val Value (0.0 to 127.0)
     * @param from_physical_knob True if originating from hardware knob (triggers Soft Takeover)
     */
    void setMacro(uint8_t macro_idx, float physical_val, bool from_physical_knob = true);

    using FxControlState = smk::FxControlState;

    const SynthPatch& activePatch() const { return active_patch_; }
    uint8_t activePatchId() const { return active_patch_.id; }
    SoftTakeover& softTakeover() { return soft_takeover_; }
    const FxControlState& fxControlState() const { return fx_state_; }
    const FmControlState& fmControlState() const { return fm_state_; }

    void setFilterType(uint8_t filter_type);
    uint8_t activeFilterType() const { return active_filter_type_; }

    void applyActiveFilterState();
    void applyActiveChorusState();

private:
    // Runtime family of the active patch, derived from wave_type (never stored).
    PatchFamily activeFamily() const { return classifyPatch(active_patch_); }

    void applyPatchToEngine(const SynthPatch& patch);
    void applyMacroToEngine(uint8_t macro_idx, float effective_val);
    // Pull the algorithm/feedback baseline from the engine once the loaded
    // preset has materialized. Safe to call on every FM knob event; cheap.
    void syncFmStateFromBaseline();

    AmyAdapter*    amy_adapter_    = nullptr;
    UIManager*     ui_manager_     = nullptr;
    ClockManager*  clock_manager_  = nullptr;
    Arpeggiator*   arpeggiator_    = nullptr;
    StepSequencer* sequencer_      = nullptr;
    SynthPatch     active_patch_;
    KnobBank       active_bank_    = KnobBank::BankA_Macros;
    SoftTakeover   soft_takeover_;
    FxControlState fx_state_;
    FmControlState fm_state_;
    float          active_filter_env_amt_   = 0.0f;
    float          active_filter_key_track_ = 0.0f;
    float          active_filter_vel_track_ = 1.5f;
    uint8_t        active_filter_type_      = 0;
};

} // namespace smk
