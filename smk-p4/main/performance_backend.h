#pragma once
#include "smk_state.h"
#include "../components/amy_runtime/sound_edit.h"
#include "../components/amy_runtime/harmony.h"
#include "../components/amy_runtime/fx_state.h"
namespace smk::p4 {
// Shared presentation DTO; local state is owner-applied AMY state, never a
// simulated S3 session. No transport frames in the widget/backend contract.
using PerformanceState = link::State;
struct PatchInfo { uint16_t id=0; char name[25]{}; bool available=false; };

enum class PresetRejectReason : uint8_t {
    None = 0,
    EngineUnavailable,
    Busy,
    SaveInProgress,
    QuietBarrier,
    PendingEdit,
    ActiveNotes,
    SustainActive,
    CompareActive,
    UnsavedSynth,
    UnsavedFx,
    InvalidSlot,
    InvalidName,
    InvalidPreset,
    StorageUnavailable,
    StorageReadError,
    StorageWriteError,
    CommitUncertain,
    OwnerTimeout,
    StaleGeneration
};

enum class PresetOperationState : uint8_t {
    Idle = 0,
    Requested,
    WaitingForQuiet,
    Writing,
    Verifying,
    Confirmed,
    Rejected,
    Failed,
    OutcomeUnknown
};

struct PresetOperationStatus {
    PresetOperationState state = PresetOperationState::Idle;
    uint8_t action = 0; // 0=None, 2=Save, 3=Recall
    uint8_t slot = 0;
    const char* message = nullptr;
    uint32_t revision = 0;
};

class PerformanceBackend {
public:
    virtual ~PerformanceBackend() = default;
    virtual void service(uint32_t now_ms)=0;
    virtual bool connected() const=0;
    virtual bool busy() const=0;
    virtual const PerformanceState& state() const=0;
    virtual uint32_t stateRevision() const=0;
    virtual const char* status() const=0;
    virtual const char* takeNotice() { return nullptr; }
    virtual uint8_t voiceCapacity() const { return 4; }
    virtual const harmony::State* harmonyState() const { return nullptr; }
    virtual bool setHarmony(harmony::Config,uint32_t) { return false; }
    virtual const fx::FxState* fxState() const { return nullptr; }
    virtual bool setFxParameter(const char*, float, uint32_t) { return false; }
    virtual uint32_t fxToken() const { return 0; }
    virtual const edit::State* soundEdit() const { return nullptr; }
    virtual bool editError() const {return false;}
    virtual bool editPending() const {return false;}
    virtual uint32_t editGeneration() const { return 0; }
    virtual uint32_t editSnapshotGeneration() const { return 0; }
    virtual uint32_t editToken() const { return 0; }
    virtual bool requestStorageQuiet() { return false; }
    virtual bool storageQuiet() const { return false; }
    virtual void releaseStorageQuiet() {}
    virtual bool editParameter(unsigned,float) { return false; }
    virtual bool editAction(unsigned,unsigned=0,const char* =nullptr) { return false; }
    virtual bool userPreset(unsigned,char*,unsigned,uint16_t&) { return false; }
    virtual bool userPresetIdentity(unsigned slot,char* name,unsigned size,uint16_t& patch,uint32_t& sequence) {
        sequence=0;
        return userPreset(slot,name,size,patch);
    }
    virtual const char* editNotice() const { return "EDITOR UNAVAILABLE"; }
    virtual PresetRejectReason lastPresetRejectReason() const { return PresetRejectReason::None; }
    virtual const char* presetRejectNotice() const { return nullptr; }
    virtual PresetOperationStatus presetOperationStatus() const { return {}; }
    virtual bool local() const { return false; }
    virtual bool midiError() const { return false; }
    virtual bool patchError() const { return false; }
    virtual uint16_t patchCount() const=0;
    virtual uint16_t patchAt(uint16_t index) const=0;
    virtual bool patchInfo(uint16_t index,PatchInfo& info,uint32_t now_ms)=0;
    virtual bool setPatch(uint16_t id,uint32_t now_ms)=0;
    virtual bool setMacro(uint8_t index,uint8_t value,uint32_t now_ms)=0;
    virtual bool setMaster(uint8_t value,uint32_t now_ms)=0;
    virtual bool noteOn(uint8_t note,uint8_t velocity,uint32_t now_ms)=0;
    virtual void noteOff(uint8_t note)=0;
    virtual void heldNote(uint8_t note)=0;
    virtual void panic()=0;
};
}
