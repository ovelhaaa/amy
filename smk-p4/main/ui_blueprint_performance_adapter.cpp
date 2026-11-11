#include "ui_blueprint_performance_adapter.h"
#include "performance_factory_routes.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace smk::p4 {
using namespace ui;
namespace {
constexpr const char* kHarmonyIds[]={"harmony.scale_enable","harmony.chord_enable","harmony.apply_touch",
    "harmony.root_note","harmony.scale_type","harmony.quant_mode","harmony.chord_type",
    "harmony.inversion","harmony.voicing","harmony.spread","harmony.usb_channel","harmony.preset"};
int harmonyIndex(const char* id){if(id)for(unsigned i=0;i<12;++i)if(std::strcmp(id,kHarmonyIds[i])==0)return int(i);return -1;}
bool eq(const char* a,const char* b) { return a && std::strcmp(a,b)==0; }
bool isFxId(const char* id) {
    return eq(id,"fx.chorus.mode") || eq(id,"fx.chorus.depth") ||
           eq(id,"fx.delay.time_ms") || eq(id,"fx.delay.feedback") || eq(id,"fx.delay.mix") ||
           eq(id,"fx.reverb.size") || eq(id,"fx.reverb.mix") || eq(id,"fx.master_tone");
}
}
UiBlueprintPerformanceAdapter::UiBlueprintPerformanceAdapter(PerformanceBackend& backend):backend_(backend) {
    meters_.is_simulated=false; // No amplitude measurement available: meters stay zero/N/A.
    auto& d=model_.diagnostics();
    d={}; d.is_demo_mode=false; d.firmware_version="SMK-P4 LIBRARY 0.7"; d.max_voices=backend_.voiceCapacity();
#if defined(SMKP4_UNIFIED_ENGINE) || defined(CONFIG_SMKP4_UNIFIED_ENGINE)
    d.firmware_version="SMK-P4 UNIFIED 0.8 CANDIDATE";
#if defined(SMKP4_HARMONY) || defined(CONFIG_SMKP4_HARMONY)
    d.firmware_version="SMK-P4 HARMONY 0.9 EXPERIMENTAL";
#if defined(SMKP4_FULL_FACTORY_LIBRARY) || defined(CONFIG_SMKP4_FULL_FACTORY_LIBRARY)
    d.firmware_version="SMK-P4 FACTORY 0.10 EXPERIMENTAL";
#endif
#endif
#endif
#if defined(SMKP4_SOUND_EDITOR) || defined(CONFIG_SMKP4_SOUND_EDITOR)
    d.firmware_version="SMK-P4 EDITOR 0.11.1 DEV";
#endif
    d.amy_status="WAIT SNAPSHOT"; d.i2s_status="NOT LIVE"; d.usb_status="DISCONNECTED";
    model_.setBool("harmony.scale_enable",false);
    model_.setBool("harmony.chord_enable",false);
    syncHarmony({});
    model_.setInt("fx.chorus.mode",0);
    model_.setFloat("fx.chorus.depth",0.0f);
    model_.setFloat("fx.delay.time_ms",350.0f);
    model_.setFloat("fx.delay.feedback",40.0f);
    model_.setFloat("fx.delay.mix",0.0f);
    model_.setFloat("fx.reverb.size",70.0f);
    model_.setFloat("fx.reverb.mix",0.0f);
    model_.setFloat("fx.master_tone",0.0f);
    model_.setNotice("WAIT OWNER SNAPSHOT; TOUCH LEASE 10s");
}
UiCommandResult UiBlueprintPerformanceAdapter::result(UiCommandState state,const char* id,const char* notice,uint32_t token) {
    if(state==UiCommandState::Requested && id) resetPickup(id);
    last_state_=state;
    last_token_=token;
    // Retain canonical/static IDs, never a caller-owned temporary string.
    const auto* desc=ParameterRegistry::findById(id);
    last_id_=desc?desc->id:"command";
    model_.setNotice(notice); model_.bumpRevision();
    return {state,token,notice};
}
void UiBlueprintPerformanceAdapter::syncHarmony(harmony::Config c) {
    const int values[]={c.scale_on,c.chord_on,c.touch,c.root,c.scale,c.quant,c.chord,c.inversion,c.open,c.spread,int(c.channel)+1};
    for(unsigned i=0;i<11;++i)model_.setInt(kHarmonyIds[i],values[i]);
}
int UiBlueprintPerformanceAdapter::editIndex(const char* id) const {
    if(eq(id,"filter.cutoff_hz"))return edit::Cutoff;
    if(eq(id,"filter.resonance"))return edit::Resonance;
    if(eq(id,"filter.env_amount"))return edit::FilterEnv;
    if(eq(id,"filter.keytrack"))return edit::FilterKeytrack;
    if(eq(id,"env.amp.attack_ms"))return edit::Attack;
    if(eq(id,"env.amp.release_ms"))return edit::Release;
    if(eq(id,"env.amp.decay_ms"))return edit::Decay;
    if(eq(id,"env.amp.sustain"))return edit::Sustain;
    if(eq(id,"osc.1.level"))return edit::OscLevel;
    if(eq(id,"osc.sub_level"))return edit::SubLevel;
    if(eq(id,"osc.noise_level"))return edit::NoiseLevel;
    if(eq(id,"osc.1.pulse_width"))return edit::PulseWidth;
    if(eq(id,"osc.1.detune_ct"))return edit::Detune;
    if(eq(id,"amp.level"))return edit::PatchLevel;
    if(eq(id,"amp.pan"))return edit::Pan;
    if(eq(id,"amp.vel_sens"))return edit::Velocity;
    if(eq(id,"fm.depth"))return edit::Depth;
    if(eq(id,"fm.algo"))return edit::FmAlgorithm;
    if(eq(id,"fm.feedback"))return edit::FmFeedback;
    if(eq(id,"fm.op.level"))return edit::OpLevel0+int(selected_op_);
    if(eq(id,"fm.op.ratio"))return edit::OpRatio0+int(selected_op_);
    if(eq(id,"fm.op.attack"))return edit::OpAttack0+int(selected_op_);
    if(eq(id,"fm.op.decay"))return edit::OpDecay0+int(selected_op_);
    if(eq(id,"fm.op.vel_sens"))return edit::OpVel0+int(selected_op_);
    if(eq(id,"lfo.1.rate_hz"))return edit::LfoRate;
    if(eq(id,"lfo.1.depth"))return edit::LfoDepth;
    if(eq(id,"lfo.1.wave"))return edit::LfoWave;
    if(eq(id,"lfo.1.dest"))return edit::LfoDest;
    if(eq(id,"mod.slot1.enable"))return edit::ModEnable;
    if(eq(id,"mod.slot1.source"))return edit::ModSource;
    if(eq(id,"mod.slot1.dest"))return edit::ModDest;
    if(eq(id,"mod.slot1.amount"))return edit::ModAmount;
    if(eq(id,"mod.slot1.polarity"))return edit::ModPolarity;
    return -1;
}
UiCommandResult UiBlueprintPerformanceAdapter::requestSoundAction(unsigned action,unsigned slot,const char* name){
    if(pending_edit_.active && pending_edit_.param==edit::FmAlgorithm)
        return result(UiCommandState::Rejected,"editor","ALGORITHM CHANGE PENDING");
    if(!backend_.editAction(action,slot,name)){
        const char* notice = backend_.presetRejectNotice();
        return result(UiCommandState::Rejected,"editor",(notice && notice[0])?notice:"COMMAND REJECTED");
    }
    edit_requested_=true;edit_errors_=backend_.soundEdit()?backend_.soundEdit()->errors:0;edit_id_="editor";
    if(action<=1)resetPickup();
    return result(UiCommandState::Requested,"editor",backend_.editNotice());
}
ui::PresetOperationStatus UiBlueprintPerformanceAdapter::presetOperationStatus() const {
    const auto s = backend_.presetOperationStatus();
    return { static_cast<ui::PresetOperationState>(s.state), s.action, s.slot, s.message, s.revision };
}
FeatureStatus UiBlueprintPerformanceAdapter::parameterStatus(const char* id) const {
    if(const auto* state=backend_.soundEdit()){
        if(id && std::strncmp(id,"perf.macro.",11)==0 && id[11]>='1' && id[11]<='8' && !id[12])
            return performance::available(unsigned(id[11]-'1'),*state)?FeatureStatus::Available:FeatureStatus::Unavailable;
        if(eq(id,"fm.selected_op"))return state->family==0?FeatureStatus::Available:FeatureStatus::Unavailable;
        const int p=editIndex(id);
        if(p>=0)return (state->available&edit::bit(unsigned(p)))?FeatureStatus::Available:FeatureStatus::Unavailable;
    }
    if(backend_.harmonyState() && harmonyIndex(id)>=0)return FeatureStatus::Available;
    if(backend_.fxState() && isFxId(id))return FeatureStatus::Available;
    return eq(id,"perf.master_volume") || eq(id,"perf.octave_shift") || eq(id,"perf.patch_id")
        ? FeatureStatus::Available : FeatureStatus::Planned;
}
bool UiBlueprintPerformanceAdapter::isParameterValueSupported(const char* id,int32_t value) const {
    if(eq(id,"fx.chorus.mode"))return value>=0 && value<=5;
    if(eq(id,"fm.algo"))return value>=1 && value<=32;
    if(const auto* state=backend_.soundEdit()){
        if(eq(id,"lfo.1.wave"))return value>=0 && value<=4;
        if(eq(id,"lfo.1.dest"))return value>=0 && value<=3 && (value!=2 || state->family==1);
        if(eq(id,"mod.slot1.source"))return value==0 || value==1;
        if(eq(id,"mod.slot1.dest"))return value==0 || value==1 || (value==3 && state->family==1);
        if(eq(id,"mod.slot1.polarity"))return value>=0 && value<=2;
    }
    if(!backend_.harmonyState())return true;
    if(eq(id,"harmony.chord_type"))return value!=10 && value!=11;
    if(eq(id,"harmony.voicing"))return value<=1;
    if(eq(id,"harmony.inversion") && value==3){const auto c=backend_.harmonyState()->configuration;return c.chord==7 || c.chord==8 || c.chord==9 || c.chord==13;}
    return true;
}
bool UiBlueprintPerformanceAdapter::isParameterEditable(const char* id) const {
    return parameterStatus(id)==FeatureStatus::Available && usable();
}
bool UiBlueprintPerformanceAdapter::isPatchSelectable(uint16_t id) const {
    size_t count=0; const auto* entries=UiModel::catalog(count);
    bool represented=false;
    for(size_t i=0;i<count;++i) if(entries[i].id==id && entries[i].status!=FeatureStatus::Demo) represented=true;
    if(!represented) return false;
    for(uint16_t i=0;i<backend_.patchCount();++i) if(backend_.patchAt(i)==id) {
        PatchInfo info;
        return backend_.patchInfo(i,info,now_ms_) && info.id==id && info.available;
    }
    return false;
}
FeatureStatus UiBlueprintPerformanceAdapter::patchStatus(uint16_t id) const {
#if defined(SMKP4_FULL_FACTORY_LIBRARY) || defined(CONFIG_SMKP4_FULL_FACTORY_LIBRARY)
    return isPatchSelectable(id)?FeatureStatus::Available:FeatureStatus::Unavailable;
#else
    return isPatchSelectable(id)?FeatureStatus::Available:FeatureStatus::Planned;
#endif
}
UiCommandResult UiBlueprintPerformanceAdapter::requestParameterValue(const char* id,const ParameterValue& v) {
    if(v.type==ParameterValueType::String) return result(UiCommandState::Rejected,id,"INVALID VALUE TYPE");
    return requestParameterFloat(id,v.asFloat());
}
UiCommandResult UiBlueprintPerformanceAdapter::requestParameterFloat(const char* id,float v) {
    if(backend_.soundEdit() && editIndex(id)>=0){
        if(pending_edit_.active && pending_edit_.param==edit::FmAlgorithm)
            return result(UiCommandState::Rejected,id,"ALGORITHM CHANGE PENDING");
        const uint32_t generation=backend_.editGeneration();
        if(!usable() || !backend_.editParameter(unsigned(editIndex(id)),v)) {
            const char* notice = backend_.editNotice();
            return result(UiCommandState::Rejected,id,(notice && notice[0])?notice:"EDIT REJECTED; SEE CONFIRMED VALUE");
        }
        edit_requested_=true;edit_errors_=backend_.soundEdit()->errors;edit_id_=ParameterRegistry::findById(id)->id;
        pending_edit_.active = true;
        pending_edit_.param = unsigned(editIndex(id));
        pending_edit_.requested_value = v;
        pending_edit_.token = backend_.editToken();
        pending_edit_.patch_id = static_cast<uint16_t>(model_.getInt("perf.patch_id", 0));
        pending_edit_.requested_ms = now_ms_;
        pending_edit_.initial_errors = backend_.soundEdit()->errors;
        pending_edit_.generation = generation;
        const char* req_notice = (pending_edit_.param == edit::FmAlgorithm) ? "ALGORITHM REQUESTED; WAIT OWNER" : "EDIT REQUESTED; WAIT OWNER";
        const auto request=result(UiCommandState::Requested,id,req_notice,pending_edit_.token);
        if(pending_edit_.param==edit::FmAlgorithm) fm_algorithm_result_=request;
        return request;
    }
    if(backend_.fxState() && isFxId(id)){
        if(!usable())return result(UiCommandState::Rejected,id,"ENGINE/SNAPSHOT UNAVAILABLE");
        const auto* initial = backend_.fxState();
        fx_initial_errors_ = initial ? initial->errors : 0;
        fx_initial_rejected_ = initial ? initial->rejected : 0;
        if(!backend_.setFxParameter(id,v,now_ms_))return result(UiCommandState::Rejected,id,"FX REJECTED; SEE CONFIRMED VALUE");
        fx_requested_=true;
        fx_requested_ms_=now_ms_;
        fx_pending_token_=backend_.fxToken();
        const auto* desc=ParameterRegistry::findById(id);
        fx_id_=desc?desc->id:"fx";
        return result(UiCommandState::Requested,id,"FX REQUESTED; WAIT OWNER");
    }
    if(!std::isfinite(v) || v < -32768 || v > 65535) return result(UiCommandState::Rejected,id,"INVALID VALUE");
    return requestParameterInt(id,static_cast<int32_t>(std::lround(v)));
}
UiCommandResult UiBlueprintPerformanceAdapter::requestParameterBool(const char* id,bool v) {
    return requestParameterInt(id,v?1:0);
}
UiCommandResult UiBlueprintPerformanceAdapter::requestParameterInt(const char* id,int32_t v) {
    if(backend_.soundEdit() && editIndex(id)>=0){
        if(!isParameterValueSupported(id,v))return result(UiCommandState::Rejected,id,"EDIT REJECTED; UNSUPPORTED VALUE");
        return requestParameterFloat(id,float(v));
    }
    if(backend_.fxState() && isFxId(id)){
        if(!isParameterValueSupported(id,v))return result(UiCommandState::Rejected,id,"FX REJECTED; UNSUPPORTED VALUE");
        return requestParameterFloat(id,float(v));
    }
    if(backend_.soundEdit() && eq(id,"fm.selected_op")){
        if(v<1 || v>6 || backend_.soundEdit()->family!=0)return result(UiCommandState::Rejected,id,"OPERATOR UNAVAILABLE");
        selected_op_=unsigned(v-1);model_.setInt(id,v);
        resetPickup();
        const auto* edit=backend_.soundEdit();
        model_.setFloat("fm.op.level",edit->applied[edit::OpLevel0+selected_op_]);
        model_.setFloat("fm.op.ratio",edit->applied[edit::OpRatio0+selected_op_]);
        model_.setFloat("fm.op.attack",edit->applied[edit::OpAttack0+selected_op_]);
        model_.setFloat("fm.op.decay",edit->applied[edit::OpDecay0+selected_op_]);
        model_.setFloat("fm.op.vel_sens",edit->applied[edit::OpVel0+selected_op_]);
        return result(UiCommandState::Confirmed,id,"OPERATOR SELECTED; LOCAL MAP VERIFIED");
    }
    if(parameterStatus(id)!=FeatureStatus::Available) return result(UiCommandState::Unsupported,id,"PLANNED; NO DSP COMMAND");
    if(!usable()) return result(UiCommandState::Rejected,id,"ENGINE/SNAPSHOT UNAVAILABLE");
    if(backend_.soundEdit() && std::strncmp(id,"perf.macro.",11)==0){
        if(pending_edit_.active && pending_edit_.param==edit::FmAlgorithm)
            return result(UiCommandState::Rejected,id,"ALGORITHM CHANGE PENDING");
        const unsigned m=unsigned(id[11]-'1');
        if(v<0 || v>127 || !backend_.setMacro(uint8_t(m),uint8_t(v),now_ms_))return result(UiCommandState::Rejected,id,"MACRO REJECTED");
        edit_requested_=true;edit_errors_=backend_.soundEdit()->errors;edit_id_=ParameterRegistry::findById(id)->id;
        pending_edit_={};
        pending_edit_.active=true;
        pending_edit_.param=edit::Count;
        pending_edit_.patch_id=backend_.state().patch_id;
        pending_edit_.token=backend_.editToken();
        pending_edit_.generation=backend_.editGeneration();
        pending_edit_.requested_ms=now_ms_;
        return result(UiCommandState::Requested,id,"MACRO REQUESTED; WAIT OWNER",pending_edit_.token);
    }
    const int hi=harmonyIndex(id);
    if(hi>=0 && backend_.harmonyState()) {
        if(harmony_pending_)return result(UiCommandState::Rejected,id,"HARMONY WAIT OWNER");
        auto c=backend_.harmonyState()->configuration;
        if(v<0 || v>127)return result(UiCommandState::Rejected,id,"HARMONY OUT OF RANGE");
        switch(hi){
            case 0:if(v>1)break;c.scale_on=v;goto submit;
            case 1:if(v>1)break;c.chord_on=v;goto submit;
            case 2:if(v>1)break;c.touch=v;goto submit;
            case 3:if(v>11)break;c.root=v;goto submit;
            case 4:if(v>10)break;c.scale=v;goto submit;
            case 5:if(v>5)break;c.quant=v;goto submit;
            case 6:if(v>13)break;c.chord=v;goto submit;
            case 7:if(v>3)break;c.inversion=v;goto submit;
            case 8:if(v>1)break;c.open=v;goto submit;
            case 9:if(v>2)break;c.spread=v;goto submit;
            case 10:if(v<1 || v>16)break;c.channel=v-1;goto submit;
            case 11:if(v>4)break;c=harmony::preset(v,c);goto submit;
        }
        return result(UiCommandState::Rejected,id,"HARMONY VALUE PLANNED/INVALID");
submit:
        if(!harmony::valid(c) || !backend_.setHarmony(c,now_ms_))return result(UiCommandState::Rejected,id,"HARMONY INCOMPATIBLE/REJECTED");
        harmony_id_=kHarmonyIds[hi];
        if(hi==11)harmony_preset_=v;
        requested_harmony_=c;harmony_pending_=true;harmony_requested_ms_=now_ms_;
        harmony_revision_=backend_.harmonyState()->revision;
        return result(UiCommandState::Requested,id,"HARMONY REQUESTED; WAIT OWNER");
    }
    if(eq(id,"perf.octave_shift")) {
        if(v < -2 || v > 2) return result(UiCommandState::Rejected,id,"OCTAVE OUT OF RANGE");
        releaseAllUiNotes(); // Old releases cannot terminate the next octave generation.
        model_.setInt(id,v);
        return result(UiCommandState::Confirmed,id,"OCTAVE INPUT UPDATED; TOUCH RELEASED");
    }
    if(eq(id,"perf.patch_id")) {
        if(v<0 || v>65535) return result(UiCommandState::Rejected,id,"PATCH OUT OF RANGE");
        return requestSelectPatch(static_cast<uint16_t>(v));
    }
    if(v<0 || v>127) return result(UiCommandState::Rejected,id,"MASTER OUT OF RANGE");
    if(!backend_.setMaster(static_cast<uint8_t>(v),now_ms_)) return result(UiCommandState::Rejected,id,"MASTER REJECTED");
    master_={true,static_cast<uint16_t>(v),backend_.stateRevision(),now_ms_};
    return result(UiCommandState::Requested,id,"MASTER REQUESTED; WAIT SNAPSHOT");
}
UiCommandResult UiBlueprintPerformanceAdapter::requestSelectPatch(uint16_t id,uint8_t layer) {
    if(layer || !isPatchSelectable(id)) return result(UiCommandState::Unsupported,"perf.patch_id","PATCH/LAYER UNAVAILABLE");
    if(!usable()) return result(UiCommandState::Rejected,"perf.patch_id","ENGINE/SNAPSHOT UNAVAILABLE");
    releaseAllUiNotes();
    if(!backend_.setPatch(id,now_ms_)) return result(UiCommandState::Rejected,"perf.patch_id","PATCH REJECTED; NOTES CANCELED");
    patch_={true,id,backend_.stateRevision(),now_ms_};
    return result(UiCommandState::Requested,"perf.patch_id","PATCH REQUESTED; WAIT SNAPSHOT");
}
bool UiBlueprintPerformanceAdapter::isNoteHeld(uint8_t note) const {
    for(const auto& g:gestures_) if(g.token && g.note==note) return true;
    return false;
}
UiCommandResult UiBlueprintPerformanceAdapter::requestNoteOn(uint8_t note,uint8_t velocity) {
    if(!usable() || note>127 || velocity==0 || velocity>127 || next_token_==UINT32_MAX)
        return result(UiCommandState::Rejected,"note","NOTE REJECTED");
    Gesture* free=nullptr;
    for(auto& g:gestures_) if(!g.token) { free=&g; break; }
    if(!free) return result(UiCommandState::Rejected,"note","GESTURE LEDGER FULL");
    if(!isNoteHeld(note) && !backend_.noteOn(note,velocity,now_ms_)) return result(UiCommandState::Rejected,"note","NOTE REJECTED");
    uint32_t start=now_ms_;
    for(const auto& g:gestures_) if(g.token && g.note==note) { start=g.start_ms; break; }
    *free={++next_token_,start,note};
    model_.bumpRevision();
    return result(UiCommandState::Requested,"note","TOUCH REQUESTED; MAX 10s",free->token);
}
UiCommandResult UiBlueprintPerformanceAdapter::requestNoteOff(uint8_t note,uint32_t token) {
    for(auto& g:gestures_) if(token && g.token==token && g.note==note) {
        g.token=0;
        if(!isNoteHeld(note)) backend_.noteOff(note);
        model_.bumpRevision();
        return result(UiCommandState::Requested,"note","TOUCH RELEASE REQUESTED");
    }
    return result(UiCommandState::Rejected,"note","STALE/UNKNOWN RELEASE IGNORED");
}
void UiBlueprintPerformanceAdapter::releaseAllUiNotes() {
    for(auto& g:gestures_) if(g.token) {
        const uint8_t note=g.note; g.token=0;
        if(!isNoteHeld(note)) backend_.noteOff(note);
    }
    model_.bumpRevision();
}
UiCommandResult UiBlueprintPerformanceAdapter::requestPanic() {
    releaseAllUiNotes(); backend_.panic();
    patch_.active=master_.active=false;
    return result(UiCommandState::Requested,"panic","PANIC REQUESTED; OWNER CANCELS ALL SOURCES");
}
const char* UiBlueprintPerformanceAdapter::midiBankName() const {
    static constexpr const char* names[]{"A / MACROS", "B / OSC-FM", "C / ENVELOPES", "D / FX"};
    return names[midi_bank_];
}

void UiBlueprintPerformanceAdapter::setControllerProfileStore(performance::ControllerProfileStore& store) {
    midi_store_=&store;
    performance::ControllerProfile profile;
    if(!store.load(profile) || !performance::validProfile(profile)) return;
    midi_bank_=profile.bank;
    for(unsigned i=0;i<8;++i){
        midi_knobs_[i]={};
        midi_knobs_[i].source=profile.knobs[i];
    }
    midi_learn_.cancel();
}

const char* UiBlueprintPerformanceAdapter::midiDestination(unsigned knob) const {
    const auto* state=backend_.soundEdit();
    if(!state || knob>=8) return nullptr;
    static constexpr const char* destinations[6][8]{
        {"perf.macro.1","perf.macro.2","perf.macro.3","perf.macro.4","perf.macro.5","perf.macro.6","perf.macro.7","perf.macro.8"},
        {"osc.1.level","osc.sub_level","osc.noise_level","osc.1.pulse_width","osc.1.detune_ct","amp.level","amp.pan","amp.vel_sens"},
        {"fm.depth","fm.feedback","fm.op.level","fm.op.ratio","amp.level",nullptr,nullptr,nullptr},
        {"filter.cutoff_hz","filter.resonance","filter.env_amount","env.amp.attack_ms","env.amp.decay_ms","env.amp.sustain","env.amp.release_ms","filter.keytrack"},
        {"fm.op.attack","fm.op.decay","fm.op.vel_sens","lfo.1.rate_hz","lfo.1.depth",nullptr,nullptr,nullptr},
        {"fx.chorus.depth","fx.delay.time_ms","fx.delay.feedback","fx.delay.mix","fx.reverb.size","fx.reverb.mix","fx.master_tone",nullptr}
    };
    const bool fm=state->family==0;
    const unsigned row=midi_bank_==0?0:midi_bank_==1?1+fm:midi_bank_==2?3+fm:5;
    return destinations[row][knob];
}

bool UiBlueprintPerformanceAdapter::midiTarget(unsigned knob,uint8_t& target) const {
    const char* id=midiDestination(knob);
    const auto* desc=id?ParameterRegistry::findById(id):nullptr;
    if(!desc || parameterStatus(id)!=FeatureStatus::Available) return false;
    const int p=editIndex(id);
    const float manual=p>=0?backend_.soundEdit()->manual[p]:model_.getFloat(id,desc->default_value);
    if(!std::isfinite(manual) || desc->maximum<=desc->minimum) return false;
    const float bounded=std::clamp(manual,desc->minimum,desc->maximum);
    const bool logarithmic=desc->scale==ParameterScale::Logarithmic && desc->minimum>0;
    const float position=logarithmic?std::log(bounded/desc->minimum)/std::log(desc->maximum/desc->minimum):
        (bounded-desc->minimum)/(desc->maximum-desc->minimum);
    target=uint8_t(std::lround(127*std::clamp(position,0.0f,1.0f)));
    return true;
}

void UiBlueprintPerformanceAdapter::resetPickup(const char* id) {
    if(midi_dispatch_) return;
    for(unsigned i=0;i<8;++i){
        const auto* destination=midiDestination(i);
        if(!id || (destination && eq(id,destination))) midi_knobs_[i].pickup.reset();
    }
}

void UiBlueprintPerformanceAdapter::midiKnobText(unsigned knob,char* text,unsigned size) const {
    if(!size || knob>=8) return;
    const char* id=midiDestination(knob);
    const auto* desc=id?ParameterRegistry::findById(id):nullptr;
    uint8_t target=0;
    if(!desc || !midiTarget(knob,target)){
        std::snprintf(text,size,"K%u  N/A",knob+1);
        return;
    }
    const auto& k=midi_knobs_[knob];
    if(k.source.channel>=16){
        std::snprintf(text,size,"K%u  %s | UNBOUND",knob+1,desc->name);
        return;
    }
    const auto state=k.pickup.state();
    const char* pickup=state==performance::PickupState::Captured?"PICKED UP":
        state==performance::PickupState::ApproachingFromBelow?"TURN RIGHT":
        state==performance::PickupState::ApproachingFromAbove?"TURN LEFT":"WAITING";
    if(k.observed) std::snprintf(text,size,"K%u %s | CH%u CC%u | %u / %u %s",knob+1,desc->name,
                                 k.source.channel+1,k.source.controller,target,k.physical,pickup);
    else std::snprintf(text,size,"K%u %s | CH%u CC%u | %u %s",knob+1,desc->name,
                       k.source.channel+1,k.source.controller,target,pickup);
}

void UiBlueprintPerformanceAdapter::midiPerformanceAction(MidiAction action,unsigned knob) {
    if(!hasMidiPerformance() || knob>=8) return;
    const auto generation=midi_input_->deviceGeneration();
    switch(action){
        case MidiAction::NextBank:
            midi_bank_=(midi_bank_+1)%4;
            resetPickup();
            midi_learn_.cancel();
            std::snprintf(midi_status_,sizeof(midi_status_),"%s",midiBankName());
            break;
        case MidiAction::Learn:
        case MidiAction::LearnModWheel:
            if(!model_.diagnostics().midi_connected){
                std::snprintf(midi_status_,sizeof(midi_status_),"CONNECT A USB MIDI CONTROLLER");
                break;
            }
            midi_learn_knob_=knob;
            midi_learn_.arm(now_ms_,generation,16,action==MidiAction::LearnModWheel);
            std::snprintf(midi_status_,sizeof(midi_status_),"K%u: MOVE A MIDI CONTROL",knob+1);
            break;
        case MidiAction::Confirm:
        case MidiAction::Replace: {
            if(!midi_learn_.confirm(now_ms_,generation)) break;
            const auto source=midi_learn_.candidate();
            int conflict=-1;
            for(unsigned i=0;i<8;++i) if(i!=midi_learn_knob_ && midi_knobs_[i].source==source) conflict=int(i);
            if(conflict>=0 && action!=MidiAction::Replace){
                midi_learn_.resolve(false,now_ms_,generation);
                std::snprintf(midi_status_,sizeof(midi_status_),"CH%u CC%u IN USE BY K%d. REPLACE OR CANCEL",
                              source.channel+1,source.controller,conflict+1);
                break;
            }
            if(conflict>=0) midi_knobs_[conflict]={};
            midi_knobs_[midi_learn_knob_]={};
            midi_knobs_[midi_learn_knob_].source=source;
            midi_learn_.resolve(true,now_ms_,generation);
            std::snprintf(midi_status_,sizeof(midi_status_),"K%u BOUND TO CH%u CC%u",midi_learn_knob_+1,
                          source.channel+1,source.controller);
            break;
        }
        case MidiAction::Cancel:
            midi_learn_.cancel();
            std::snprintf(midi_status_,sizeof(midi_status_),"LEARN CANCELLED");
            break;
        case MidiAction::SaveProfile:
            if(midi_save_pending_) break;
            if(!midi_store_ || !backend_.requestStorageQuiet()){
                std::snprintf(midi_status_,sizeof(midi_status_),"PROFILE SAVE UNAVAILABLE OR ENGINE BUSY");
                break;
            }
            midi_learn_.cancel();
            midi_saved_profile_.bank=uint8_t(midi_bank_);
            for(unsigned i=0;i<8;++i) midi_saved_profile_.knobs[i]=midi_knobs_[i].source;
            midi_save_started_ms_=now_ms_;
            midi_save_pending_=true;
            std::snprintf(midi_status_,sizeof(midi_status_),"WAITING FOR QUIET TO SAVE PROFILE");
            break;
    }
    model_.bumpRevision();
}

void UiBlueprintPerformanceAdapter::serviceMidi() {
    if(!hasMidiPerformance()) return;
    if(midi_save_pending_ && (backend_.storageQuiet() || now_ms_-midi_save_started_ms_>=2000)){
        const bool saved=backend_.storageQuiet() && midi_store_->save(midi_saved_profile_);
        backend_.releaseStorageQuiet();
        midi_save_pending_=false;
        std::snprintf(midi_status_,sizeof(midi_status_),"%s",saved?"PROFILE SAVED":"PROFILE SAVE FAILED");
        model_.bumpRevision();
    }
    const auto& edit=*backend_.soundEdit();
    const auto device=midi_input_->deviceGeneration();
    const auto owner=backend_.editGeneration();
    if(device!=midi_device_generation_ || owner!=midi_owner_generation_ ||
       edit.epoch!=midi_edit_epoch_ || edit.compare!=midi_compare_){
        resetPickup();
        midi_device_generation_=device;
        midi_owner_generation_=owner;
        midi_edit_epoch_=edit.epoch;
        midi_compare_=edit.compare;
    }
    const auto learn_before=midi_learn_.state();
    midi_learn_.service(now_ms_,device);
    if(midi_learn_.state()!=learn_before){
        std::snprintf(midi_status_,sizeof(midi_status_),"%s",
                      midi_learn_.state()==performance::LearnState::Timeout?"LEARN TIMEOUT":"LEARN CANCELLED: USB CHANGED");
        model_.bumpRevision();
    }
    // Keep only the latest value per knob for this UI cycle.
    bool changed[8]{};
    uint8_t values[8]{};
    amy::MidiControlEvent event;
    for(unsigned i=0;i<amy::MidiControlEvents::kCapacity && midi_input_->popControl(event);++i){
        if(!midi_input_->currentControl(event)) continue;
        if(midi_learn_.capture(event,now_ms_,device)){
            std::snprintf(midi_status_,sizeof(midi_status_),"K%u: CH%u CC%u. CONFIRM OR CANCEL",
                          midi_learn_knob_+1,event.channel+1,event.controller);
            model_.bumpRevision();
            continue;
        }
        const auto learning=midi_learn_.state();
        if(learning==performance::LearnState::Armed || learning==performance::LearnState::Captured) continue;
        const performance::MidiSource source{event.cable,event.channel,event.controller};
        for(unsigned k=0;k<8;++k) if(midi_knobs_[k].source==source){
            auto& knob=midi_knobs_[k];
            uint8_t target=0,output=0;
            knob.physical=event.value;
            knob.observed=true;
            if(usable() && !edit.compare && midiTarget(k,target) && knob.pickup.update(event.value,target,output)){
                changed[k]=output!=target;
                values[k]=output;
            }
            model_.bumpRevision();
        }
    }
    if(device!=midi_input_->deviceGeneration() || owner!=backend_.editGeneration()) return;
    for(unsigned k=0;k<8;++k) if(changed[k]){
        const auto* id=midiDestination(k);
        const auto* desc=ParameterRegistry::findById(id);
        if(!desc) continue;
        const float position=values[k]/127.0f;
        const float value=desc->scale==ParameterScale::Logarithmic && desc->minimum>0?
            std::clamp(desc->minimum*std::pow(desc->maximum/desc->minimum,position),desc->minimum,desc->maximum):
            desc->minimum+(desc->maximum-desc->minimum)*position;
        midi_dispatch_=true;
        const auto result=requestParameterFloat(id,value);
        midi_dispatch_=false;
        if(!result.accepted()) midi_knobs_[k].pickup.reset();
    }
}
void UiBlueprintPerformanceAdapter::service(uint32_t now_ms) {
    now_ms_=now_ms; backend_.service(now_ms);
    if(was_connected_ && !backend_.connected()) {
        releaseAllUiNotes(); patch_.active=master_.active=false; have_snapshot_=false;
        result(UiCommandState::Rejected,"connection","ENGINE DISCONNECTED");
    }
    was_connected_=backend_.connected();
    // Fixed lease per pitch starts at FIRST accepted On. Later gestures cannot renew it.
    for(auto& g:gestures_) if(g.token && uint32_t(now_ms-g.start_ms)>=kTouchLeaseMs) {
        const uint8_t note=g.note;
        for(auto& same:gestures_) if(same.token && same.note==note) same.token=0;
        backend_.noteOff(note); model_.setNotice("TOUCH LEASE EXPIRED; RELEASE AND PRESS AGAIN"); model_.bumpRevision();
    }
    const uint32_t rev=backend_.stateRevision();
    if(backend_.connected() && rev && (!have_snapshot_ || rev!=revision_)) {
        have_snapshot_=true; revision_=rev;
        if(const auto* edit=backend_.soundEdit()){
            if(edit_requested_ && !(pending_edit_.active && pending_edit_.param==edit::FmAlgorithm) && !backend_.editPending() && !backend_.busy()){
                edit_requested_=false;
                if(pending_edit_.active && (backend_.state().patch_id != pending_edit_.patch_id ||
                   (pending_edit_.param==edit::Count && (edit->request!=pending_edit_.token ||
                    backend_.editSnapshotGeneration()!=pending_edit_.generation)))){
                    pending_edit_.active = false;
                    result(UiCommandState::Rejected, edit_id_, "PATCH CHANGED; EDIT CANCELED");
                } else {
                    result(edit->errors==edit_errors_ && !backend_.editError()?UiCommandState::Confirmed:UiCommandState::Rejected,
                           edit_id_,backend_.editNotice(),pending_edit_.token);
                    pending_edit_.active = false;
                }
            }
            const char* ids[]={
                "osc.1.level","osc.sub_level","osc.noise_level","osc.1.pulse_width","osc.1.detune_ct",
                "amp.level","amp.pan","amp.vel_sens",
                "filter.cutoff_hz","filter.resonance","filter.env_amount","filter.keytrack",
                "env.amp.attack_ms","env.amp.release_ms","env.amp.decay_ms","env.amp.sustain",
                "fm.depth","fm.algo","fm.feedback","fm.op.level","fm.op.ratio","fm.op.attack","fm.op.decay","fm.op.vel_sens",
                "lfo.1.rate_hz","lfo.1.depth","lfo.1.wave","lfo.1.dest",
                "mod.slot1.enable","mod.slot1.source","mod.slot1.dest","mod.slot1.amount","mod.slot1.polarity"
            };
            for(const char* id:ids){
                const int p=editIndex(id);
                if(p>=0 && (edit->available & edit::bit(unsigned(p)))) model_.setFloat(id,edit->applied[p]);
            }
            model_.setInt("fm.selected_op",int(selected_op_)+1);
            char macro_id[]="perf.macro.1";
            for(unsigned m=0;m<performance::kMacroCount;++m){
                macro_id[11]=char('1'+m);
                model_.setInt(macro_id,edit->performance_macros[m]);
            }
        }
        if(const auto* fx=backend_.fxState()){
            if(fx_requested_){
                if(fx->errors > fx_initial_errors_ || fx->rejected > fx_initial_rejected_){
                    fx_requested_=false;
                    result(UiCommandState::Rejected,fx_id_,"FX REJECTED; CONFIRMED VALUES RESTORED");
                } else if(fx->request >= fx_pending_token_ && !backend_.busy()){
                    fx_requested_=false;
                    result(UiCommandState::Confirmed,fx_id_,"FX CONFIRMED BY OWNER");
                }
            }
            model_.setInt("fx.chorus.mode",fx->config.chorus_mode);
            model_.setFloat("fx.chorus.depth",fx->config.chorus_depth*100.0f);
            model_.setFloat("fx.delay.time_ms",fx->config.delay_time_ms);
            model_.setFloat("fx.delay.feedback",fx->config.delay_feedback*100.0f);
            model_.setFloat("fx.delay.mix",fx->config.delay_mix*100.0f);
            model_.setFloat("fx.reverb.size",fx->config.reverb_size*100.0f);
            model_.setFloat("fx.reverb.mix",fx->config.reverb_mix*100.0f);
            model_.setFloat("fx.master_tone",fx->config.master_tone*100.0f);
        }
        const auto& s=backend_.state();
        if(isPatchSelectable(s.patch_id)) {
            if(model_.getInt("perf.patch_id")!=s.patch_id) model_.setInt("perf.patch_id",s.patch_id);
            if(model_.getInt("layer_a.patch_id")!=s.patch_id) model_.setInt("layer_a.patch_id",s.patch_id);
        }
        if(s.master<=127 && model_.getInt("perf.master_volume")!=s.master) model_.setInt("perf.master_volume",s.master);
        if(patch_.active && backend_.patchError()) {
            patch_.active=false; result(UiCommandState::Rejected,"perf.patch_id","PATCH LOAD FAILED; RETRY SAFE PATCH");
        }
        if(patch_.active && !backend_.busy() && rev!=patch_.revision && s.patch_id==patch_.value) {
            patch_.active=false; result(UiCommandState::Confirmed,"perf.patch_id","PATCH CONFIRMED BY SNAPSHOT");
        }
        if(master_.active && rev!=master_.revision && s.master==master_.value) {
            master_.active=false; result(UiCommandState::Confirmed,"perf.master_volume","MASTER CONFIRMED BY SNAPSHOT");
        }
        auto& d=model_.diagnostics();
        if(d.active_voices!=s.active_notes || d.render_max_us!=s.max_render_us || d.deadline_misses!=s.deadline_misses) model_.bumpRevision();
        d.active_voices=static_cast<uint8_t>(s.active_notes); d.render_max_us=s.max_render_us; d.deadline_misses=s.deadline_misses;
    }
    if(patch_.active && uint32_t(now_ms-patch_.requested_ms)>=kPatchConfirmationTimeoutMs) {
        patch_.active=false;
        result(UiCommandState::Rejected,"perf.patch_id","PATCH NOT CONFIRMED; OWNER STATE RETAINED");
    }
    if(fx_requested_ && uint32_t(now_ms-fx_requested_ms_)>=kPatchConfirmationTimeoutMs) {
        fx_requested_=false;
        if(const auto* fx=backend_.fxState()){
            model_.setInt("fx.chorus.mode",fx->config.chorus_mode);
            model_.setFloat("fx.chorus.depth",fx->config.chorus_depth*100.0f);
            model_.setFloat("fx.delay.time_ms",fx->config.delay_time_ms);
            model_.setFloat("fx.delay.feedback",fx->config.delay_feedback*100.0f);
            model_.setFloat("fx.delay.mix",fx->config.delay_mix*100.0f);
            model_.setFloat("fx.reverb.size",fx->config.reverb_size*100.0f);
            model_.setFloat("fx.reverb.mix",fx->config.reverb_mix*100.0f);
            model_.setFloat("fx.master_tone",fx->config.master_tone*100.0f);
        }
        result(UiCommandState::Rejected,fx_id_,"FX NOT CONFIRMED; OWNER STATE RETAINED");
    }
    if(pending_edit_.active && pending_edit_.param==edit::FmAlgorithm){
        const auto& request=pending_edit_;
        const auto* edit=backend_.soundEdit();
        UiCommandState state=UiCommandState::Rejected;
        const char* notice=nullptr;
        if(backend_.editGeneration()!=request.generation || backend_.state().patch_id!=request.patch_id){
            notice="PATCH CHANGED; ALGORITHM CANCELED";
        }else if(uint32_t(now_ms-request.requested_ms)>=kPatchConfirmationTimeoutMs){
            notice="ALGORITHM CHANGE NOT CONFIRMED";
        }else if(backend_.connected() && edit && request.token && edit->request==request.token &&
                 edit->patch==request.patch_id && backend_.editSnapshotGeneration()==request.generation){
            if(edit->errors!=request.initial_errors){
                notice="ALGORITHM REJECTED: OWNER ERROR";
            }else if(edit->applied[edit::FmAlgorithm]==request.requested_value){
                state=UiCommandState::Confirmed;
                notice="ALGORITHM APPLIED";
            }
        }
        if(notice){
            pending_edit_.active=false;
            edit_requested_=false;
            fm_algorithm_result_=result(state,"fm.algo",notice,request.token);
        }
    }else if(pending_edit_.active &&
             uint32_t(now_ms-pending_edit_.requested_ms)>=kPatchConfirmationTimeoutMs){
        pending_edit_.active=false;
        edit_requested_=false;
        result(UiCommandState::Rejected,edit_id_,"EDIT NOT CONFIRMED; OWNER STATE RETAINED");
    }
    if(const auto* h=backend_.harmonyState()) {
        model_.live_harmony=*h;model_.live_harmony.pending=harmony_pending_;
        if(harmony_pending_ && h->revision!=harmony_revision_ && harmony::pack(h->configuration)==harmony::pack(requested_harmony_)) {
            harmony_pending_=false;model_.live_harmony.pending=false;
            syncHarmony(h->configuration);model_.setInt("harmony.preset",harmony_preset_);result(UiCommandState::Confirmed,harmony_id_,"HARMONY APPLIED; EXISTING GROUPS FROZEN");
        } else if(harmony_pending_ && uint32_t(now_ms-harmony_requested_ms_)>=kPatchConfirmationTimeoutMs) {
            harmony_pending_=false;model_.live_harmony.pending=false;
            result(UiCommandState::Rejected,harmony_id_,"HARMONY NOT CONFIRMED; OWNER RETAINED");
        } else if(!harmony_pending_)syncHarmony(h->configuration);
        model_.bumpRevision();
    }
    // MIDI publication does not advance stateRevision. Inspect flags every tick.
    auto& d=model_.diagnostics();
    if(d.max_voices!=backend_.voiceCapacity()) model_.bumpRevision();
    d.max_voices=backend_.voiceCapacity();
    const auto& s=backend_.state();
    const bool midi=(s.flags&link::kUsbConnected)!=0;
    const bool audio=backend_.connected() && (s.flags&link::kAudioLive);
    const char* usb_status=backend_.midiError()?"ERROR":midi?"CONNECTED":"DISCONNECTED";
    if(d.midi_connected!=midi || d.audio_live!=audio || std::strcmp(d.usb_status,usb_status)!=0 || std::strcmp(d.amy_status,backend_.status())!=0) model_.bumpRevision();
    d.midi_connected=midi; d.audio_live=audio;
    d.amy_status=backend_.status(); d.i2s_status=audio?"RUN":"NOT LIVE";
    d.usb_status=usb_status;
    d.uptime_ms=now_ms;
    serviceMidi();
}
}
