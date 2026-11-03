#pragma once
#include "ui_backend.h"
#include <cstdint>

namespace smk::p4::ui {

// Purely in-memory demonstrative backend for the SMK-P4 UI Blueprint 1.1.
// Implements the generic IUiBackend contract without touching AMY, I2S, USB-MIDI, NVS, or UART.
class UiDemoBackend final : public IUiBackend {
public:
    using SimulatedMeters = UiSignalMeters;

    struct NoteEventRecord {
        bool     is_note_on = false;
        uint8_t  note       = 0;
        uint8_t  velocity   = 0;
        uint32_t token      = 0;
    };

    static constexpr size_t kEventLogCapacity = 64;

    UiDemoBackend();

    UiModel& model() override { return model_; }
    const UiModel& model() const override { return model_; }

    void service(uint32_t now_ms) override;

    // Backend identity & status (IUiBackend)
    bool isDemoMode() const override { return true; }
    bool isConnected() const override { return true; }
    bool isBusy() const override { return false; }
    const char* backendName() const override { return "UiDemoBackend (RAM-Only Simulation)"; }
    const char* modeBadgeText() const override { return "UI DEMO"; }
    const char* statusText() const override { return model_.lastNotice(); }

    // Availability & command state (IUiBackend)
    FeatureStatus parameterStatus(const char* id) const override {
        return model_.effectiveStatus(id);
    }
    bool isParameterEditable(const char* id) const override {
        return model_.isParameterEnabled(id);
    }
    bool isPatchSelectable(uint16_t patch_id) const override;
    UiCommandState lastCommandState() const override { return last_cmd_state_; }
    const char* lastCommandParameterId() const override { return last_cmd_param_; }

    // Musical & parameter requests (IUiBackend)
    UiCommandResult requestParameterValue(const char* id, const ParameterValue& val) override;
    UiCommandResult requestParameterFloat(const char* id, float val) override;
    UiCommandResult requestParameterInt(const char* id, int32_t val) override;
    UiCommandResult requestParameterBool(const char* id, bool val) override;
    UiCommandResult requestSelectPatch(uint16_t patch_id, uint8_t target_layer = 0) override;

    UiCommandResult requestNoteOn(uint8_t note, uint8_t velocity) override;
    UiCommandResult requestNoteOff(uint8_t note, uint32_t token = 0) override;
    void releaseAllUiNotes() override;
    UiCommandResult requestPanic() override;

    // Legacy convenience wrappers (preserved for backwards compatibility)
    void triggerNoteOn(uint8_t note, uint8_t velocity) { (void)requestNoteOn(note, velocity); }
    void triggerNoteOff(uint8_t note) { (void)requestNoteOff(note, 0); }
    void triggerPanic() { (void)requestPanic(); }

    const UiSignalMeters& meters() const override { return meters_; }
    uint8_t activeSequencerStep() const override { return active_demo_step_; }
    uint8_t activeDemoStep() const { return active_demo_step_; }
    bool isNoteHeld(uint8_t note) const override { return (note < 128) ? (held_tokens_[note] != 0) : false; }
    uint32_t heldNoteToken(uint8_t note) const { return (note < 128) ? held_tokens_[note] : 0; }

    // Event telemetry for host verification tests (Section 3 & 11)
    uint32_t noteOnCount() const { return note_on_count_; }
    uint32_t noteOffCount() const { return note_off_count_; }
    size_t eventLogCount() const { return event_log_count_; }
    const NoteEventRecord& eventLogAt(size_t idx) const {
        return event_log_[idx < kEventLogCapacity ? idx : 0];
    }
    void clearEventLog() {
        note_on_count_ = 0;
        note_off_count_ = 0;
        event_log_count_ = 0;
    }

private:
    void recomputeMeters();
    void updateActiveVoiceCount();
    void recordNoteEvent(bool is_on, uint8_t note, uint8_t velocity, uint32_t token);

    UiModel          model_{};
    UiSignalMeters   meters_{};
    uint32_t         held_tokens_[128]{};
    uint32_t         next_token_       = 1;
    uint8_t          active_demo_step_ = 0;
    uint32_t         last_step_ms_     = 0;
    UiCommandState   last_cmd_state_   = UiCommandState::DemoApplied;
    const char*      last_cmd_param_   = "";
    uint32_t         note_on_count_    = 0;
    uint32_t         note_off_count_   = 0;
    NoteEventRecord  event_log_[kEventLogCapacity]{};
    size_t           event_log_count_  = 0;
};

} // namespace smk::p4::ui
