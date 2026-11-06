#pragma once
#include "performance_backend.h"
#include "amy_runtime.h"
#include <cstdio>
namespace smk::p4 {
// UI task only. publish() receives a copied single-element owner mailbox.
class LocalAmyPerformanceBackend final : public PerformanceBackend {
public:
    explicit LocalAmyPerformanceBackend(amy::Commands& commands):commands_(commands) {}
    void publish(const amy::Snapshot& snapshot,bool output_live,uint32_t max_render_us,uint32_t deadlines) {
#ifdef SMKP4_UNIFIED_ENGINE
        if(snapshot.patch_changes!=changes_ || snapshot.patch_failures!=failures_) pending_=false;
        changes_=snapshot.patch_changes;failures_=snapshot.patch_failures;
        loading_=snapshot.patch_state==amy::PatchState::Requested || snapshot.patch_state==amy::PatchState::Loading || snapshot.patch_state==amy::PatchState::Verifying;
        failed_=snapshot.patch_state==amy::PatchState::Failed;
#endif
        #ifdef SMKP4_HARMONY
        harmony_.configuration=snapshot.harmony;harmony_.revision=snapshot.harmony_revision;
        harmony_.errors=snapshot.harmony_errors;harmony_.rejected=snapshot.groups_rejected;
        harmony_.admitted=snapshot.groups_admitted;harmony_.identity=snapshot.last_group;
        harmony_.result=snapshot.last_result;harmony_.count=snapshot.last_count;harmony_.input=snapshot.last_input;harmony_.last_admitted=snapshot.last_admitted;
        for(unsigned i=0;i<4;++i)harmony_.notes[i]=snapshot.last_notes[i];
        harmony_.voices=snapshot.active_notes;harmony_.capacity=amy::kVoices;harmony_.available=true;
#endif
        ready_=snapshot.ready; state_.patch_id=state_.engine_patch=snapshot.patch_id;
        const int index=amy::patchIndex(snapshot.patch_id);
        std::snprintf(state_.patch_name,sizeof(state_.patch_name),"%s",index<0?"ERROR":amy::kPatches[index].name);
        state_.master=snapshot.master; state_.active_notes=snapshot.active_notes;
        state_.sample_rate_hz=amy::kSampleRateHz; state_.block_frames=amy::kBlockFrames;
        state_.flags=(output_live?link::kAudioLive:0)|(midi_connected_?link::kUsbConnected:0);
        state_.max_render_us=max_render_us; state_.deadline_misses=deadlines;
        state_.commands_dropped=commands_.overflows();
        const uint32_t generation=commands_.generation();
        if(generation_!=generation) { generation_=generation; clearTokens(); }
        ++revision_;
    }
    void service(uint32_t) override {}
    void publishMidi(bool connected,bool error,uint32_t events) {
        midi_connected_=connected; midi_error_=error; state_.midi_events=events;
        if(connected) state_.flags|=link::kUsbConnected;
        else state_.flags&=~link::kUsbConnected;
    }
    const harmony::State* harmonyState() const override {
#ifdef SMKP4_HARMONY
        return &harmony_;
#else
        return nullptr;
#endif
    }
    bool setHarmony(harmony::Config config,uint32_t now_ms) override {
#ifdef SMKP4_HARMONY
        if(!ready_ || busy() || failed_ || !harmony::valid(config))return false;
        return commands_.push({amy::CommandType::Harmony,0,0,0,harmony::pack(config),now_ms});
#else
        (void)config;(void)now_ms;return false;
#endif
    }
    bool midiError() const override { return midi_error_; }
    bool patchError() const override { return failed_; }
    bool connected() const override { return ready_; }
    bool busy() const override { return pending_ || loading_; }
    uint8_t voiceCapacity() const override { return amy::kVoices; }
    bool local() const override { return true; }
    const PerformanceState& state() const override { return state_; }
    uint32_t stateRevision() const override { return revision_; }
    const char* status() const override { return !ready_?"AMY INIT/ERROR":failed_?"PATCH LOAD FAILED; RETRY":busy()?"PATCH LOADING":(state_.flags&link::kAudioLive)?"LOCAL AMY / PCM5102A":"LOCAL AMY / I2S DISABLED"; }
    uint16_t patchCount() const override { return amy::kPatchCount; }
    uint16_t patchAt(uint16_t index) const override { return index<amy::kPatchCount?amy::kPatches[index].id:UINT16_MAX; }
    bool patchInfo(uint16_t index,PatchInfo& info,uint32_t) override {
        if(index>=amy::kPatchCount) return false;
        info.id=patchAt(index); info.available=ready_;
        std::snprintf(info.name,sizeof(info.name),"%s",amy::kPatches[index].name); return true;
    }
    bool setPatch(uint16_t id,uint32_t now_ms) override {
        if(!ready_ || busy() || amy::patchIndex(id)<0) return false;
        panic(); const bool accepted=commands_.push({amy::CommandType::Patch,id,0,0,0,now_ms});
#ifdef SMKP4_UNIFIED_ENGINE
        pending_=accepted;
#endif
        return accepted;
    }
    bool setMacro(uint8_t,uint8_t,uint32_t) override { return false; }
    bool setMaster(uint8_t value,uint32_t now_ms) override {
        return ready_ && value<=127 && commands_.push({amy::CommandType::Master,0,value,0,0,now_ms});
    }
    bool noteOn(uint8_t note,uint8_t velocity,uint32_t now_ms) override {
        if(!ready_ || busy() || failed_ || note>=128 || velocity>127) return false;
        if(!velocity) { noteOff(note); return true; }
        if(tokens_[note]) return true;
        // Token zero is reserved. Exhaustion fails closed rather than reusing
        // an old generation. At touch rates this is decades of operation.
        const uint32_t token=commands_.nextToken();
        if(!token) { panic(); return false; }
        if(!commands_.push({amy::CommandType::NoteOn,note,velocity,0,token,now_ms})) return false;
        tokens_[note]=token; return true;
    }
    void noteOff(uint8_t note) override {
        if(note<128 && tokens_[note]) { commands_.release(note,tokens_[note]); tokens_[note]=0; }
    }
    void heldNote(uint8_t) override {} // Owner enforces a fixed independent lease.
    void panic() override { commands_.panic(); generation_=commands_.generation(); clearTokens();pending_=false; }
private:
    void clearTokens() { for(auto& token:tokens_) token=0; }
    amy::Commands& commands_; PerformanceState state_;
    harmony::State harmony_;
    uint32_t tokens_[128]{},revision_=0,generation_=0;
    bool ready_=false;
    bool midi_connected_=false,midi_error_=false;
    bool pending_=false,loading_=false,failed_=false;
    uint32_t changes_=0,failures_=0;
};
}
