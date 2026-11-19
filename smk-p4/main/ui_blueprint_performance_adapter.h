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
    bool isBusy() const override { return patch_.active || backend_.busy(); }
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
    const arp::Status* arpStatus() const override { return backend_.arpStatus(); }
    const char* arpNotice() const override { return arp_notice_; }
    ui::UiCommandResult requestArpTransport(bool play) override;
    ui::UiCommandResult requestClearLatch() override;
    bool delayStatus(DelayStatus& out) const override;
    using SaveTempo = bool (*)(unsigned);
    void setTempoSaver(SaveTempo saver) { save_tempo_=saver; }
    bool canSaveTempo() const override { return save_tempo_!=nullptr; }
    ui::UiCommandResult requestSaveTempo() override;
    const char* tempoSaveNotice() const override { return tempo_save_notice_; }
    bool patchNeedsDiscardConfirmation() const override { return backend_.patchHasUnsavedEdits(); }
    ui::UiCommandResult confirmPatchDiscard(uint16_t, uint8_t) override;
    ui::UiCommandResult confirmPatchDiscard(uint16_t id, uint8_t layer, uint8_t preserve_mask) override;
    ui::UiCommandResult requestSelectPatch(uint16_t, uint8_t layer=0) override;
    ui::UiCommandResult requestSelectPatch(uint16_t id, uint8_t layer, uint8_t preserve_mask) override;
    bool isPatchLoading() const override { return patch_.active || backend_.busy(); }
    bool hasPatchError() const override { return backend_.patchError(); }
    ui::UiCommandResult patchLoadResult() const override { return patch_result_; }
    ui::UiCommandResult requestNoteOn(uint8_t, uint8_t) override;
    ui::UiCommandResult requestNoteOff(uint8_t, uint32_t token=0) override;
    void releaseAllUiNotes() override;
    ui::UiCommandResult requestPanic() override;
    void service(uint32_t now_ms) override;
    const ui::UiSignalMeters& meters() const override { return meters_; }
    const seq::Status* sequencerStatus() const override { return backend_.sequencerStatus(); }
    const seq::Pattern* sequencerPattern() const override { return backend_.sequencerPattern(); }
    ui::UiCommandResult sequencerResult() const override;
    ui::UiCommandResult requestSequencer(const seq::Request&) override;
    void setPatternStore(seq::PatternStore&);
    const seq::Slot* patternSlot(unsigned slot) const override {
        return pattern_store_ && slot<seq::kPatternSlots?&pattern_slots_[slot]:nullptr;
    }
    const char* patternNotice() const override { return pattern_notice_; }
    bool patternBusy() const override { return pattern_phase_!=PatternPhase::Idle; }
    void requestPattern(const ui::PatternOperation&) override;
    void setSceneStore(scene::Store&);
    const scene::Slot* sceneSlot(unsigned slot) const override {
        return scene_store_ && slot<scene::kSlots?&scene_slots_[slot]:nullptr;
    }
    ui::SceneView sceneView() const override { return scene_view_; }
    uint8_t sceneCaptureDomains() const override { return scene_current_.domains; }
    scene::Reject sceneCompatibility(const ui::SceneOperation&) const override;
    void requestScene(const ui::SceneOperation&) override;
    uint8_t activeSequencerStep() const override {
        const auto* status=sequencerStatus();
        return status?status->current_step:seq::kNoStep;
    }
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
    MidiShortcut takeMidiShortcut() override;
    const char* midiProfileName() const override;
    void midiObservationText(char*, unsigned) const override;
    void midiLastBindingText(char*, unsigned) const override;
    void midiPadText(unsigned, char*, unsigned) const override;
    void midiPerformanceAction(MidiAction action,unsigned knob) override;
    const char* midiBankName() const override;
    const char* midiLearnStatus() const override { return midi_status_; }
    void midiKnobText(unsigned knob,char* text,unsigned size) const override;

private:
    SaveTempo save_tempo_=nullptr;
    const char* tempo_save_notice_="";
    uint32_t tempo_save_started_ms_=0;
    uint16_t tempo_save_bpm_=0;
    ui::UiCommandResult submitArp(arp::Action,uint8_t,const char*);
    void serviceArp();
    void serviceSequencer();
    void servicePattern();
    void captureScene(scene::Configuration&) const;
    void serviceScenes();
    void finishSceneWrite(const char*);
    scene::Reject prepareSceneRecall(const ui::SceneOperation&,scene::RecallRequest&) const;
    void finishPattern(const char*);
    bool submitPattern(const seq::Request&);
    enum class PatternPhase { Idle, Quiet, ReadyToWrite, Resume, Ack, Boundary };
    PatternPhase pattern_phase_=PatternPhase::Idle;
    seq::PatternStore* pattern_store_=nullptr;
    seq::Slot pattern_slots_[seq::kPatternSlots]{};
    seq::Slot pattern_write_;
    ui::PatternOperation pattern_op_;
    const char* pattern_notice_="";
    uint32_t pattern_started_=0, pattern_token_=0, pattern_generation_=0;
    uint8_t pattern_write_slot_=0;
    bool pattern_follow_load_=false,pattern_wait_boundary_=false;
    scene::Store* scene_store_=nullptr;
    scene::Slot scene_slots_[scene::kSlots]{}, scene_write_;
    scene::Configuration scene_baseline_, scene_current_;
    ui::SceneView scene_view_;
    uint32_t scene_token_=0,scene_write_started_=0,scene_reference_sequence_=0;
    uint8_t scene_pending_slot_=seq::kNoSlot,scene_write_slot_=seq::kNoSlot;
    bool scene_write_ready_=false,scene_write_capture_=false;
    bool replace_scene_=false;
    ui::SceneOperation replacement_scene_;
    struct SequencerRequest {
        uint32_t token=0, generation=0, requested_ms=0;
    };
    SequencerRequest seq_command_, seq_stop_;
    std::array<seq::Request,seq::kQueueCapacity> seq_deferred_{};
    unsigned seq_deferred_count_=0;
    uint32_t seq_deferred_generation_=0;
    ui::UiCommandResult seq_result_{ui::UiCommandState::Confirmed,0,nullptr};
    uint32_t arp_token_=0,arp_generation_=0,arp_requested_ms_=0,arp_usb_token_=0;
    const char* arp_id_="arp.enable";
    const char* arp_notice_=nullptr;
    void serviceMidi();
    const char* midiDestination(unsigned knob) const;
    bool midiTarget(unsigned knob,uint8_t& target) const;
    unsigned firstDelayDivision() const;
    unsigned midi_delay_range_=0;
    void resetPickup(const char* id=nullptr);
    struct MidiKnob {
        performance::MidiSource source{0,16,0};
        performance::Pickup pickup;
        uint8_t physical=0;
        bool observed=false;
    };
    amy::UsbInput* midi_input_=nullptr;
    MidiKnob midi_knobs_[16]{};
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
    std::array<uint8_t,8> midi_physical_b_destinations_{};
    bool midi_legacy_=false, midi_view_b_=false, midi_restore_pending_=false;
    uint32_t midi_control_drops_=0,midi_usb_generation_=0;
    char midi_knob_bank_='?', midi_pad_bank_='?';
    uint16_t midi_pads_held_=0;
    uint32_t midi_save_pad_ms_=0;
    bool midi_save_pad_armed_=false;
    uint8_t midi_pad_velocity_[16]{};
    MidiShortcut midi_shortcut_=MidiShortcut::None;
    amy::MidiControlEvent midi_last_input_{};
    bool midi_has_last_input_=false;
    unsigned midiKnobCount() const { return midi_legacy_ ? 16 : 8; }
    void applyMidiProfile(const performance::ControllerProfile&);
    void resetMidiGesture();
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
    uint32_t patch_failure_revision_=0;
    ui::UiCommandResult patch_result_{};
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
    bool edit_meta_valid_=false;
    uint32_t last_edit_rev_=0, last_edit_epoch_=0;
    uint64_t last_edit_avail_=0;
    uint16_t last_edit_patch_=0;
    bool last_edit_compare_=false, last_edit_dirty_=false;
    int8_t last_edit_user_slot_=-1;
    bool fx_meta_valid_=false;
    uint32_t last_fx_rev_=0;
    float last_fx_dsp_ms_=0.0f, last_fx_target_ms_=0.0f;
    bool last_fx_oor_=false, last_fx_bypass_=false;
    uint32_t last_preset_op_rev_=0;
    bool harmony_meta_valid_=false;
    uint32_t last_harmony_pack_=0;
    harmony::State last_harmony_state_{};
    bool last_usable_=false;
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
