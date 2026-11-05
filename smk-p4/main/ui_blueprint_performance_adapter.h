#pragma once
#include "performance_backend.h"
#include "ui_backend.h"

namespace smk::p4 {
// Single UI-task owner. Fixed gesture ledger; tokens never alias backend tokens.
class UiBlueprintPerformanceAdapter final : public ui::IUiBackend {
public:
    static constexpr unsigned kGestureCapacity = 32;
    static constexpr uint32_t kTouchLeaseMs = 10000;
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
    bool isParameterEditable(const char* id) const override;
    bool isPatchSelectable(uint16_t id) const override;
    ui::FeatureStatus patchStatus(uint16_t id) const override;
    ui::UiCommandState lastCommandState() const override { return last_state_; }
    const char* lastCommandParameterId() const override { return last_id_; }
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
private:
    struct Gesture { uint32_t token=0, start_ms=0; uint8_t note=0; };
    struct Pending { bool active=false; uint16_t value=0; uint32_t revision=0, requested_ms=0; };
    ui::UiCommandResult result(ui::UiCommandState, const char*, const char*, uint32_t token=0);
    bool usable() const { return backend_.connected() && !backend_.busy() && have_snapshot_; }
    PerformanceBackend& backend_;
    ui::UiModel model_;
    ui::UiSignalMeters meters_{};
    Gesture gestures_[kGestureCapacity]{};
    Pending patch_, master_;
    uint32_t next_token_=0, now_ms_=0, revision_=0;
    bool have_snapshot_=false, was_connected_=false;
    ui::UiCommandState last_state_=ui::UiCommandState::Rejected;
    const char* last_id_="boot";
};
}
