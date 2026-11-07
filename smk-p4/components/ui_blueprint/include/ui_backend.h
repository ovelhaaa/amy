#pragma once
#include "ui_model.h"
#include <cstdint>

namespace smk::p4::ui {

// Outcome of a musical or parameter command requested by the UI layer.
// Distinguishes between owner-confirmed engine changes, queued/pending requests,
// demonstrative RAM-only simulation, planned/unsupported features, and errors.
enum class UiCommandState : uint8_t {
    Confirmed   = 0, // Applied and confirmed by the underlying musical engine
    Requested   = 1, // Queued/sent to the engine; awaiting snapshot confirmation
    DemoApplied = 2, // Applied only in UI demo RAM model (no real AMY DSP write)
    Unsupported = 3, // Feature is PLANNED or UNAVAILABLE in current backend
    Rejected    = 4  // Backend busy, disconnected, out-of-range, or error
};

const char* commandStateLabel(UiCommandState state);

struct UiCommandResult {
    UiCommandState state  = UiCommandState::Rejected;
    uint32_t       token  = 0;       // Non-zero note/request token when applicable
    const char*    notice = nullptr; // Human-readable status feedback

    bool accepted() const {
        return state == UiCommandState::Confirmed ||
               state == UiCommandState::Requested ||
               state == UiCommandState::DemoApplied;
    }
};

// Presentation density level (Section 7):
// - StageMinimal: Used on Performance Stage (fewer technical badges, larger controls)
// - BlueprintVerbose: Used on Editor/System/Diagnostics screens (full AVAIL/PLAN/N/A/DEMO badges)
enum class PresentationLevel : uint8_t {
    StageMinimal     = 0,
    BlueprintVerbose = 1
};

// Simulated level meters (explicitly tagged when simulated in demo mode)
struct UiSignalMeters {
    bool    is_simulated = true;
    uint8_t layer_a_l    = 0;
    uint8_t layer_a_r    = 0;
    uint8_t layer_b_l    = 0;
    uint8_t layer_b_r    = 0;
    uint8_t drums_l      = 0;
    uint8_t drums_r      = 0;
    uint8_t master_l     = 0;
    uint8_t master_r     = 0;
};

// Generic presentation/backend contract for the SMK-P4 800x480 LVGL UI Blueprint.
// Decouples all 14 screens and 11 widgets from concrete UiDemoBackend or PerformanceBackend.
// Never imports AMY, USB Host, or PCM5102A headers into the UI component.
class IUiBackend {
public:
    virtual ~IUiBackend() = default;

    // 1. Model & Snapshot access
    virtual UiModel& model() = 0;
    virtual const UiModel& model() const = 0;

    // 2. Backend identity & live status
    virtual bool isDemoMode() const = 0;
    virtual bool isConnected() const = 0;
    virtual bool isBusy() const = 0;
    virtual bool hasMidiError() const { return false; }
    virtual const char* backendName() const = 0;
    virtual const char* modeBadgeText() const = 0;
    virtual const char* statusText() const = 0;

    // 3. Resource availability & command state tracking
    virtual FeatureStatus parameterStatus(const char* id) const = 0;
    virtual bool isParameterValueSupported(const char*,int32_t) const { return true; }
    virtual bool isParameterEditable(const char* id) const = 0;
    virtual bool isPatchSelectable(uint16_t patch_id) const = 0;
    virtual FeatureStatus patchStatus(uint16_t id) const {
        size_t count=0; const auto* entries=UiModel::catalog(count);
        for(size_t i=0;i<count;++i) if(entries[i].id==id) return entries[i].status;
        return FeatureStatus::Unavailable;
    }
    virtual UiCommandState lastCommandState() const = 0;
    virtual const char* lastCommandParameterId() const = 0;

    // 4. Musical & parameter commands (never assume slider move == hardware applied)
    virtual UiCommandResult requestParameterValue(const char* id, const ParameterValue& val) = 0;
    virtual UiCommandResult requestParameterFloat(const char* id, float val) = 0;
    virtual UiCommandResult requestParameterInt(const char* id, int32_t val) = 0;
    virtual UiCommandResult requestParameterBool(const char* id, bool val) = 0;

    virtual UiCommandResult requestSelectPatch(uint16_t patch_id, uint8_t target_layer = 0) = 0;

    virtual bool hasSoundEditor() const { return false; }
    virtual const char* soundEditName() const {return model().activePatch().name;}
    virtual const char* soundEditStatus() const { return "EDITOR PLANNED"; }
    virtual UiCommandResult requestSoundAction(unsigned,unsigned=0,const char* =nullptr) { return {}; }
    virtual bool userPreset(unsigned,char*,unsigned,uint16_t&) { return false; }

    // 5. Note lifecycle with explicit token/pitch pairing (prevents stuck notes on octave shift)
    virtual UiCommandResult requestNoteOn(uint8_t note, uint8_t velocity) = 0;
    virtual UiCommandResult requestNoteOff(uint8_t note, uint32_t token = 0) = 0;
    virtual void releaseAllUiNotes() = 0;
    virtual UiCommandResult requestPanic() = 0;

    // 6. Periodic service & indicator updates
    virtual void service(uint32_t now_ms) = 0;
    virtual const UiSignalMeters& meters() const = 0;
    virtual uint8_t activeSequencerStep() const = 0;
    virtual bool isNoteHeld(uint8_t note) const = 0;
};

} // namespace smk::p4::ui
