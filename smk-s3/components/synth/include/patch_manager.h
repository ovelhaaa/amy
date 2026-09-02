#pragma once
#include "patch_types.h"
#include "soft_takeover.h"
#include "factory_patches.h"

namespace smk {

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

    const SynthPatch& activePatch() const { return active_patch_; }
    uint8_t activePatchId() const { return active_patch_.id; }
    SoftTakeover& softTakeover() { return soft_takeover_; }

private:
    void applyPatchToEngine(const SynthPatch& patch);
    void applyMacroToEngine(uint8_t macro_idx, float effective_val);

    AmyAdapter*    amy_adapter_    = nullptr;
    UIManager*     ui_manager_     = nullptr;
    ClockManager*  clock_manager_  = nullptr;
    Arpeggiator*   arpeggiator_    = nullptr;
    StepSequencer* sequencer_      = nullptr;
    SynthPatch     active_patch_;
    KnobBank       active_bank_    = KnobBank::BankA_Macros;
    SoftTakeover   soft_takeover_;
    std::array<uint8_t, 4> bank_b_fx_values_{0, 0, 40, 15}; // Chorus, Delay, Reverb, Drive
};

} // namespace smk
