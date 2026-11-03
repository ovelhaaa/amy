#include "ui_model.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace smk::p4::ui {

namespace {

const BlueprintPatchEntry kPatchCatalog[UiModel::kCatalogSize] = {
    // Real P4 standalone ROM patches (Available in smk-p4/components/amy_runtime/runtime_config.h)
    {128, "DX7 BRASS 1",     "ROM P4",   PatchCategory::Brass,         SynthFamily::FM,            FeatureStatus::Available, true,
     {"CHAR", "BRTE", "MOTN", "FDBK", "RATIO", "DTUNE", "SPCE", "EDGE"}},
    {135, "DX7 PIANO 1",     "ROM P4",   PatchCategory::Piano,         SynthFamily::FM,            FeatureStatus::Available, true,
     {"CHAR", "BRTE", "MOTN", "FDBK", "RATIO", "DTUNE", "SPCE", "EDGE"}},

    // Existing S3 / AMY ROM Factory Patches (Planned for P4 standalone expansion)
    {  0, "JUNO BRASS SET 1","ROM JUNO", PatchCategory::Brass,         SynthFamily::Subtractive,   FeatureStatus::Planned,   true,
     {"CHAR", "BRTE", "MOTN", "SHAP", "ATK", "REL", "SPCE", "DRV"}},
    {  1, "JUNO BRASS SWELL","ROM JUNO", PatchCategory::Pads,          SynthFamily::Subtractive,   FeatureStatus::Planned,   false,
     {"CHAR", "BRTE", "MOTN", "SHAP", "ATK", "REL", "SPCE", "DRV"}},
    {  4, "MOVING STRINGS",  "ROM JUNO", PatchCategory::Strings,       SynthFamily::Subtractive,   FeatureStatus::Planned,   true,
     {"CHAR", "BRTE", "MOTN", "SHAP", "ATK", "REL", "SPCE", "DRV"}},
    {  8, "JUNO ORGAN I",    "ROM JUNO", PatchCategory::Organ,         SynthFamily::Subtractive,   FeatureStatus::Planned,   false,
     {"CHAR", "BRTE", "MOTN", "SHAP", "ATK", "REL", "SPCE", "DRV"}},
    { 12, "DONALD PLUCK",    "ROM JUNO", PatchCategory::Plucks,        SynthFamily::Subtractive,   FeatureStatus::Planned,   false,
     {"CHAR", "BRTE", "MOTN", "SHAP", "ATK", "REL", "SPCE", "DRV"}},
    { 14, "ELECT. PIANO I",  "ROM JUNO", PatchCategory::ElectricPiano, SynthFamily::Subtractive,   FeatureStatus::Planned,   true,
     {"CHAR", "BRTE", "MOTN", "SHAP", "ATK", "REL", "SPCE", "DRV"}},
    { 17, "STEEL DRUMS PCM", "ROM JUNO", PatchCategory::Percussion,    SynthFamily::PCM,           FeatureStatus::Planned,   false,
     {"CHAR", "BRTE", "MOTN", "SHAP", "ATK", "REL", "SPCE", "DRV"}},
    { 31, "SYNTH BASS I",    "ROM JUNO", PatchCategory::Bass,          SynthFamily::Subtractive,   FeatureStatus::Planned,   true,
     {"CHAR", "BRTE", "MOTN", "SHAP", "ATK", "REL", "SPCE", "DRV"}},
    { 32, "JUNO LEAD I",     "ROM JUNO", PatchCategory::Leads,         SynthFamily::Subtractive,   FeatureStatus::Planned,   false,
     {"CHAR", "BRTE", "MOTN", "SHAP", "ATK", "REL", "SPCE", "DRV"}},
    { 38, "THUD WAH FX",     "ROM JUNO", PatchCategory::FX,            SynthFamily::Subtractive,   FeatureStatus::Planned,   false,
     {"CHAR", "BRTE", "MOTN", "SHAP", "ATK", "REL", "SPCE", "DRV"}},
    {138, "DX7 E.PIANO 1",   "ROM DX7",  PatchCategory::ElectricPiano, SynthFamily::FM,            FeatureStatus::Planned,   true,
     {"CHAR", "BRTE", "MOTN", "FDBK", "RATIO", "DTUNE", "SPCE", "EDGE"}},
    {142, "DX7 BASS 1",      "ROM DX7",  PatchCategory::Bass,          SynthFamily::FM,            FeatureStatus::Planned,   true,
     {"CHAR", "BRTE", "MOTN", "FDBK", "RATIO", "DTUNE", "SPCE", "EDGE"}},

    // User Bank Demo Slots (Explicitly marked DEMO)
    {240, "USER STAGE PAD",  "USER DEMO",PatchCategory::Pads,          SynthFamily::Subtractive,   FeatureStatus::Demo,      false,
     {"CHAR", "BRTE", "MOTN", "SHAP", "ATK", "REL", "SPCE", "DRV"}},
    {241, "USER KARPLUS PLK","USER DEMO",PatchCategory::Plucks,        SynthFamily::KarplusStrong, FeatureStatus::Demo,      false,
     {"CHAR", "BRTE", "MOTN", "SHAP", "ATK", "REL", "SPCE", "DRV"}}
};

// Bitmask of active pitch classes relative to root (bit 0 = root)
uint16_t scaleIntervalMask(int32_t scale_idx) {
    switch (scale_idx) {
        case 0:  return 0x0FFF; // Chromatic: all 12 notes
        case 1:  return 0x0AB5; // Major: 0, 2, 4, 5, 7, 9, 11
        case 2:  return 0x05AD; // Natural Minor: 0, 2, 3, 5, 7, 8, 10
        case 3:  return 0x09AD; // Harmonic Minor: 0, 2, 3, 5, 7, 8, 11
        case 4:  return 0x02A5; // Pentatonic Major: 0, 2, 4, 7, 9
        case 5:  return 0x04A9; // Pentatonic Minor: 0, 3, 5, 7, 10
        case 6:  return 0x04E9; // Blues: 0, 3, 5, 6, 7, 10
        case 7:  return 0x06AD; // Dorian: 0, 2, 3, 5, 7, 9, 10
        case 8:  return 0x05AB; // Phrygian: 0, 1, 3, 5, 7, 8, 10
        case 9:  return 0x0AD5; // Lydian: 0, 2, 4, 6, 7, 9, 11
        case 10: return 0x06B5; // Mixolydian: 0, 2, 4, 5, 7, 9, 10
        default: return 0x0FFF;
    }
}

} // namespace

const char* synthFamilyName(SynthFamily family) {
    switch (family) {
        case SynthFamily::FM:            return "FM 6-OP";
        case SynthFamily::Subtractive:   return "ANALOG SUB";
        case SynthFamily::PCM:           return "PCM SAMPLE";
        case SynthFamily::KarplusStrong: return "KARPLUS";
        default:                         return "GENERIC";
    }
}

const char* patchCategoryName(PatchCategory cat) {
    switch (cat) {
        case PatchCategory::All:           return "ALL";
        case PatchCategory::Piano:         return "PIANO";
        case PatchCategory::ElectricPiano: return "E.PIANO";
        case PatchCategory::Organ:         return "ORGAN";
        case PatchCategory::Bass:          return "BASS";
        case PatchCategory::Brass:         return "BRASS";
        case PatchCategory::Strings:       return "STRINGS";
        case PatchCategory::Pads:          return "PADS";
        case PatchCategory::Leads:         return "LEADS";
        case PatchCategory::Plucks:        return "PLUCKS";
        case PatchCategory::Percussion:    return "PERC";
        case PatchCategory::FX:            return "FX";
        default:                           return "OTHER";
    }
}

UiModel::UiModel() {
    resetToDefaults();
}

void UiModel::resetToDefaults() {
    size_t count = 0;
    const ParameterDescriptor* descs = ParameterRegistry::all(count);
    for (size_t i = 0; i < count && i < kMaxParameters; ++i) {
        const auto& d = descs[i];
        switch (d.type) {
            case ParameterValueType::Float:
                values_[i] = ParameterValue::makeFloat(d.default_value);
                break;
            case ParameterValueType::Int:
                values_[i] = ParameterValue::makeInt(static_cast<int32_t>(d.default_value));
                break;
            case ParameterValueType::Bool:
                values_[i] = ParameterValue::makeBool(d.default_value != 0.0f);
                break;
            case ParameterValueType::Enum:
                values_[i] = ParameterValue::makeEnum(static_cast<int32_t>(d.default_value));
                break;
            case ParameterValueType::String:
                values_[i] = ParameterValue::makeString("");
                break;
        }
    }

    for (size_t i = 0; i < kCatalogSize; ++i) {
        favorites_[i] = kPatchCatalog[i].favorite_default;
    }

    active_edit_layer_ = 0;
    selected_seq_step_ = 0;

    // Seed a musical 16-step demo pattern
    static constexpr uint8_t kDemoNotes[16] = {
        60, 63, 67, 72, 60, 63, 67, 70,
        58, 62, 65, 70, 56, 60, 63, 67
    };
    for (size_t s = 0; s < kSeqSteps; ++s) {
        seq_steps_[s].active = (s % 2 == 0) || (s == 3) || (s == 11) || (s == 15);
        seq_steps_[s].note = kDemoNotes[s];
        seq_steps_[s].velocity = (s % 4 == 0) ? 115 : 92;
        seq_steps_[s].probability = (s == 15) ? 75 : 100;
        seq_steps_[s].ratchet = (s == 7) ? 2 : 1;
    }

    // Seed an 808-style demo drum pattern across 8 tracks
    std::memset(drum_grid_, 0, sizeof(drum_grid_));
    for (size_t s = 0; s < kSeqSteps; ++s) {
        drum_grid_[0][s] = (s == 0 || s == 6 || s == 8 || s == 10); // Kick
        drum_grid_[1][s] = (s == 4 || s == 12);                     // Snare
        drum_grid_[2][s] = (s % 2 == 0);                            // Closed Hat
        drum_grid_[3][s] = (s == 2 || s == 14);                     // Open Hat
        drum_grid_[6][s] = (s == 4 || s == 12 || s == 15);          // Clap
    }

    // Seed 8 modulation matrix slots
    mod_slots_[0] = {true,  1, 1,  40, 1, 0}; // Mod Wheel -> Osc Pitch (+40% Bipolar)
    mod_slots_[1] = {true,  2, 3,  55, 0, 0}; // Velocity  -> Filter Cutoff (+55% Uni)
    mod_slots_[2] = {true,  4, 3,  25, 1, 0}; // LFO 1     -> Filter Cutoff (+25% Bipolar)
    mod_slots_[3] = {true,  6, 8,  45, 0, 1}; // Env 2     -> FM Index (+45% Exp)
    mod_slots_[4] = {false, 7, 4,  30, 0, 0}; // Macro 1   -> Filter Res
    mod_slots_[5] = {false, 8, 9,  50, 0, 0}; // Macro 2   -> FX Send
    mod_slots_[6] = {false, 3, 5,  20, 0, 0}; // Aftertouch-> Amp Level
    mod_slots_[7] = {false, 9, 6,  35, 1, 0}; // Keytrack  -> Pan

    // Seed SMK25 V2 data-driven CC mappings
    static constexpr const char* kKnobNames[8] = {
        "SMK KNOB 1", "SMK KNOB 2", "SMK KNOB 3", "SMK KNOB 4",
        "SMK KNOB 5", "SMK KNOB 6", "SMK KNOB 7", "SMK KNOB 8"
    };
    static constexpr const char* kTargetIds[8] = {
        "perf.macro.1", "perf.macro.2", "perf.macro.3", "perf.macro.4",
        "filter.cutoff_hz", "filter.resonance", "fx.reverb.mix", "perf.master_volume"
    };
    for (size_t i = 0; i < kMidiMappings; ++i) {
        midi_mappings_[i].control_label = kKnobNames[i];
        midi_mappings_[i].channel = 1;
        midi_mappings_[i].cc_number = static_cast<uint8_t>(20 + i);
        midi_mappings_[i].target_param_id = kTargetIds[i];
        midi_mappings_[i].learning = false;
    }

    active_learn_slot_ = -1;
    last_preview_note_ = 60;
    std::snprintf(last_notice_, sizeof(last_notice_), "UI DEMO READY (RAM ONLY - NO AMY WRITES)");
    ++revision_;
}

bool UiModel::getValue(const char* id, ParameterValue& out_val) const {
    const int idx = ParameterRegistry::indexOf(id);
    if (idx < 0 || static_cast<size_t>(idx) >= kMaxParameters) return false;
    out_val = values_[idx];
    return true;
}

float UiModel::getFloat(const char* id, float fallback) const {
    ParameterValue pv{};
    if (!getValue(id, pv)) return fallback;
    return pv.asFloat();
}

int32_t UiModel::getInt(const char* id, int32_t fallback) const {
    ParameterValue pv{};
    if (!getValue(id, pv)) return fallback;
    return pv.asInt();
}

bool UiModel::getBool(const char* id, bool fallback) const {
    ParameterValue pv{};
    if (!getValue(id, pv)) return fallback;
    return pv.asBool();
}

bool UiModel::setValue(const char* id, const ParameterValue& val) {
    const int idx = ParameterRegistry::indexOf(id);
    if (idx < 0 || static_cast<size_t>(idx) >= kMaxParameters) return false;
    size_t count = 0;
    const ParameterDescriptor& desc = ParameterRegistry::all(count)[idx];

    // Reject writes to read-only parameters (e.g., hardware sample rate)
    if (desc.capabilities & kCapReadOnly) {
        return false;
    }

    // Type compatibility check
    if (val.type != desc.type) {
        return false;
    }

    ParameterValue clamped = val;
    switch (desc.type) {
        case ParameterValueType::Float:
            clamped.f_val = std::clamp(val.f_val, desc.minimum, desc.maximum);
            break;
        case ParameterValueType::Int:
            clamped.i_val = std::clamp(
                val.i_val,
                static_cast<int32_t>(desc.minimum),
                static_cast<int32_t>(desc.maximum));
            break;
        case ParameterValueType::Enum: {
            const int32_t max_enum = (desc.enum_count > 0)
                                         ? static_cast<int32_t>(desc.enum_count - 1)
                                         : static_cast<int32_t>(desc.maximum);
            if (val.enum_idx < 0 || val.enum_idx > max_enum) {
                return false;
            }
            clamped.enum_idx = val.enum_idx;
            break;
        }
        case ParameterValueType::Bool:
            clamped.b_val = val.b_val;
            break;
        case ParameterValueType::String:
            clamped.s_val = val.s_val ? val.s_val : "";
            break;
    }

    values_[idx] = clamped;
    ++revision_;
    return true;
}

bool UiModel::setFloat(const char* id, float val) {
    const int idx = ParameterRegistry::indexOf(id);
    if (idx < 0) return false;
    size_t count = 0;
    const ParameterDescriptor& desc = ParameterRegistry::all(count)[idx];
    if (desc.type == ParameterValueType::Float) {
        return setValue(id, ParameterValue::makeFloat(val));
    }
    if (desc.type == ParameterValueType::Int) {
        return setValue(id, ParameterValue::makeInt(static_cast<int32_t>(std::lround(val))));
    }
    if (desc.type == ParameterValueType::Enum) {
        return setValue(id, ParameterValue::makeEnum(static_cast<int32_t>(std::lround(val))));
    }
    if (desc.type == ParameterValueType::Bool) {
        return setValue(id, ParameterValue::makeBool(val >= 0.5f));
    }
    return false;
}

bool UiModel::setInt(const char* id, int32_t val) {
    const int idx = ParameterRegistry::indexOf(id);
    if (idx < 0) return false;
    size_t count = 0;
    const ParameterDescriptor& desc = ParameterRegistry::all(count)[idx];
    if (desc.type == ParameterValueType::Int) {
        return setValue(id, ParameterValue::makeInt(val));
    }
    if (desc.type == ParameterValueType::Enum) {
        return setValue(id, ParameterValue::makeEnum(val));
    }
    if (desc.type == ParameterValueType::Float) {
        return setValue(id, ParameterValue::makeFloat(static_cast<float>(val)));
    }
    if (desc.type == ParameterValueType::Bool) {
        return setValue(id, ParameterValue::makeBool(val != 0));
    }
    return false;
}

bool UiModel::setBool(const char* id, bool val) {
    return setValue(id, ParameterValue::makeBool(val));
}

FeatureStatus UiModel::effectiveStatus(const char* id) const {
    const ParameterDescriptor* desc = ParameterRegistry::findById(id);
    if (!desc) return FeatureStatus::Unavailable;

    // Check explicit parameter dependency
    if (desc->depends_on_id) {
        const int32_t dep_val = getInt(desc->depends_on_id, 0);
        if (desc->depends_on_value >= 0) {
            if (dep_val != desc->depends_on_value) {
                return FeatureStatus::Unavailable;
            }
        } else if (dep_val == 0) {
            return FeatureStatus::Unavailable;
        }
    }

    // Family-specific rules: Subtractive sub-osc/detune/generic ADSR vs FM 6-op
    const SynthFamily fam = activePatch().family;
    if (std::strcmp(desc->ns, "fm") == 0 && fam != SynthFamily::FM) {
        return FeatureStatus::Unavailable;
    }
    if ((std::strcmp(id, "osc.sub_level") == 0 || std::strcmp(id, "osc.1.unison") == 0) &&
        fam == SynthFamily::FM) {
        return FeatureStatus::Unavailable;
    }

    return desc->status;
}

bool UiModel::isParameterEnabled(const char* id) const {
    const ParameterDescriptor* desc = ParameterRegistry::findById(id);
    if (!desc) return false;
    if (desc->capabilities & kCapReadOnly) return false;
    return effectiveStatus(id) != FeatureStatus::Unavailable;
}

const BlueprintPatchEntry* UiModel::catalog(size_t& out_count) {
    out_count = kCatalogSize;
    return kPatchCatalog;
}

const BlueprintPatchEntry& UiModel::activePatch() const {
    const int32_t id = getInt("perf.patch_id", 128);
    for (size_t i = 0; i < kCatalogSize; ++i) {
        if (kPatchCatalog[i].id == static_cast<uint16_t>(id)) {
            return kPatchCatalog[i];
        }
    }
    return kPatchCatalog[0];
}

const BlueprintPatchEntry& UiModel::layerPatch(uint8_t layer_idx) const {
    const int32_t id = getInt(layer_idx == 0 ? "layer_a.patch_id" : "layer_b.patch_id",
                              layer_idx == 0 ? 128 : 135);
    for (size_t i = 0; i < kCatalogSize; ++i) {
        if (kPatchCatalog[i].id == static_cast<uint16_t>(id)) {
            return kPatchCatalog[i];
        }
    }
    return kPatchCatalog[layer_idx == 0 ? 0 : 1];
}

bool UiModel::selectPatchById(uint16_t patch_id, uint8_t target_layer) {
    const BlueprintPatchEntry* found = nullptr;
    for (size_t i = 0; i < kCatalogSize; ++i) {
        if (kPatchCatalog[i].id == patch_id) {
            found = &kPatchCatalog[i];
            break;
        }
    }
    if (!found) return false;

    if (target_layer == 0) {
        setInt("perf.patch_id", static_cast<int32_t>(patch_id));
        setInt("layer_a.patch_id", static_cast<int32_t>(patch_id));
        // Sync osc.1.wave to match patch synthesis family so editor tabs reflect family
        switch (found->family) {
            case SynthFamily::FM:            setInt("osc.1.wave", 8); break;
            case SynthFamily::Subtractive:   setInt("osc.1.wave", 2); break;
            case SynthFamily::PCM:           setInt("osc.1.wave", 7); break;
            case SynthFamily::KarplusStrong: setInt("osc.1.wave", 6); break;
        }
    } else {
        setInt("layer_b.patch_id", static_cast<int32_t>(patch_id));
    }

    char buf[64];
    std::snprintf(buf, sizeof(buf), "DEMO PATCH %03u: %s [%s]",
                  found->id, found->name, featureStatusLabel(found->status));
    setNotice(buf);
    return true;
}

bool UiModel::isFavorite(uint16_t patch_id) const {
    for (size_t i = 0; i < kCatalogSize; ++i) {
        if (kPatchCatalog[i].id == patch_id) return favorites_[i];
    }
    return false;
}

void UiModel::toggleFavorite(uint16_t patch_id) {
    for (size_t i = 0; i < kCatalogSize; ++i) {
        if (kPatchCatalog[i].id == patch_id) {
            favorites_[i] = !favorites_[i];
            ++revision_;
            return;
        }
    }
}

const StepCell& UiModel::seqStep(size_t step_idx) const {
    return seq_steps_[step_idx < kSeqSteps ? step_idx : 0];
}

void UiModel::toggleSeqStep(size_t step_idx) {
    if (step_idx >= kSeqSteps) return;
    seq_steps_[step_idx].active = !seq_steps_[step_idx].active;
    selected_seq_step_ = step_idx;
    setInt("seq.step_vel", seq_steps_[step_idx].velocity);
    setInt("seq.step_prob", seq_steps_[step_idx].probability);
    setInt("seq.step_ratchet", seq_steps_[step_idx].ratchet);
    ++revision_;
}

void UiModel::setSeqStepNote(size_t step_idx, uint8_t note, uint8_t velocity) {
    if (step_idx >= kSeqSteps) return;
    seq_steps_[step_idx].note = std::clamp<uint8_t>(note, 0, 127);
    seq_steps_[step_idx].velocity = std::clamp<uint8_t>(velocity, 1, 127);
    ++revision_;
}

void UiModel::setSelectedSeqStep(size_t step_idx) {
    if (step_idx >= kSeqSteps) return;
    selected_seq_step_ = step_idx;
    setInt("seq.step_vel", seq_steps_[step_idx].velocity);
    setInt("seq.step_prob", seq_steps_[step_idx].probability);
    setInt("seq.step_ratchet", seq_steps_[step_idx].ratchet);
    ++revision_;
}

bool UiModel::drumStep(size_t track_idx, size_t step_idx) const {
    if (track_idx >= kDrumTracks || step_idx >= kSeqSteps) return false;
    return drum_grid_[track_idx][step_idx];
}

void UiModel::toggleDrumStep(size_t track_idx, size_t step_idx) {
    if (track_idx >= kDrumTracks || step_idx >= kSeqSteps) return;
    drum_grid_[track_idx][step_idx] = !drum_grid_[track_idx][step_idx];
    ++revision_;
}

const ModSlot& UiModel::modSlot(size_t slot_idx) const {
    return mod_slots_[slot_idx < kModSlots ? slot_idx : 0];
}

void UiModel::setModSlot(size_t slot_idx, const ModSlot& slot) {
    if (slot_idx >= kModSlots) return;
    mod_slots_[slot_idx] = slot;
    if (slot_idx == 0) {
        setInt("mod.slot1.source", slot.source);
        setInt("mod.slot1.dest", slot.dest);
        setFloat("mod.slot1.amount", static_cast<float>(slot.amount));
        setInt("mod.slot1.polarity", slot.polarity);
    }
    ++revision_;
}

const MidiCcMapping& UiModel::midiMapping(size_t idx) const {
    return midi_mappings_[idx < kMidiMappings ? idx : 0];
}

void UiModel::setMidiMappingCc(size_t idx, uint8_t cc_num, uint8_t channel) {
    if (idx >= kMidiMappings) return;
    midi_mappings_[idx].cc_number = cc_num & 0x7F;
    midi_mappings_[idx].channel = std::clamp<uint8_t>(channel, 0, 16);
    midi_mappings_[idx].learning = false;
    if (active_learn_slot_ == static_cast<int>(idx)) {
        active_learn_slot_ = -1;
        setBool("midi.cc_learn_active", false);
    }
    ++revision_;
}

void UiModel::setMidiLearnSlot(int idx) {
    for (size_t i = 0; i < kMidiMappings; ++i) {
        midi_mappings_[i].learning = (static_cast<int>(i) == idx);
    }
    active_learn_slot_ = (idx >= 0 && static_cast<size_t>(idx) < kMidiMappings) ? idx : -1;
    setBool("midi.cc_learn_active", active_learn_slot_ >= 0);
    ++revision_;
}

HarmonyPreviewResult UiModel::computeHarmony(uint8_t raw_midi_note) const {
    HarmonyPreviewResult res{};
    res.is_demo_calculation = true;
    res.input_note = raw_midi_note & 0x7F;

    const int32_t root = std::clamp<int32_t>(getInt("harmony.root_note", 0), 0, 11);
    const int32_t scale_idx = std::clamp<int32_t>(getInt("harmony.scale_type", 2), 0, 10);
    const bool scale_on = getBool("harmony.scale_enable", true);
    const int32_t quant_mode = getInt("harmony.quant_mode", 0);

    const uint16_t rel_mask = scaleIntervalMask(scale_idx);
    // Rotate mask so bit p corresponds to absolute pitch class p (0=C..11=B)
    uint16_t abs_mask = 0;
    for (int pc = 0; pc < 12; ++pc) {
        const int rel = (pc + 12 - root) % 12;
        if (rel_mask & (1u << rel)) {
            abs_mask |= static_cast<uint16_t>(1u << pc);
        }
    }
    res.scale_mask_12 = abs_mask;

    const int in_pc = res.input_note % 12;
    res.input_in_scale = (abs_mask & (1u << in_pc)) != 0;

    int q_note = res.input_note;
    if (scale_on && !res.input_in_scale) {
        if (quant_mode == 2) {
            // Snap to root of current octave
            q_note = (res.input_note / 12) * 12 + root;
        } else {
            for (int dist = 1; dist <= 6; ++dist) {
                const int cand_up = std::clamp(static_cast<int>(res.input_note) + dist, 0, 127);
                const int cand_dn = std::clamp(static_cast<int>(res.input_note) - dist, 0, 127);
                if (quant_mode == 1) {
                    // Prefer downward
                    if (abs_mask & (1u << (cand_dn % 12))) { q_note = cand_dn; break; }
                    if (abs_mask & (1u << (cand_up % 12))) { q_note = cand_up; break; }
                } else {
                    // Prefer upward
                    if (abs_mask & (1u << (cand_up % 12))) { q_note = cand_up; break; }
                    if (abs_mask & (1u << (cand_dn % 12))) { q_note = cand_dn; break; }
                }
            }
        }
    }
    res.quantized_note = static_cast<uint8_t>(std::clamp(q_note, 0, 127));

    // Chord Assist intervals
    const bool chord_on = getBool("harmony.chord_enable", true);
    const int32_t chord_type = getInt("harmony.chord_type", 2);
    const int32_t inversion = std::clamp<int32_t>(getInt("harmony.inversion", 0), 0, 3);
    const int32_t spread = std::clamp<int32_t>(getInt("harmony.spread", 0), 0, 2);

    if (!chord_on || chord_type == 0) {
        res.chord_notes[0] = res.quantized_note;
        res.chord_count = 1;
        return res;
    }

    int intervals[5] = {0, 4, 7, 0, 0};
    uint8_t count = 3;
    switch (chord_type) {
        case 1:  intervals[1] = 4; intervals[2] = 7; count = 3; break; // Major
        case 2:  intervals[1] = 3; intervals[2] = 7; count = 3; break; // Minor
        case 3:  intervals[1] = 3; intervals[2] = 6; count = 3; break; // Diminished
        case 4:  intervals[1] = 4; intervals[2] = 8; count = 3; break; // Augmented
        case 5:  intervals[1] = 2; intervals[2] = 7; count = 3; break; // Sus2
        case 6:  intervals[1] = 5; intervals[2] = 7; count = 3; break; // Sus4
        case 7:  intervals[1] = 4; intervals[2] = 7; intervals[3] = 10; count = 4; break; // Dom 7
        case 8:  intervals[1] = 4; intervals[2] = 7; intervals[3] = 11; count = 4; break; // Maj 7
        case 9:  intervals[1] = 3; intervals[2] = 7; intervals[3] = 10; count = 4; break; // Min 7
        case 10: intervals[1] = 4; intervals[2] = 7; intervals[3] = 14; count = 4; break; // Add9
        case 11: intervals[1] = 4; intervals[2] = 7; intervals[3] = 10; intervals[4] = 14; count = 5; break; // 9th
        default: break;
    }

    // Apply inversion by raising lower chord tones by +12 semitones
    for (int inv = 0; inv < inversion && inv < static_cast<int>(count); ++inv) {
        intervals[inv] += 12;
    }
    // Apply spread to upper voices
    if (spread > 0 && count >= 3) {
        intervals[1] += 12 * spread;
    }
    std::sort(intervals, intervals + count);

    uint8_t out_idx = 0;
    for (uint8_t i = 0; i < count && out_idx < 6; ++i) {
        const int n = static_cast<int>(res.quantized_note) + intervals[i];
        if (n >= 0 && n <= 127) {
            res.chord_notes[out_idx++] = static_cast<uint8_t>(n);
        }
    }
    res.chord_count = (out_idx > 0) ? out_idx : 1;
    return res;
}

void UiModel::setNotice(const char* msg) {
    if (!msg) return;
    std::snprintf(last_notice_, sizeof(last_notice_), "%s", msg);
    ++revision_;
}

} // namespace smk::p4::ui
