#pragma once
#include "ui_parameters.h"
#include "../../amy_runtime/patch_library.h"
#include <cstddef>
#include <cstdint>

namespace smk::p4::ui {

using SynthFamily = library::SynthFamily;
using PatchCategory = library::PatchCategory;
const char* synthFamilyName(SynthFamily family);
const char* patchCategoryName(PatchCategory cat);

struct BlueprintPatchEntry {
    uint16_t      id;
    const char*   name;
    const char*   bank_name;     // "ROM P4", "ROM JUNO", "ROM DX7", "USER DEMO"
    PatchCategory category;
    SynthFamily   family;
    FeatureStatus status;        // Available (128, 135 on P4) vs Planned (S3/AMY ROM) vs Demo (User slot)
    bool          favorite_default;
    const char*   macro_labels[8];
};

struct StepCell {
    bool    active      = false;
    uint8_t note        = 60;
    uint8_t velocity    = 100;
    uint8_t probability = 100;
    uint8_t ratchet     = 1;
};

struct ModSlot {
    bool    enabled  = false;
    uint8_t source   = 0;
    uint8_t dest     = 0;
    int8_t  amount   = 0; // -100 .. +100
    uint8_t polarity = 0; // 0=Uni+, 1=Bipolar, 2=Inv-
    uint8_t curve    = 0; // 0=Lin, 1=Exp, 2=Log
};

struct MidiCcMapping {
    const char* control_label; // e.g. "SMK KNOB 1"
    uint8_t     channel;       // 1..16 (0=OMNI)
    uint8_t     cc_number;     // 0..127
    const char* target_param_id;
    bool        learning;
};

struct HarmonyPreviewResult {
    bool     is_demo_calculation = true;
    uint8_t  input_note          = 60;
    uint8_t  quantized_note      = 60;
    bool     input_in_scale      = true;
    uint16_t scale_mask_12       = 0x0FFF; // Bit b set if pitch class b (0=C..11=B) is in active scale
    uint8_t  chord_notes[6]      = {60, 0, 0, 0, 0, 0};
    uint8_t  chord_count         = 1;
};

struct DiagnosticsSnapshot {
    bool        is_demo_mode         = true;
    bool        midi_connected       = false;
    bool        audio_live           = false;
    bool        memory_valid         = false;
    const char* firmware_version     = "SMK-P4 BLUEPRINT 1.1 (DEMO)";
    const char* amy_status           = "DISCONNECTED (UI DEMO)";
    const char* i2s_status           = "NOT INITIALIZED (UI DEMO)";
    const char* usb_status           = "NOT INITIALIZED (UI DEMO)";
    uint8_t     active_voices        = 0;
    uint8_t     max_voices           = 4;
    uint32_t    render_avg_us        = 0;
    uint32_t    render_max_us        = 0;
    uint32_t    budget_us            = 5333;
    uint32_t    deadline_misses      = 0;
    uint32_t    free_internal_ram    = 0;
    uint32_t    free_psram           = 0;
    uint32_t    lvgl_free_bytes      = 0;
    uint32_t    lvgl_largest_block   = 0;
    uint32_t    lvgl_peak_used       = 0;
    uint8_t     lvgl_frag_pct        = 0;
    uint32_t    uptime_ms            = 0;
    uint32_t    screen_switches      = 0;
    uint32_t    demo_panic_count     = 0;
};

class UiModel {
public:
    static constexpr size_t kMaxParameters = 160;
    static constexpr size_t kSeqSteps      = 16;
    static constexpr size_t kDrumTracks    = 8;
    static constexpr size_t kModSlots      = 8;
    static constexpr size_t kMidiMappings  = 8;
    static constexpr size_t kCatalogSize   = 16;

    UiModel();

    void resetToDefaults();

    // Parameter access & validation
    bool getValue(const char* id, ParameterValue& out_val) const;
    float getFloat(const char* id, float fallback = 0.0f) const;
    int32_t getInt(const char* id, int32_t fallback = 0) const;
    bool getBool(const char* id, bool fallback = false) const;

    bool setValue(const char* id, const ParameterValue& val);
    bool setFloat(const char* id, float val);
    bool setInt(const char* id, int32_t val);
    bool setBool(const char* id, bool val);

    // Evaluates whether a parameter is currently applicable given patch family & dependencies
    FeatureStatus effectiveStatus(const char* id) const;
    bool isParameterEnabled(const char* id) const;

    // Patch Catalog & Selection
    static const BlueprintPatchEntry* catalog(size_t& out_count);
    const BlueprintPatchEntry& activePatch() const;
    const BlueprintPatchEntry& layerPatch(uint8_t layer_idx) const;
    bool selectPatchById(uint16_t patch_id, uint8_t target_layer = 0);
    bool isFavorite(uint16_t patch_id) const;
    void toggleFavorite(uint16_t patch_id);

    uint8_t activeEditLayer() const { return active_edit_layer_; }
    void setActiveEditLayer(uint8_t layer) { active_edit_layer_ = (layer > 1) ? 1 : layer; }

    // Sequencer & Drums state (RAM-only demo state)
    const StepCell& seqStep(size_t step_idx) const;
    void toggleSeqStep(size_t step_idx);
    void setSeqStepNote(size_t step_idx, uint8_t note, uint8_t velocity);
    size_t selectedSeqStep() const { return selected_seq_step_; }
    void setSelectedSeqStep(size_t step_idx);

    bool drumStep(size_t track_idx, size_t step_idx) const;
    void toggleDrumStep(size_t track_idx, size_t step_idx);

    // Modulation matrix slots
    const ModSlot& modSlot(size_t slot_idx) const;
    void setModSlot(size_t slot_idx, const ModSlot& slot);

    // MIDI mappings
    const MidiCcMapping& midiMapping(size_t idx) const;
    void setMidiMappingCc(size_t idx, uint8_t cc_num, uint8_t channel);
    void setMidiLearnSlot(int idx);
    int midiLearnSlot() const { return active_learn_slot_; }

    // Scale Quantizer & Chord Assist (visual demo calculation)
    HarmonyPreviewResult computeHarmony(uint8_t raw_midi_note) const;
    uint8_t lastPreviewNote() const { return last_preview_note_; }
    void setLastPreviewNote(uint8_t note) { last_preview_note_ = note & 0x7F; }

    // Revision & Diagnostics
    uint32_t revision() const { return revision_; }
    void bumpRevision() { ++revision_; }
    const char* lastNotice() const { return last_notice_; }
    void setNotice(const char* msg);

    DiagnosticsSnapshot& diagnostics() { return diag_; }
    const DiagnosticsSnapshot& diagnostics() const { return diag_; }

private:
    ParameterValue values_[kMaxParameters]{};
    bool           favorites_[kCatalogSize]{};
    uint8_t        active_edit_layer_ = 0; // 0 = Layer A, 1 = Layer B
    size_t         selected_seq_step_ = 0;
    StepCell       seq_steps_[kSeqSteps]{};
    bool           drum_grid_[kDrumTracks][kSeqSteps]{};
    ModSlot        mod_slots_[kModSlots]{};
    MidiCcMapping  midi_mappings_[kMidiMappings]{};
    int            active_learn_slot_ = -1;
    uint8_t        last_preview_note_ = 60; // C4
    uint32_t       revision_          = 1;
    char           last_notice_[64]   = "UI DEMO: RAM ONLY (NO AMY)";
    DiagnosticsSnapshot diag_{};
};

} // namespace smk::p4::ui
