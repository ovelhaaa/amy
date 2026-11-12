#pragma once
#include "performance_backend.h"
#include "ui_backend.h"
#include "performance_midi.h"
#include "usb_input.h"
#include "controller_profile.h"

namespace smk::p4 {
// Single UI-task owner. Fixed gesture ledger; tokens never alias backend tokens.
class UiBlueprintPerformanceAdapter final : public ui::IUiBackend {
public:
    static constexpr unsigned kGestureCapacity = 32;
    static constexpr uint32_t kTouchLeaseMs = 10000;
    static constexpr uint32_t kPatchConfirmationTimeoutMs = 2000;
    explicit UiBlueprintPerformanceAdapter(PerformanceBackend& backend);
    ui::UiModel& model() override { return model_; }
    const ui::UiModel& model() const override { return model_; }
    bool isDemoMode() const override { return false; }
    bool isConnected() const override { return backend_.connected(); }
    bool isBusy() const override { return backend_.busy(); }
    bool hasMidiError() const override { return backend_.midiError(); }
    const char* backendName() const override { return "LOCAL AMY / BLUEPRINT 1.1"; }
    const char* modeBadgeText() const override { return "P4 LIVE"; }
    const char* statusText() const override { return backend_.status(); }
    ui::FeatureStatus parameterStatus(const char* id) const override;
    bool isParameterValueSupported(const char*,int32_t) const override;
    bool isParameterEditable(const char* id) const override;
    bool isPatchSelectable(uint16_t id) const override;
    ui::FeatureStatus patchStatus(uint16_t id) const override;
    ui::UiCommandState lastCommandState() const override { return last_state_; }
    const char* lastCommandParameterId() const override { return last_id_; }
    ui::UiCommandResult fmAlgorithmResult() const override { return fm_algorithm_result_; }
    uint32_t lastCommandToken() const override { return last_token_; }
    ui::UiCommandResult requestParameterValue(const char*, const ui::ParameterValue&) override;
    ui::UiCommandResult requestParameterFloat(const char*, float) override;
    ui::UiCommandResult requestParameterInt(const char*, int32_t) override;
    ui::UiCommandResult requestParameterBool(const char*, bool) override;
    ui::UiCommandResult requestSelectPatch(uint16_t, uint8_t layer=0) override;
    ui::UiCommandResult requestNoteOn(uint8_t, uint8_t) override;
    ui::UiCommandResult requestNoteOff(uint8_t, uint32_t token=0) override;
    void releaseAllUiNotes() override;
    ui::UiCommandResult requestPanic() override;
    void service(uint32_t now_ms) override;
    const ui::UiSignalMeters& meters() const override { return meters_; }
    uint8_t activeSequencerStep() const override { return 0; }
    bool isNoteHeld(uint8_t note) const override;
    bool routingValues(unsigned, RoutingValues&) const override;
    bool routingSnapshot(RoutingSnapshot&) const override;
    RoutingDestination routingDestination(unsigned) const override;
    performance::Route factoryRoute(unsigned, unsigned) const override;
    ui::UiCommandResult requestRouting(const RoutingSnapshot&, const performance::Routes&, uint8_t) override;
    ui::UiCommandResult routingResult() const override { return routing_result_; }
    bool legacyOffsets(LegacyOffsets& offsets) const override;
    ui::UiCommandResult resetLegacyOffsets(const LegacyOffsets& expected) override;
    bool hasSoundEditor() const override { return backend_.soundEdit()!=nullptr; }
    const char* soundEditName() const override {return backend_.state().patch_name;}
    const char* soundEditStatus() const override { return backend_.editNotice(); }
    ui::UiCommandResult requestSoundAction(unsigned action,unsigned slot=0,const char* name=nullptr) override;
    bool userPresetIdentity(unsigned slot,char* name,unsigned size,uint16_t& patch,uint32_t& sequence) override {
        return backend_.userPresetIdentity(slot,name,size,patch,sequence);
    }
    bool userPreset(unsigned slot,char* name,unsigned capacity,uint16_t& origin) override {return backend_.userPreset(slot,name,capacity,origin);}
    ui::PresetOperationStatus presetOperationStatus() const override;
    void setMidiInput(amy::UsbInput& input) { midi_input_=&input; }
    void setControllerProfileStore(performance::ControllerProfileStore& store);
    const char* midiTargetName(unsigned) const override;
    unsigned midiCustomTarget(unsigned) const override;
    bool setMidiCustomTarget(unsigned, unsigned) override;
    bool hasMidiPerformance() const override { return midi_input_ && hasSoundEditor(); }
    void midiPerformanceAction(MidiAction action,unsigned knob) override;
    const char* midiBankName() const override;
    const char* midiLearnStatus() const override { return midi_status_; }
    void midiKnobText(unsigned knob,char* text,unsigned size) const override;

private:
    void serviceMidi();
    const char* midiDestination(unsigned knob) const;
    bool midiTarget(unsigned knob,uint8_t& target) const;
    void resetPickup(const char* id=nullptr);
    struct MidiKnob {
        performance::MidiSource source{0,16,0};
        performance::Pickup pickup;
        uint8_t physical=0;
        bool observed=false;
    };
    amy::UsbInput* midi_input_=nullptr;
    MidiKnob midi_knobs_[8]{};
    performance::MidiLearn midi_learn_;
    uint32_t midi_device_generation_=0,midi_owner_generation_=0,midi_edit_epoch_=0;
    bool midi_compare_=false;
    bool midi_dispatch_=false;
    performance::ControllerProfileStore* midi_store_=nullptr;
    performance::ControllerProfile midi_saved_profile_;
    uint32_t midi_save_started_ms_=0;
    bool midi_save_pending_=false;
    unsigned midi_bank_=0,midi_learn_knob_=0;
    performance::MidiDestinations midi_destinations_{};
    char midi_status_[72]="SELECT A KNOB TO LEARN";
    struct Gesture { uint32_t token=0, start_ms=0; uint8_t note=0; };
    struct Pending { bool active=false; uint16_t value=0; uint32_t revision=0, requested_ms=0; };
    ui::UiCommandResult result(ui::UiCommandState, const char*, const char*, uint32_t token=0);
    bool usable() const { return backend_.connected() && !backend_.busy() && have_snapshot_; }
    PerformanceBackend& backend_;
    ui::UiModel model_;
    ui::UiSignalMeters meters_{};
    Gesture gestures_[kGestureCapacity]{};
    Pending patch_, master_;
    harmony::Config requested_harmony_;
    bool harmony_pending_=false;
    const char* harmony_id_="harmony.preset";
    int32_t harmony_preset_=0;
    uint32_t harmony_requested_ms_=0,harmony_revision_=0;
    void syncHarmony(harmony::Config config);
    int editIndex(const char* id) const;
    unsigned selected_op_=0;
    bool edit_requested_=false;
    uint32_t edit_errors_=0;
    const char* edit_id_="editor";
    bool fx_requested_=false;
    const char* fx_id_="fx";
    uint32_t fx_requested_ms_=0;
    uint32_t fx_pending_token_=0;
    uint32_t fx_initial_errors_=0;
    uint32_t fx_initial_rejected_=0;
    struct PendingEdit {
        bool active=false;
        unsigned param=0;
        float requested_value=0.0f;
        uint32_t token=0;
        uint16_t patch_id=0;
        uint32_t requested_ms=0;
        uint32_t initial_errors=0;
        uint32_t generation=0;
    } pending_edit_;
    ui::UiCommandResult fm_algorithm_result_{};
    ui::UiCommandResult routing_result_{};
    uint32_t routing_epoch_=0, routing_generation_=0, routing_errors_=0;
    uint32_t last_token_=0;
    uint32_t next_token_=0, now_ms_=0, revision_=0;
    bool have_snapshot_=false, was_connected_=false;
    ui::UiCommandState last_state_=ui::UiCommandState::Rejected;
    const char* last_id_="boot";
};
}
