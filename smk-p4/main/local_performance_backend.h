#pragma once
#include "performance_backend.h"
#include "amy_runtime.h"
#include <cstdio>
#include "user_presets.h"
#ifdef SMKP4_SOUND_EDITOR
#include "preset_transaction.h"
#endif
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
#ifdef SMKP4_SOUND_EDITOR
        quiet_=snapshot.storage_quiet;
        if(edit_.patch!=snapshot.edit.patch || edit_.revision!=snapshot.edit.revision){
            const bool changed=edit_.epoch!=snapshot.edit.epoch;
            edit_=snapshot.edit;
            if(edit_.request>edit_serial_)edit_serial_=edit_.request;
            if(changed){edit_pending_=false;edit_token_=edit_.request;saved_valid_=false;user_name_[0]=0;}
            if(edit_.request==edit_token_){
                notice_=edit_.errors!=last_edit_errors_?"EDIT REJECTED; CONFIRMED VALUES RESTORED":"EDIT CONFIRMED";
                edit_error_=edit_.errors!=last_edit_errors_;last_edit_errors_=edit_.errors;
                if(recall_wait_ && edit_.request==recall_token_){
                    if(edit_.errors==recall_errors_){saved_edit_=edit_;saved_fx_=recall_fx_;saved_valid_=true;std::snprintf(user_name_,sizeof(user_name_),"%s",recall_.name);}
                    recall_wait_=false;
                }
            }
        }
#endif
        if(snapshot.patch_id!=state_.patch_id && !recalling_){desired_fx_=original_fx_=snapshot.fx.config;}
        fx_state_=snapshot.fx;
        ready_=snapshot.ready; state_.patch_id=state_.engine_patch=snapshot.patch_id;
        const int index=amy::patchIndex(snapshot.patch_id);
        std::snprintf(state_.patch_name,sizeof(state_.patch_name),"%s",index<0?"ERROR":amy::kPatches[index].name);
        #ifdef SMKP4_SOUND_EDITOR
        if(user_name_[0])std::snprintf(state_.patch_name,sizeof(state_.patch_name),"%s",user_name_);
#endif
        state_.master=snapshot.master; state_.active_notes=snapshot.active_notes;
        state_.sample_rate_hz=amy::kSampleRateHz; state_.block_frames=amy::kBlockFrames;
        state_.flags=(output_live?link::kAudioLive:0)|(midi_connected_?link::kUsbConnected:0);
        state_.max_render_us=max_render_us; state_.deadline_misses=deadlines;
        state_.commands_dropped=commands_.overflows();
        const uint32_t generation=commands_.generation();
        if(generation_!=generation) { generation_=generation; clearTokens(); }
        ++revision_;
    }
    void service(uint32_t now_ms) override {
        (void)now_ms;
#ifdef SMKP4_SOUND_EDITOR
        last_service_ms_=now_ms;
        if(saving_ && quiet_){
            const auto res=coordinator_.save(save_slot_,save_preset_,save_fx_);
            edit_error_=(res.status!=preset::SaveResult::Status::Success);
            notice_=res.message;
            if(!edit_error_){
                saved_edit_=edit_;
                saved_fx_=save_fx_;
                saved_valid_=true;
                std::snprintf(user_name_,sizeof(user_name_),"%s",save_preset_.name);
            }
            saving_=false;commands_.storage_requested.store(false,std::memory_order_release);
        }
        if(recalling_ && !busy() && !failed_ && edit_.patch==recall_.patch){
            desired_={};desired_.patch=edit_.patch;desired_.generation=commands_.generation();
            for(unsigned m=0;m<3;++m)desired_.macros[m]=recall_.macros[m];
            desired_.mask=recall_.mask;desired_.slot=int8_t(recall_slot_);
            for(unsigned p=0;p<edit::Count;++p)desired_.values[p]=recall_.values[p];
            desired_.request=newEditToken();edit_pending_=true;recalling_=false;recall_wait_=true;recall_token_=edit_token_;recall_errors_=edit_.errors;
            desired_fx_=original_fx_=recall_fx_;
            fx_pending_=true;
            fx::FxRequest req{};req.config=desired_fx_;req.generation=commands_.generation();req.request=++fx_token_;
            if(commands_.fx.push(req)){fx_pending_=false;last_fx_ms_=now_ms;}
        }
        if(!edit_pending_ && edit_.request!=edit_token_ && !edit_timeout_ && uint32_t(now_ms-last_edit_ms_)>2000){
            edit_timeout_=edit_error_=true;notice_="EDIT TIMEOUT; PANIC; RETRY AFTER SNAPSHOT";commands_.panic();
        }
        if(saving_ && !quiet_ && uint32_t(now_ms-save_requested_ms_)>2000){
            saving_=false;edit_error_=true;notice_="SAVE QUIET BARRIER TIMEOUT";commands_.storage_requested.store(false,std::memory_order_release);
        }
        if(edit_pending_ && !saving_ && !busy() && uint32_t(now_ms-last_edit_ms_)>=20){
            if(desired_.patch!=edit_.patch || desired_.generation!=commands_.generation()){
                edit_pending_=false;edit_token_=edit_.request;edit_error_=true;notice_="STALE EDIT CANCELED";
            }else if(commands_.edits.push(desired_)){edit_pending_=false;last_edit_ms_=now_ms;}
        }
#endif
        if(fx_pending_ && !busy() && uint32_t(now_ms-last_fx_ms_)>=15){
            fx::FxRequest req{};req.config=desired_fx_;req.generation=commands_.generation();req.request=++fx_token_;
            if(commands_.fx.push(req)){fx_pending_=false;last_fx_ms_=now_ms;}
        }
    }
#ifdef SMKP4_SOUND_EDITOR
    void presetStore(edit::PresetStore* store){
        store_=store;
        coordinator_.setSynthStore(static_cast<edit::VersionedPresetStore*>(store));
    }
    void fxPresetStore(fx::FxPresetStore* store){
        fx_store_=store;
        coordinator_.setFxStore(static_cast<fx::NvsFxPresets*>(store));
    }
    preset::PresetTransactionCoordinator& transactionCoordinator() { return coordinator_; }
    bool presetStorageReady() const { return store_ != nullptr && fx_store_ != nullptr; }
    bool fxStorageReady() const { return fx_store_ != nullptr; }
    bool userPresetFx(unsigned slot,fx::FxConfig& out){
        edit::Preset dummy_p;
        if(!coordinator_.load(slot,dummy_p,out)){out=fx::FxConfig{};return false;}
        return true;
    }
    const edit::State* soundEdit() const override { return &edit_; }
    uint32_t editCoalesced() const {return coalesced_;}
    bool editError() const override {return edit_error_;}
    bool editPending() const override {return edit_pending_ || (!edit_timeout_ && edit_.request!=edit_token_) || recalling_ || recall_wait_;}
    bool fxPending() const { return fx_pending_; }
    bool unsaved() const {
        if(saving_ || edit_pending_ || (!edit_timeout_ && edit_.request!=edit_token_) || fx_pending_)return true;
        return synthUnsaved() || fxUnsaved();
    }
    bool synthUnsaved() const {
        if(edit_pending_ || (!edit_timeout_ && edit_.request!=edit_token_))return true;
        if(!edit_.dirty)return false;
        if(!saved_valid_ || saved_edit_.patch!=edit_.patch || saved_edit_.mask!=edit_.mask)return true;
        for(unsigned m=0;m<3;++m)if(saved_edit_.macros[m]!=edit_.macros[m])return true;
        for(unsigned p=0;p<edit::Count;++p)if((edit_.mask&edit::bit(p)) && saved_edit_.manual[p]!=edit_.manual[p])return true;
        return false;
    }
    bool fxUnsaved() const {
        if(fx_pending_)return true;
        const bool fx_dirty = !fxEquivalent(desired_fx_, original_fx_);
        if(!fx_dirty)return false;
        if(!saved_valid_)return true;
        return !fxEquivalent(desired_fx_, saved_fx_);
    }
    const char* editNotice() const override {
        if(saving_)return notice_;
        if(edit_error_)return notice_;
        if(edit_pending_ || fx_pending_ || (!edit_timeout_ && edit_.request!=edit_token_))return "WAIT OWNER";
        if(notice_ && std::strcmp(notice_,"SAVED")==0 && !unsaved())return "SAVED";
        if(edit_.compare)return "ORIGINAL / EDITS RETAINED";
        if(unsaved()){
            if(synthUnsaved() && fxUnsaved())return "EDITED / UNSAVED";
            if(fxUnsaved())return "FX EDITED / UNSAVED";
            return "EDITED / UNSAVED";
        }
        return (edit_.dirty || !fxEquivalent(desired_fx_,original_fx_))?"EDITED / SAVED":"FACTORY ORIGINAL";
    }
    bool editParameter(unsigned p,float value) override {
        if(!ready_ || busy() || saving_ || edit_.compare || p>=edit::Count || !(edit_.available&edit::bit(p)) || !edit::valid(p,value))return false;
        if(edit_pending_)++coalesced_;
        prepareEdit();desired_.values[p]=value;
        if(value==edit_.original[p])desired_.mask&=~edit::bit(p);else desired_.mask|=edit::bit(p);
        desired_.request=newEditToken();edit_pending_=true;notice_="EDIT REQUESTED; WAIT OWNER";return true;
    }
    // 0 Compare,1 Revert,2 Save As,3 Recall. Overwrite/rename uses explicit Save As.
    bool editAction(unsigned action,unsigned slot=0,const char* name=nullptr) override {
        if(!ready_ || busy() || saving_)return false;
        if(action<2){prepareEdit();desired_.compare=action==0?!desired_.compare:false;
            if(action==1){desired_.mask=0;desired_.slot=-1;for(auto& m:desired_.macros)m=64;desired_fx_=original_fx_;}
            desired_.request=newEditToken();edit_pending_=true;
            fx::FxRequest req{};req.config=desired_.compare?original_fx_:desired_fx_;req.generation=commands_.generation();req.request=++fx_token_;commands_.fx.push(req);
            return true;}
        if(slot>=edit::kSlots || !store_ || edit_pending_ || edit_.request!=edit_token_)return false;
        if(action==2){
            if(!fx_store_){
                edit_error_=true;
                notice_="SAVE FAILED: FX STORAGE UNAVAILABLE";
                return false;
            }
            if(!name || !name[0] || std::strlen(name)>24 || state_.active_notes || edit_.compare)return false;
            save_preset_={};save_preset_.sequence=1;save_preset_.patch=edit_.patch;save_preset_.family=edit_.family;
            std::snprintf(save_preset_.name,sizeof(save_preset_.name),"%s",name);save_preset_.mask=edit_.mask;
            for(unsigned m=0;m<3;++m)save_preset_.macros[m]=edit_.macros[m];
            for(unsigned p=0;p<edit::Count;++p)save_preset_.values[p]=edit_.manual[p];
            if(!store_->accepts(save_preset_))return false;
            save_fx_=desired_fx_;
            save_slot_=slot;saving_=true;save_requested_ms_=last_service_ms_;notice_="SAVE REQUESTED; QUIET BARRIER";
            commands_.storage_requested.store(true,std::memory_order_release);return true;
        }
        if(action==3){
            if(unsaved()) return false;
            if(!coordinator_.load(slot,recall_,recall_fx_)) return false;
            if(amy::patchIndex(recall_.patch)<0) return false;
            recalling_=true;recall_slot_=slot;
            if(!setPatch(recall_.patch,0)){recalling_=false;return false;}return true;
        }
        return false;
    }
    bool userPreset(unsigned slot,char* name,unsigned capacity,uint16_t& origin) override {
        edit::Preset preset;fx::FxConfig dummy_fx{};
        if(!coordinator_.load(slot,preset,dummy_fx))return false;
        std::snprintf(name,capacity,"%s",preset.name);origin=preset.patch;return true;
    }
#endif
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
    const fx::FxState* fxState() const override { return &fx_state_; }
    uint32_t fxToken() const override { return fx_token_; }
    bool setFxParameter(const char* id, float value, uint32_t now_ms) override {
        if (!ready_ || busy()) return false;
        if (!id) return false;
        if (std::strcmp(id, "fx.chorus.mode") == 0) {
            desired_fx_.chorus_mode = static_cast<uint8_t>(std::clamp(int(std::lround(value)), 0, 5));
        } else if (std::strcmp(id, "fx.chorus.depth") == 0) {
            desired_fx_.chorus_depth = std::clamp(value / 100.0f, 0.0f, 1.0f);
        } else if (std::strcmp(id, "fx.delay.time_ms") == 0) {
            desired_fx_.delay_time_ms = std::clamp(value, 10.0f, 1000.0f);
        } else if (std::strcmp(id, "fx.delay.feedback") == 0) {
            desired_fx_.delay_feedback = std::clamp(value / 100.0f, 0.0f, 0.95f);
        } else if (std::strcmp(id, "fx.delay.mix") == 0) {
            desired_fx_.delay_mix = std::clamp(value / 100.0f, 0.0f, 1.0f);
        } else if (std::strcmp(id, "fx.reverb.size") == 0) {
            desired_fx_.reverb_size = std::clamp(value / 100.0f, 0.0f, 1.0f);
        } else if (std::strcmp(id, "fx.reverb.mix") == 0) {
            desired_fx_.reverb_mix = std::clamp(value / 100.0f, 0.0f, 1.0f);
        } else if (std::strcmp(id, "fx.master_tone") == 0) {
            desired_fx_.master_tone = std::clamp(value / 100.0f, -1.0f, 1.0f);
        } else {
            return false;
        }
        fx_pending_ = true;
        fx::FxRequest req{}; req.config = desired_fx_; req.generation = commands_.generation(); req.request = ++fx_token_;
        if (commands_.fx.push(req)) {
            fx_pending_ = false;
            last_fx_ms_ = now_ms;
        }
        return true;
    }
    bool patchError() const override { return failed_; }
    bool connected() const override { return ready_; }
    bool busy() const override { return pending_ || loading_
#ifdef SMKP4_SOUND_EDITOR
 || saving_ || quiet_
#endif
; }
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
#ifdef SMKP4_SOUND_EDITOR
        if(!recalling_ && unsaved())return false;
#endif
        if(!ready_ || busy() || amy::patchIndex(id)<0) return false;
        panic();
#ifdef SMKP4_SOUND_EDITOR
        if(!recalling_){
            desired_fx_=original_fx_=fx::FxConfig{};
        }
#endif
        const bool accepted=commands_.push({amy::CommandType::Patch,id,0,0,0,now_ms});
#ifdef SMKP4_UNIFIED_ENGINE
        pending_=accepted;
#endif
        return accepted;
    }
    bool setMacro(uint8_t index,uint8_t value,uint32_t) override {
#ifdef SMKP4_SOUND_EDITOR
        const int m=index==1?0:index==4?1:index==5?2:-1;
        return m>=0 && macroParameter(unsigned(m),value);
#else
        (void)index;(void)value;return false;
#endif
    }
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
    void panic() override { commands_.panic(); generation_=commands_.generation(); clearTokens();pending_=false;fx_pending_=false; }
private:
#ifdef SMKP4_SOUND_EDITOR
    bool macroParameter(unsigned m,uint8_t value){
        if(m>=3 || value>127 || busy() || !ready_ || edit_.compare || (edit_.family==0 && m>0))return false;
        prepareEdit();desired_.macros[m]=value;desired_.request=newEditToken();edit_pending_=true;return true;
    }
    static bool fxEquivalent(const fx::FxConfig& a, const fx::FxConfig& b) {
        if (a.chorus_mode != b.chorus_mode) return false;
        if (std::lround(a.chorus_depth * 100.0f) != std::lround(b.chorus_depth * 100.0f)) return false;
        if (std::lround(a.delay_time_ms) != std::lround(b.delay_time_ms)) return false;
        if (std::lround(a.delay_feedback * 100.0f) != std::lround(b.delay_feedback * 100.0f)) return false;
        if (std::lround(a.delay_mix * 100.0f) != std::lround(b.delay_mix * 100.0f)) return false;
        if (std::lround(a.reverb_size * 100.0f) != std::lround(b.reverb_size * 100.0f)) return false;
        if (std::lround(a.reverb_mix * 100.0f) != std::lround(b.reverb_mix * 100.0f)) return false;
        if (std::lround(a.master_tone * 100.0f) != std::lround(b.master_tone * 100.0f)) return false;
        if (std::lround(a.eq_low_db) != std::lround(b.eq_low_db)) return false;
        if (std::lround(a.eq_mid_db) != std::lround(b.eq_mid_db)) return false;
        if (std::lround(a.eq_high_db) != std::lround(b.eq_high_db)) return false;
        if (a.bypass_all != b.bypass_all) return false;
        return true;
    }
    uint32_t newEditToken(){edit_error_=edit_timeout_=false;edit_token_=++edit_serial_;return edit_token_;}
    void prepareEdit(){
        if(edit_timeout_){edit_token_=edit_.request;edit_pending_=false;edit_timeout_=false;}
        if(edit_pending_ || (desired_.request==edit_token_ && edit_.request!=edit_token_))return;
        desired_={};desired_.patch=edit_.patch;desired_.generation=commands_.generation();desired_.mask=edit_.mask;
        for(unsigned m=0;m<3;++m)desired_.macros[m]=edit_.macros[m];
        desired_.compare=edit_.compare;desired_.slot=edit_.user_slot;
        for(unsigned p=0;p<edit::Count;++p)desired_.values[p]=edit_.manual[p];
    }
    edit::State edit_;
    edit::Request desired_;
    edit::PresetStore* store_=nullptr;
    edit::Preset save_preset_,recall_;
    unsigned save_slot_=0,recall_slot_=0;
    bool edit_pending_=false,saving_=false,quiet_=false,recalling_=false;
    uint32_t coalesced_=0,save_requested_ms_=0,last_service_ms_=0,edit_serial_=0;
    bool edit_error_=false,edit_timeout_=false;
    uint32_t last_edit_ms_=0,edit_token_=0,last_edit_errors_=0;
    edit::State saved_edit_;
    bool saved_valid_=false,recall_wait_=false;
    uint32_t recall_token_=0,recall_errors_=0;
    char user_name_[25]{};
    const char* notice_="FACTORY ORIGINAL";
    preset::PresetTransactionCoordinator coordinator_;
#endif
    void clearTokens() { for(auto& token:tokens_) token=0; }
    amy::Commands& commands_; PerformanceState state_;
    harmony::State harmony_;
    uint32_t tokens_[128]{},revision_=0,generation_=0;
    bool ready_=false;
    bool midi_connected_=false,midi_error_=false;
    bool pending_=false,loading_=false,failed_=false;
    uint32_t changes_=0,failures_=0;
    fx::FxState fx_state_{};
    fx::FxConfig desired_fx_{};
    fx::FxConfig original_fx_{};
    fx::FxPresetStore* fx_store_=nullptr;
    fx::FxConfig save_fx_{}, saved_fx_{}, recall_fx_{};
    bool fx_pending_=false;
    uint32_t fx_token_=0,last_fx_ms_=0;
};
}
