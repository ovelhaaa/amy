#pragma once
#include "patch_types.h"
#include "patch_family.h"
#include "macro_profile.h"
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
class StorageManager;

// Where the currently active patch came from. This is provenance only; it is
// intentionally independent from the selected user storage slot below.
enum class PatchSource : uint8_t {
    Factory = 0, // embedded factory table / in-memory selection
    Storage = 1  // loaded from a user storage slot
};

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

    /**
     * @brief Apply an already-materialized patch (factory or loaded from storage).
     *
     * Factory selection and stored-patch reload both converge here so they share
     * the same engine apply, soft-takeover reset and UI refresh. The stored patch
     * must already be CRC-verified by StorageManager.
     *
     * This overload carries no provenance and does NOT change the selected user
     * storage slot. Patch identity and storage location are separate domains:
     *   SynthPatch::id        = the patch's own identity (factory id / engine
     *                           source). It is never a storage address.
     *   active_storage_slot_  = the user slot an unqualified Save targets.
     * See docs/patch_storage_semantics.md.
     */
    bool applyLoadedPatch(const SynthPatch& patch);

    /**
     * @brief Apply a stored patch and record the user slot it was loaded from.
     *
     * Loading from storage selects that slot, so an unqualified Save writes back
     * to the same place. The slot is metadata; it is never folded into
     * SynthPatch::id.
     */
    bool applyLoadedPatch(const SynthPatch& patch, uint8_t storage_slot);

    /**
     * @brief Build the semantic snapshot that should be persisted for the active
     * patch: metadata and macro positions from the active patch, with every
     * macro-affected sound parameter replaced by the *manual* base value.
     *
     * Persisting the final macro-processed value alongside the macro positions
     * would double-apply the macro on reload. The returned patch has crc32 == 0;
     * StorageManager::savePatch() is responsible for recomputing and writing the
     * CRC. This does not modify the active runtime state.
     */
    SynthPatch buildPersistablePatch() const;

    // ── Patch identity vs. storage location (M2.3) ──────────────────────────
    // The selected user storage slot is what an unqualified Save writes to.
    // It defaults to a fixed, documented slot so even a factory FM/DX7 patch
    // (whose id is >= 128) can always be saved. Selecting a factory patch does
    // NOT change it: browsing the factory bank must never retarget a user save.
    static constexpr uint8_t kDefaultStorageSlot = 0;

    PatchSource activePatchSource() const { return active_patch_source_; }
    uint8_t activeStorageSlot() const { return active_storage_slot_; }
    void setActiveStorageSlot(uint8_t slot);

    /**
     * @brief Build the persistable snapshot and save it through StorageManager.
     *
     * Single save path shared by the console (`patch_save`) and the hardware
     * long-hold gesture. The no-slot form targets the selected user slot; the
     * explicit form also selects that slot on success, so a following
     * unqualified Save repeats there. Returns false if the slot is out of range
     * or the underlying write/verify fails.
     */
    bool saveActivePatch(StorageManager& storage);
    bool saveActivePatch(StorageManager& storage, uint8_t slot);

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

    // M2.1 state model:
    //   active_patch_ / fx_state_ / fm_state_ = final effective musical state
    //   manualControlState()                  = detailed-control base state
    // Macros are composed as final = manual (+) macro contribution. The final
    // state is derived only and is never read back as the manual base.
    const ManualControlState& manualControlState() const { return manual_state_; }

    void setFilterType(uint8_t filter_type);
    uint8_t activeFilterType() const { return active_filter_type_; }

    void applyActiveFilterState();
    void applyActiveChorusState();

    // True when the active patch mixes legacy (0..7) and relative macro routes.
    // Such a patch is unsupported: the relative routes are applied and the
    // legacy routes are reported instead of silently ignored.
    bool hasMixedMacroMappings() const {
        return macroMappingMode() == MacroMappingMode::Mixed;
    }
    MacroMappingMode macroMappingMode() const;

private:
    // Runtime family of the active patch, derived from wave_type (never stored).
    PatchFamily activeFamily() const { return classifyPatch(active_patch_); }

    void applyPatchToEngine(const SynthPatch& patch);
    // Legacy absolute macro path, kept for patches whose persisted mappings use
    // the original param_type 0..7 destinations.
    void applyMacroToEngine(uint8_t macro_idx, float effective_val);
    // Pull the algorithm/feedback baseline from the engine once the loaded
    // preset has materialized. Safe to call on every FM knob event; cheap.
    void syncFmStateFromBaseline();

    // Sound & Musicality M2.1: deterministic family-aware macro model.
    // Capture the runtime manual-control state from the loaded patch and the
    // FX/FM state that applyPatchToEngine just configured. The manual state is
    // the base layer that macros compose on top of; it starts equal to the
    // patch baseline.
    void captureManualControlState();
    // Recompute every target from the manual state and the eight macro
    // positions, then reconcile only the engine parameters that actually
    // changed. Deterministic and order-independent.
    void recomputeMacroTargets();

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

    // M2.3 storage provenance. Kept separate from SynthPatch::id on purpose.
    uint8_t        active_storage_slot_ = kDefaultStorageSlot;
    PatchSource    active_patch_source_ = PatchSource::Factory;

    // Runtime-only manual-control base state for the relative macro model.
    // Captured on patch load; never stored in the patch (format stays v5).
    ManualControlState manual_state_;
    // One diagnostic per patch load when an unsupported mixed mapping is seen.
    bool mixed_macro_warning_emitted_ = false;
};

} // namespace smk
