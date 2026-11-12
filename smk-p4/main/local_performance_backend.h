#pragma once
#include "performance_backend.h"
#include "amy_runtime.h"
#include <cstdio>
#include "user_presets.h"
#ifdef SMKP4_SOUND_EDITOR
#include "preset_transaction.h"
#include "performance_factory_routes.h"
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
        edit_snapshot_generation_=snapshot.generation;
        quiet_=snapshot.storage_quiet;
        if(edit_.patch!=snapshot.edit.patch || edit_.revision!=snapshot.edit.revision){
            const bool changed=edit_.epoch!=snapshot.edit.epoch;
            edit_=snapshot.edit;
            if(edit_.request>edit_serial_)edit_serial_=edit_.request;
            if(changed){edit_pending_=false;edit_token_=edit_.request;saved_valid_=false;user_name_[0]=0;}
            if(edit_.request==edit_token_){
                notice_=edit_.errors!=last_edit_errors_?"EDIT REJECTED; CONFIRMED VALUES RESTORED":"EDIT CONFIRMED";
                edit_error_=edit_.errors!=last_edit_errors_;last_edit_errors_=edit_.errors;
                if(edit_.family == 0){
                    const unsigned cur_algo = unsigned(std::lround(edit_.applied[edit::FmAlgorithm]));
                    std::printf("FM_ALGO_DIAG requested=%u confirmed=%u token=%u generation=%u state=%s\n",
                        cur_algo, cur_algo, unsigned(edit_.request), unsigned(commands_.generation()),
                        edit_error_ ? "REJECTED" : "CONFIRMED");
                }

            }
        }
#endif
        if(snapshot.patch_id!=state_.patch_id
#ifdef SMKP4_SOUND_EDITOR
           && !recalling_
#endif
        ){
            desired_fx_=original_fx_=snapshot.fx.config;
        }
        fx_state_=snapshot.fx;
#ifdef SMKP4_SOUND_EDITOR
        if(recall_wait_ && commands_.generation()==recall_generation_ &&
           snapshot.generation==recall_generation_ && edit_.patch==recall_.patch){
            if((edit_.request==recall_token_ && edit_.errors!=recall_errors_) ||
               fx_state_.errors!=recall_fx_errors_){
                failRecall("RECALL REJECTED: OWNER ERROR", PresetRejectReason::InvalidPreset);
            }else if(edit_.request==recall_token_ && recall_fx_token_ &&
                     fx_state_.request==recall_fx_token_ &&
                     fxEquivalent(fx_state_.config,recall_fx_)){
                recall_wait_=false;
                saved_edit_=edit_;saved_fx_=original_fx_=fx_state_.config;saved_valid_=true;
                std::snprintf(user_name_,sizeof(user_name_),"%s",recall_.name);
                std::snprintf(preset_op_msg_,sizeof(preset_op_msg_),"SLOT %02u RECALLED",recall_slot_+1);
                preset_op_status_={PresetOperationState::Confirmed,3,uint8_t(recall_slot_),preset_op_msg_};
                logPresetDiag("RECALL",recall_slot_,"CONFIRMED",PresetRejectReason::None);
            }
        }
#endif
        ready_=snapshot.ready; state_.patch_id=state_.engine_patch=snapshot.patch_id;
        const int index=amy::patchIndex(snapshot.patch_id);
        std::snprintf(state_.patch_name,sizeof(state_.patch_name),"%s",index<0?"ERROR":amy::kPatches[index].name);
        #ifdef SMKP4_SOUND_EDITOR
        if(user_name_[0])std::snprintf(state_.patch_name,sizeof(state_.patch_name),"%s",user_name_);
#endif
        state_.master=snapshot.master; state_.active_notes=snapshot.active_notes;
        sustain_active_=snapshot.sustain_active;
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
            preset_op_status_.state = PresetOperationState::Writing;
            std::snprintf(preset_op_msg_, sizeof(preset_op_msg_), "WRITING PRESET...");
            preset_op_status_.message = preset_op_msg_;
            const auto res=coordinator_.save(save_slot_,save_preset_,save_fx_);
            edit_error_=(res.status!=preset::SaveResult::Status::Success);
            notice_=res.message;
            if(!edit_error_){
                saved_edit_=edit_;
                saved_fx_=save_fx_;
                saved_valid_=true;
                std::snprintf(user_name_,sizeof(user_name_),"%s",save_preset_.name);
                std::snprintf(preset_op_msg_, sizeof(preset_op_msg_), "SAVED TO SLOT %02u", save_slot_ + 1);
                preset_op_status_ = { PresetOperationState::Confirmed, 2, uint8_t(save_slot_), preset_op_msg_ };
                logPresetDiag("SAVE", save_slot_, "CONFIRMED", PresetRejectReason::None);
            } else if(res.status == preset::SaveResult::Status::CommitOutcomeUnknown){
                uncertain_[save_slot_]={true,res.expected_commit,res.expected_synth};
                std::snprintf(preset_op_msg_, sizeof(preset_op_msg_), "SAVE UNCERTAIN: CHECK SLOT %02u", save_slot_ + 1);
                preset_op_status_ = { PresetOperationState::OutcomeUnknown, 2, uint8_t(save_slot_), preset_op_msg_ };
                logPresetDiag("SAVE", save_slot_, "OUTCOME_UNKNOWN", PresetRejectReason::CommitUncertain);
            } else {
                std::snprintf(preset_op_msg_, sizeof(preset_op_msg_), "%s", res.message ? res.message : "SAVE FAILED: STORAGE ERROR");
                preset_op_status_ = { PresetOperationState::Failed, 2, uint8_t(save_slot_), preset_op_msg_ };
                logPresetDiag("SAVE", save_slot_, "FAILED", PresetRejectReason::StorageWriteError);
            }
            saving_=false;commands_.storage_requested.store(false,std::memory_order_release);
        }
        const uint32_t owner_generation=commands_.generation();
        const auto transition=commands_.transition_context.load(std::memory_order_acquire);
        const bool recall_patch_interrupted=recalling_ && owner_generation!=recall_generation_ &&
            (uint32_t(owner_generation-recall_generation_)>2 ||
             ((transition>>4)==(owner_generation&0x0fffffffu) &&
              amy::OutputReason(transition&7u)!=amy::OutputReason::Patch));
        if(recalling_ && failures_!=recall_patch_failures_){
            failRecall("RECALL FAILED: PATCH LOAD ERROR", PresetRejectReason::InvalidPreset);
        }else if((recall_wait_ && owner_generation!=recall_generation_) || recall_patch_interrupted){
            failRecall("RECALL CANCELED: STALE GENERATION", PresetRejectReason::StaleGeneration);
        }else if((recalling_ || recall_wait_) && amy::eventAgeMs(now_ms,recall_started_ms_)>=amy::kRecallTimeoutMs){
            failRecall("RECALL NOT CONFIRMED: OWNER TIMEOUT", PresetRejectReason::OwnerTimeout);
        }
        // The owner advances the generation at patch admission and load completion.
        if(recalling_ && !busy() && !failed_ && changes_!=recall_patch_changes_ &&
           commands_.generation()==recall_generation_+2 && edit_.patch==recall_.patch){
            std::snprintf(preset_op_msg_, sizeof(preset_op_msg_), "VERIFYING SLOT %02u...", recall_slot_ + 1);
            preset_op_status_ = { PresetOperationState::Verifying, 3, uint8_t(recall_slot_), preset_op_msg_ };
            desired_={};desired_.patch=edit_.patch;desired_.generation=commands_.generation();
            for(unsigned m=0;m<3;++m)desired_.macros[m]=recall_.macros[m];
            desired_.performance_macros=recall_.performance_macros;
            desired_.mask=recall_.mask;desired_.slot=int8_t(recall_slot_);
            for(unsigned p=0;p<edit::Count;++p)desired_.values[p]=recall_.values[p];
            desired_.preset_recall=true;desired_.recall_started_ms=recall_started_ms_;
            desired_.request=newEditToken();edit_pending_=true;recalling_=false;recall_wait_=true;recall_token_=edit_token_;recall_errors_=edit_.errors;
            recall_generation_=desired_.generation;
            recall_fx_errors_=fx_state_.errors;
            recall_fx_token_=0;
            original_fx_=fx_state_.config;
            desired_fx_=recall_fx_;
            fx_pending_=true;
        }
        if(!recall_wait_ && !edit_pending_ && edit_.request!=edit_token_ && !edit_timeout_ && uint32_t(now_ms-last_edit_ms_)>2000){
            edit_timeout_=edit_error_=true;notice_="EDIT TIMEOUT; PANIC; RETRY AFTER SNAPSHOT";commands_.panic();
        }
        if(saving_ && !quiet_ && uint32_t(now_ms-save_requested_ms_)>2000){
            saving_=false;edit_error_=true;notice_="SAVE QUIET BARRIER TIMEOUT";commands_.storage_requested.store(false,std::memory_order_release);
            std::snprintf(preset_op_msg_, sizeof(preset_op_msg_), "SAVE QUIET BARRIER TIMEOUT");
            preset_op_status_ = { PresetOperationState::Failed, 2, uint8_t(save_slot_), preset_op_msg_ };
            logPresetDiag("SAVE", save_slot_, "TIMEOUT", PresetRejectReason::QuietBarrier);
        }
        if(edit_pending_ && !saving_ && !busy() && uint32_t(now_ms-last_edit_ms_)>=20){
            if(desired_.patch!=edit_.patch || desired_.generation!=commands_.generation()){
                edit_pending_=false;edit_token_=edit_.request;edit_error_=true;notice_="STALE EDIT CANCELED";
            }else if(commands_.edits.push(desired_)){edit_pending_=false;last_edit_ms_=now_ms;}
        }
#endif
        if(fx_pending_ && !busy() && uint32_t(now_ms-last_fx_ms_)>=15){
            fx::FxRequest req{};req.config=desired_fx_;req.generation=commands_.generation();req.request=++fx_token_;
#ifdef SMKP4_SOUND_EDITOR
            req.preset_recall=recall_wait_;req.recall_started_ms=recall_started_ms_;
#endif
            if(commands_.fx.push(req)){
                fx_pending_=false;last_fx_ms_=now_ms;
#ifdef SMKP4_SOUND_EDITOR
                if(recall_wait_) recall_fx_token_=req.request;
#endif
            }
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
        if (!edit_.dirty) {
            if (saved_valid_ && edit_.patch == saved_edit_.patch && edit_.user_slot == saved_edit_.user_slot)
                for (unsigned m = 0; m < 3; ++m)
                    if (edit_.macros[m] != saved_edit_.macros[m]) return true;
            return false;
        }
        if(!saved_valid_ || saved_edit_.patch!=edit_.patch || saved_edit_.mask!=edit_.mask)return true;
        for(unsigned m=0;m<3;++m)if(saved_edit_.macros[m]!=edit_.macros[m])return true;
        if(saved_edit_.performance_macros!=edit_.performance_macros)return true;
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
        if(saving_)return "SAVE IN PROGRESS";
        if(edit_error_)return notice_;
        if(edit_pending_ || fx_pending_ || (!edit_timeout_ && edit_.request!=edit_token_))return "WAIT OWNER";
        if(notice_ && std::strcmp(notice_,"SAVED")==0 && !unsaved())return "SAVED";
        if(edit_.compare)return "COMPARE ACTIVE";
        if(unsaved()){
            if(synthUnsaved() && fxUnsaved())return "SYNTH + FX UNSAVED";
            if(synthUnsaved())return "EDITED / UNSAVED";
            if(fxUnsaved())return "FX EDITED / UNSAVED";
            return "EDITED / UNSAVED";
        }
        return (edit_.dirty || !fxEquivalent(desired_fx_,original_fx_))?"EDITED / SAVED":"FACTORY ORIGINAL";
    }
    uint32_t editGeneration() const override { return commands_.generation(); }
    uint32_t editSnapshotGeneration() const override { return edit_snapshot_generation_; }
    uint32_t editToken() const override { return edit_token_; }
    bool editParameter(unsigned p,float value) override {
        if(!ready_ || busy() || saving_ || recalling_ || recall_wait_ || edit_.compare || p>=edit::Count || !(edit_.available&edit::bit(p)) || !edit::valid(p,value))return false;
        if(p == edit::FmAlgorithm) {
            const unsigned target_algo = unsigned(std::lround(value));
            const unsigned current_algo = unsigned(std::lround(edit_.applied[edit::FmAlgorithm]));
            if(target_algo != current_algo && (state_.active_notes > 0 || sustain_active_)) {
                edit_error_ = true;
                notice_ = "RELEASE NOTES TO CHANGE FM ALGORITHM";
                const auto reason = state_.active_notes > 0 ? PresetRejectReason::ActiveNotes : PresetRejectReason::SustainActive;
                setRejectNotice(reason, notice_);
                std::printf("FM_ALGO_DIAG requested=%u confirmed=%u token=%u generation=%u state=REJECTED\n",
                    target_algo, current_algo, unsigned(edit_token_), unsigned(commands_.generation()));
                return false;
            }
        }
        if(edit_pending_)++coalesced_;
        prepareEdit();desired_.values[p]=value;
        if(value==edit_.original[p])desired_.mask&=~edit::bit(p);else desired_.mask|=edit::bit(p);
        desired_.request=newEditToken();edit_pending_=true;notice_="EDIT REQUESTED; WAIT OWNER";
        if(p == edit::FmAlgorithm) {
            const unsigned target_algo = unsigned(std::lround(value));
            const unsigned current_algo = unsigned(std::lround(edit_.applied[edit::FmAlgorithm]));
            std::printf("FM_ALGO_DIAG requested=%u confirmed=%u token=%u generation=%u state=PENDING\n",
                target_algo, current_algo, unsigned(desired_.request), unsigned(commands_.generation()));
        }
        return true;
    }
    PresetRejectReason lastPresetRejectReason() const override { return last_reject_reason_; }
    const char* presetRejectNotice() const override { return reject_notice_[0] ? reject_notice_ : nullptr; }
    PresetOperationStatus presetOperationStatus() const override {
        auto status=preset_op_status_;
        status.revision=preset_op_revision_;
        return status;
    }
    // 0 Compare,1 Revert,2 Save As,3 Recall. Overwrite/rename uses explicit Save As.
    bool editAction(unsigned action,unsigned slot=0,const char* name=nullptr) override {
        const char* act_name = (action == 0) ? "COMPARE" : (action == 1) ? "REVERT" : (action == 2) ? "SAVE" : (action == 3) ? "RECALL" : "UNKNOWN";
        if(recalling_ || recall_wait_){
            setRejectNotice(PresetRejectReason::PendingEdit, "PRESET BLOCKED: RECALL IN PROGRESS");
            logPresetDiag(act_name, slot, "REJECTED", PresetRejectReason::PendingEdit);
            return false;
        }
        if(!ready_){
            setRejectNotice(PresetRejectReason::EngineUnavailable, "PRESET ENGINE UNAVAILABLE");
            preset_op_status_ = { PresetOperationState::Rejected, uint8_t(action), uint8_t(slot), reject_notice_ };
            logPresetDiag(act_name, slot, "REJECTED", PresetRejectReason::EngineUnavailable);
            return false;
        }
        if(busy()){
            setRejectNotice(PresetRejectReason::Busy, "PRESET BLOCKED: ENGINE BUSY");
            preset_op_status_ = { PresetOperationState::Rejected, uint8_t(action), uint8_t(slot), reject_notice_ };
            logPresetDiag(act_name, slot, "REJECTED", PresetRejectReason::Busy);
            return false;
        }
        if(saving_){
            setRejectNotice(PresetRejectReason::SaveInProgress, "PRESET BLOCKED: SAVE IN PROGRESS");
            preset_op_status_ = { PresetOperationState::Rejected, uint8_t(action), uint8_t(slot), reject_notice_ };
            logPresetDiag(act_name, slot, "REJECTED", PresetRejectReason::SaveInProgress);
            return false;
        }
        if(action<2){
            if(edit_.family == 0 && (state_.active_notes > 0 || sustain_active_)) {
                const unsigned current_algo = unsigned(std::lround(edit_.applied[edit::FmAlgorithm]));
                unsigned target_algo = current_algo;
                if(action == 0) { // Compare
                    target_algo = (!edit_.compare)
                        ? unsigned(std::lround(edit_.original[edit::FmAlgorithm]))
                        : ((edit_.mask & edit::bit(edit::FmAlgorithm))
                            ? unsigned(std::lround(edit_.manual[edit::FmAlgorithm]))
                            : unsigned(std::lround(edit_.original[edit::FmAlgorithm])));
                } else if(action == 1) { // Revert
                    target_algo = unsigned(std::lround(edit_.original[edit::FmAlgorithm]));
                }
                if(target_algo != current_algo) {
                    edit_error_ = true;
                    notice_ = "RELEASE NOTES TO CHANGE FM ALGORITHM";
                    const auto reason = state_.active_notes > 0 ? PresetRejectReason::ActiveNotes : PresetRejectReason::SustainActive;
                    setRejectNotice(reason, "RELEASE NOTES TO CHANGE FM ALGORITHM");
                    preset_op_status_ = { PresetOperationState::Rejected, uint8_t(action), uint8_t(slot), reject_notice_ };
                    logPresetDiag(act_name, slot, "REJECTED", reason);
                    return false;
                }
            }
            prepareEdit();desired_.compare=action==0?!desired_.compare:false;
            if(action==1){desired_.mask=0;desired_.slot=-1;for(auto& m:desired_.macros)m=64;desired_.performance_macros=performance::kNeutral;desired_fx_=original_fx_;}
            desired_.request=newEditToken();edit_pending_=true;
            fx::FxRequest req{};req.config=desired_.compare?original_fx_:desired_fx_;req.generation=commands_.generation();req.request=++fx_token_;commands_.fx.push(req);
            setRejectNotice(PresetRejectReason::None, nullptr);
            logPresetDiag(act_name, slot, "ACCEPTED", PresetRejectReason::None);
            return true;
        }
        if(slot>=edit::kSlots){
            setRejectNotice(PresetRejectReason::InvalidSlot, "PRESET INVALID: SLOT RANGE");
            preset_op_status_ = { PresetOperationState::Rejected, uint8_t(action), uint8_t(slot), reject_notice_ };
            logPresetDiag(act_name, slot, "REJECTED", PresetRejectReason::InvalidSlot);
            return false;
        }
        if(!store_){
            setRejectNotice(PresetRejectReason::StorageUnavailable, "PRESET STORAGE UNAVAILABLE");
            preset_op_status_ = { PresetOperationState::Rejected, uint8_t(action), uint8_t(slot), reject_notice_ };
            logPresetDiag(act_name, slot, "REJECTED", PresetRejectReason::StorageUnavailable);
            return false;
        }
        if(edit_pending_ || edit_.request!=edit_token_){
            setRejectNotice(PresetRejectReason::PendingEdit, "SAVE BLOCKED: WAITING FOR EDIT OWNER");
            preset_op_status_ = { PresetOperationState::Rejected, uint8_t(action), uint8_t(slot), reject_notice_ };
            logPresetDiag(act_name, slot, "REJECTED", PresetRejectReason::PendingEdit);
            return false;
        }
        if(action==2){
            if(!fx_store_){
                edit_error_=true;
                notice_="SAVE FAILED: FX STORAGE UNAVAILABLE";
                setRejectNotice(PresetRejectReason::StorageUnavailable,notice_);
                preset_op_status_={PresetOperationState::Rejected,2,uint8_t(slot),notice_};
                return false;
            }
            auto& uncertain=uncertain_[slot];
            if(uncertain.active){
                edit::Preset stored{};
                fx::FxConfig stored_fx{};
                preset::CommitMarker marker{};
                if(coordinator_.load(slot,stored,stored_fx,&marker) &&
                   marker.commit_seq==uncertain.commit && stored.sequence==uncertain.synth){
                    uncertain.active=false;
                    ++preset_op_revision_;
                    preset_op_status_={PresetOperationState::Confirmed,2,uint8_t(slot),"PREVIOUS SAVE VERIFIED; CONFIRM AGAIN"};
                    setRejectNotice(PresetRejectReason::CommitUncertain,"PREVIOUS SAVE VERIFIED; CONFIRM AGAIN");
                }else{
                    preset_op_status_={PresetOperationState::OutcomeUnknown,2,uint8_t(slot),"SAVE UNCERTAIN: SLOT WRITE BLOCKED"};
                    setRejectNotice(PresetRejectReason::CommitUncertain,"SAVE UNCERTAIN: SLOT WRITE BLOCKED");
                }
                return false;
            }
            if(edit_.compare){
                setRejectNotice(PresetRejectReason::CompareActive, "OVERWRITE BLOCKED: COMPARE ACTIVE");
                preset_op_status_ = { PresetOperationState::Rejected, uint8_t(action), uint8_t(slot), reject_notice_ };
                logPresetDiag(act_name, slot, "REJECTED", PresetRejectReason::CompareActive);
                return false;
            }
            if(state_.active_notes > 0){
                setRejectNotice(PresetRejectReason::ActiveNotes, "SAVE BLOCKED: NOTES STILL ACTIVE");
                preset_op_status_ = { PresetOperationState::Rejected, uint8_t(action), uint8_t(slot), reject_notice_ };
                logPresetDiag(act_name, slot, "REJECTED", PresetRejectReason::ActiveNotes);
                return false;
            }
            if(sustain_active_){
                setRejectNotice(PresetRejectReason::SustainActive, "SAVE BLOCKED: SUSTAIN ACTIVE");
                preset_op_status_ = { PresetOperationState::Rejected, uint8_t(action), uint8_t(slot), reject_notice_ };
                logPresetDiag(act_name, slot, "REJECTED", PresetRejectReason::SustainActive);
                return false;
            }
            if(!name || !name[0] || std::strlen(name)>24){
                setRejectNotice(PresetRejectReason::InvalidName, "SAVE BLOCKED: INVALID PRESET NAME");
                preset_op_status_ = { PresetOperationState::Rejected, uint8_t(action), uint8_t(slot), reject_notice_ };
                logPresetDiag(act_name, slot, "REJECTED", PresetRejectReason::InvalidName);
                return false;
            }
            save_preset_={};save_preset_.sequence=1;save_preset_.patch=edit_.patch;save_preset_.family=edit_.family;
            std::snprintf(save_preset_.name,sizeof(save_preset_.name),"%s",name);save_preset_.mask=edit_.mask;
            for(unsigned m=0;m<3;++m)save_preset_.macros[m]=edit_.macros[m];
            save_preset_.performance_macros=edit_.performance_macros;
            for(unsigned p=0;p<edit::Count;++p)save_preset_.values[p]=edit_.manual[p];
            if(!store_->accepts(save_preset_)){
                setRejectNotice(PresetRejectReason::InvalidPreset, "SAVE BLOCKED: INVALID PRESET");
                preset_op_status_ = { PresetOperationState::Rejected, uint8_t(action), uint8_t(slot), reject_notice_ };
                logPresetDiag(act_name, slot, "REJECTED", PresetRejectReason::InvalidPreset);
                return false;
            }
            save_fx_=desired_fx_;
            ++preset_op_revision_;
            save_slot_=slot;saving_=true;save_requested_ms_=last_service_ms_;notice_="SAVE REQUESTED; QUIET BARRIER";
            const auto st = quiet_ ? PresetOperationState::Writing : PresetOperationState::WaitingForQuiet;
            std::snprintf(preset_op_msg_, sizeof(preset_op_msg_), "%s", quiet_ ? "WRITING PRESET..." : "WAITING FOR QUIET");
            preset_op_status_ = { st, 2, uint8_t(slot), preset_op_msg_ };
            commands_.storage_requested.store(true,std::memory_order_release);
            setRejectNotice(PresetRejectReason::None, nullptr);
            logPresetDiag(act_name, slot, "ACCEPTED", PresetRejectReason::None);
            return true;
        }
        if(action==3){
            if(unsaved()){
                if(synthUnsaved()){
                    setRejectNotice(PresetRejectReason::UnsavedSynth, "RECALL BLOCKED: UNSAVED SYNTH EDITS");
                    preset_op_status_ = { PresetOperationState::Rejected, uint8_t(action), uint8_t(slot), reject_notice_ };
                    logPresetDiag(act_name, slot, "REJECTED", PresetRejectReason::UnsavedSynth);
                } else if(fxUnsaved()){
                    setRejectNotice(PresetRejectReason::UnsavedFx, "RECALL BLOCKED: UNSAVED FX EDITS");
                    preset_op_status_ = { PresetOperationState::Rejected, uint8_t(action), uint8_t(slot), reject_notice_ };
                    logPresetDiag(act_name, slot, "REJECTED", PresetRejectReason::UnsavedFx);
                } else {
                    setRejectNotice(PresetRejectReason::PendingEdit, "RECALL BLOCKED: PENDING EDITS");
                    preset_op_status_ = { PresetOperationState::Rejected, uint8_t(action), uint8_t(slot), reject_notice_ };
                    logPresetDiag(act_name, slot, "REJECTED", PresetRejectReason::PendingEdit);
                }
                return false;
            }
            if(!coordinator_.load(slot,recall_,recall_fx_)){
                char emsg[48]; std::snprintf(emsg, sizeof(emsg), "PRESET INVALID: SLOT %02u", slot + 1);
                setRejectNotice(PresetRejectReason::StorageReadError, emsg);
                preset_op_status_ = { PresetOperationState::Rejected, uint8_t(action), uint8_t(slot), reject_notice_ };
                logPresetDiag(act_name, slot, "REJECTED", PresetRejectReason::StorageReadError);
                return false;
            }
            if(amy::patchIndex(recall_.patch)<0){
                setRejectNotice(PresetRejectReason::InvalidPreset, "RECALL FAILED: UNKNOWN PATCH ROM");
                preset_op_status_ = { PresetOperationState::Rejected, uint8_t(action), uint8_t(slot), reject_notice_ };
                logPresetDiag(act_name, slot, "REJECTED", PresetRejectReason::InvalidPreset);
                return false;
            }
            ++preset_op_revision_;
            recalling_=true;recall_slot_=slot;
            std::snprintf(preset_op_msg_, sizeof(preset_op_msg_), "RECALL REQUESTED: SLOT %02u", slot + 1);
            preset_op_status_ = { PresetOperationState::Requested, 3, uint8_t(slot), preset_op_msg_ };
            recall_started_ms_=last_service_ms_;
            if(!setPatch(recall_.patch,recall_started_ms_)){
                recalling_=false;
                setRejectNotice(PresetRejectReason::Busy, "RECALL BLOCKED: ENGINE BUSY");
                preset_op_status_ = { PresetOperationState::Rejected, 3, uint8_t(slot), reject_notice_ };
                logPresetDiag(act_name, slot, "REJECTED", PresetRejectReason::Busy);
                return false;
            }
            recall_generation_=commands_.generation();
            recall_patch_changes_=changes_;
            recall_patch_failures_=failures_;
            setRejectNotice(PresetRejectReason::None, nullptr);
            logPresetDiag(act_name, slot, "ACCEPTED", PresetRejectReason::None);
            return true;
        }
        setRejectNotice(PresetRejectReason::InvalidPreset, "UNKNOWN PRESET ACTION");
        preset_op_status_ = { PresetOperationState::Rejected, uint8_t(action), uint8_t(slot), reject_notice_ };
        logPresetDiag(act_name, slot, "REJECTED", PresetRejectReason::InvalidPreset);
        return false;
    }
    bool userPreset(unsigned slot,char* name,unsigned capacity,uint16_t& origin) override {
        uint32_t sequence=0;
        return userPresetIdentity(slot,name,capacity,origin,sequence);
    }
    bool userPresetIdentity(unsigned slot,char* name,unsigned capacity,uint16_t& origin,uint32_t& sequence) override {
        edit::Preset preset;fx::FxConfig dummy_fx{};
        if(!coordinator_.load(slot,preset,dummy_fx))return false;
        std::snprintf(name,capacity,"%s",preset.name);origin=preset.patch;sequence=preset.sequence;return true;
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
#ifdef SMKP4_SOUND_EDITOR
        if(recalling_ || recall_wait_)return false;
#endif
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
 || saving_ || quiet_ || profile_quiet_requested_
#endif
; }
#ifdef SMKP4_SOUND_EDITOR
    bool requestStorageQuiet() override {
        if(!ready_ || busy() || editPending() || fx_pending_ || state_.active_notes || sustain_active_)return false;
        profile_quiet_requested_=true;
        commands_.storage_requested.store(true,std::memory_order_release);
        return true;
    }
    bool storageQuiet() const override {return profile_quiet_requested_ && quiet_;}
    void releaseStorageQuiet() override {
        if(!profile_quiet_requested_)return;
        profile_quiet_requested_=false;
        commands_.storage_requested.store(false,std::memory_order_release);
    }
#endif
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
        if(recall_wait_ || (!recalling_ && unsaved()))return false;
#endif
        if(!ready_ || busy() || amy::patchIndex(id)<0) return false;
        panic();
#ifdef SMKP4_SOUND_EDITOR
        if(!recalling_){
            desired_fx_=original_fx_=fx::FxConfig{};
        }
#endif
        amy::Command request{amy::CommandType::Patch,id,0,0,0,now_ms};
#ifdef SMKP4_SOUND_EDITOR
        request.preset_recall=recalling_;
#endif
        const bool accepted=commands_.push(request);
#ifdef SMKP4_UNIFIED_ENGINE
        pending_=accepted;
#endif
        return accepted;
    }
    bool resetLegacyOffsets() override {
#ifdef SMKP4_SOUND_EDITOR
        if (!ready_ || busy() || saving_ || recalling_ || recall_wait_ ||
            edit_.compare || editPending() || !edit_.available) return false;
        prepareEdit();
        for (auto& position : desired_.macros) position = 64;
        desired_.request = newEditToken();
        edit_pending_ = true;
        return true;
#else
        return false;
#endif
    }
    bool setMacro(uint8_t index,uint8_t value,uint32_t) override {
#ifdef SMKP4_SOUND_EDITOR
        if(index>=performance::kMacroCount || value>127 || saving_ || recalling_ || recall_wait_ ||
           busy() || !ready_ || edit_.compare || !performance::available(index,edit_)) return false;
        prepareEdit();
        if(desired_.compare)return false;
        desired_.performance_macros[index]=value;
        desired_.request=newEditToken();
        edit_pending_=true;
        return true;
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
        desired_.performance_macros=edit_.performance_macros;
        desired_.compare=edit_.compare;desired_.slot=edit_.user_slot;
        for(unsigned p=0;p<edit::Count;++p)desired_.values[p]=edit_.manual[p];
    }
    edit::State edit_;
    edit::Request desired_;
    edit::PresetStore* store_=nullptr;
    edit::Preset save_preset_,recall_;
    unsigned save_slot_=0,recall_slot_=0;
    bool edit_pending_=false,saving_=false,quiet_=false,recalling_=false,profile_quiet_requested_=false;
    uint32_t edit_snapshot_generation_=0;
    uint32_t coalesced_=0,save_requested_ms_=0,last_service_ms_=0,edit_serial_=0;
    bool edit_error_=false,edit_timeout_=false;
    uint32_t last_edit_ms_=0,edit_token_=0,last_edit_errors_=0;
    edit::State saved_edit_;
    bool saved_valid_=false,recall_wait_=false;
    uint32_t recall_token_=0,recall_errors_=0;
    uint32_t recall_started_ms_=0,recall_generation_=0,recall_patch_changes_=0,recall_patch_failures_=0;
    uint32_t recall_fx_token_=0,recall_fx_errors_=0;
    void failRecall(const char* message, PresetRejectReason reason){
        if(recalling_)pending_=false;
        if(recall_wait_){
            edit_pending_=false;
            edit_token_=edit_.request;
            fx_pending_=false;
            desired_fx_=fx_state_.config;
        }
        recalling_=recall_wait_=false;
        edit_error_=true;
        notice_=message;
        preset_op_status_={PresetOperationState::Failed,3,uint8_t(recall_slot_),message};
        logPresetDiag("RECALL",recall_slot_,"FAILED",reason);
    }
    char user_name_[25]{};
    const char* notice_="FACTORY ORIGINAL";
    PresetOperationStatus preset_op_status_{};
    uint32_t preset_op_revision_=0;
    struct UncertainSave { bool active=false; uint32_t commit=0,synth=0; };
    UncertainSave uncertain_[edit::kSlots]{};
    char preset_op_msg_[64]{};
    PresetRejectReason last_reject_reason_ = PresetRejectReason::None;
    char reject_notice_[64]{};
    void setRejectNotice(PresetRejectReason reason, const char* msg) {
        last_reject_reason_ = reason;
        if(msg) std::snprintf(reject_notice_, sizeof(reject_notice_), "%s", msg);
        else reject_notice_[0] = 0;
    }
    void logPresetDiag(const char* action, unsigned slot, const char* state_str, PresetRejectReason reason) {
        const char* reason_str = "NONE";
        switch (reason) {
            case PresetRejectReason::None: reason_str = "NONE"; break;
            case PresetRejectReason::EngineUnavailable: reason_str = "ENGINE_UNAVAILABLE"; break;
            case PresetRejectReason::Busy: reason_str = "BUSY"; break;
            case PresetRejectReason::SaveInProgress: reason_str = "SAVE_IN_PROGRESS"; break;
            case PresetRejectReason::QuietBarrier: reason_str = "QUIET_BARRIER"; break;
            case PresetRejectReason::PendingEdit: reason_str = "PENDING_EDIT"; break;
            case PresetRejectReason::ActiveNotes: reason_str = "ACTIVE_NOTES"; break;
            case PresetRejectReason::SustainActive: reason_str = "SUSTAIN_ACTIVE"; break;
            case PresetRejectReason::CompareActive: reason_str = "COMPARE_ACTIVE"; break;
            case PresetRejectReason::UnsavedSynth: reason_str = "UNSAVED_SYNTH"; break;
            case PresetRejectReason::UnsavedFx: reason_str = "UNSAVED_FX"; break;
            case PresetRejectReason::InvalidSlot: reason_str = "INVALID_SLOT"; break;
            case PresetRejectReason::InvalidName: reason_str = "INVALID_NAME"; break;
            case PresetRejectReason::InvalidPreset: reason_str = "INVALID_PRESET"; break;
            case PresetRejectReason::StorageUnavailable: reason_str = "STORAGE_UNAVAILABLE"; break;
            case PresetRejectReason::StorageReadError: reason_str = "STORAGE_READ_ERROR"; break;
            case PresetRejectReason::StorageWriteError: reason_str = "STORAGE_WRITE_ERROR"; break;
            case PresetRejectReason::CommitUncertain: reason_str = "COMMIT_UNCERTAIN"; break;
            case PresetRejectReason::OwnerTimeout: reason_str = "OWNER_TIMEOUT"; break;
            case PresetRejectReason::StaleGeneration: reason_str = "STALE_GENERATION"; break;
        }
        std::printf("PRESET_DIAG action=%s slot=%u state=%s reason=%s active_notes=%u sustain=%d saving=%d quiet=%d edit_pending=%d synth_unsaved=%d fx_unsaved=%d compare=%d generation=%u\n",
            action, slot, state_str, reason_str,
            unsigned(state_.active_notes), sustain_active_ ? 1 : 0,
            saving_ ? 1 : 0, quiet_ ? 1 : 0,
            edit_pending_ ? 1 : 0,
            synthUnsaved() ? 1 : 0,
            fxUnsaved() ? 1 : 0,
            edit_.compare ? 1 : 0,
            unsigned(commands_.generation()));
    }
    preset::PresetTransactionCoordinator coordinator_;
#endif
    void clearTokens() { for(auto& token:tokens_) token=0; }
    amy::Commands& commands_; PerformanceState state_;
    harmony::State harmony_;
    uint32_t tokens_[128]{},revision_=0,generation_=0;
    bool ready_=false;
    bool midi_connected_=false,midi_error_=false,sustain_active_=false;
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
