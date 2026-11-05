#include "ui_blueprint_performance_adapter.h"
#include <cmath>
#include <cstring>

namespace smk::p4 {
using namespace ui;
namespace {
bool eq(const char* a,const char* b) { return a && std::strcmp(a,b)==0; }
}
UiBlueprintPerformanceAdapter::UiBlueprintPerformanceAdapter(PerformanceBackend& backend):backend_(backend) {
    meters_.is_simulated=false; // No amplitude measurement available: meters stay zero/N/A.
    auto& d=model_.diagnostics();
    d={}; d.is_demo_mode=false; d.firmware_version="SMK-P4 LIBRARY 0.7"; d.max_voices=backend_.voiceCapacity();
    d.amy_status="WAIT SNAPSHOT"; d.i2s_status="NOT LIVE"; d.usb_status="DISCONNECTED";
    model_.setBool("harmony.scale_enable",false);
    model_.setBool("harmony.chord_enable",false);
    model_.setNotice("WAIT OWNER SNAPSHOT; TOUCH LEASE 10s");
}
UiCommandResult UiBlueprintPerformanceAdapter::result(UiCommandState state,const char* id,const char* notice,uint32_t token) {
    last_state_=state;
    // Retain canonical/static IDs, never a caller-owned temporary string.
    const auto* desc=ParameterRegistry::findById(id);
    last_id_=desc?desc->id:"command";
    model_.setNotice(notice); model_.bumpRevision();
    return {state,token,notice};
}
FeatureStatus UiBlueprintPerformanceAdapter::parameterStatus(const char* id) const {
    return eq(id,"perf.master_volume") || eq(id,"perf.octave_shift") || eq(id,"perf.patch_id")
        ? FeatureStatus::Available : FeatureStatus::Planned;
}
bool UiBlueprintPerformanceAdapter::isParameterEditable(const char* id) const {
    return parameterStatus(id)==FeatureStatus::Available && usable();
}
bool UiBlueprintPerformanceAdapter::isPatchSelectable(uint16_t id) const {
    size_t count=0; const auto* entries=UiModel::catalog(count);
    bool represented=false;
    for(size_t i=0;i<count;++i) if(entries[i].id==id) represented=true;
    if(!represented) return false;
    for(uint16_t i=0;i<backend_.patchCount();++i) if(backend_.patchAt(i)==id) {
        PatchInfo info;
        return backend_.patchInfo(i,info,now_ms_) && info.id==id && info.available;
    }
    return false;
}
FeatureStatus UiBlueprintPerformanceAdapter::patchStatus(uint16_t id) const {
    return isPatchSelectable(id)?FeatureStatus::Available:FeatureStatus::Planned;
}
UiCommandResult UiBlueprintPerformanceAdapter::requestParameterValue(const char* id,const ParameterValue& v) {
    if(v.type==ParameterValueType::String) return result(UiCommandState::Rejected,id,"INVALID VALUE TYPE");
    return requestParameterFloat(id,v.asFloat());
}
UiCommandResult UiBlueprintPerformanceAdapter::requestParameterFloat(const char* id,float v) {
    if(!std::isfinite(v) || v < -32768 || v > 65535) return result(UiCommandState::Rejected,id,"INVALID VALUE");
    return requestParameterInt(id,static_cast<int32_t>(std::lround(v)));
}
UiCommandResult UiBlueprintPerformanceAdapter::requestParameterBool(const char* id,bool v) {
    return requestParameterInt(id,v?1:0);
}
UiCommandResult UiBlueprintPerformanceAdapter::requestParameterInt(const char* id,int32_t v) {
    if(parameterStatus(id)!=FeatureStatus::Available) return result(UiCommandState::Unsupported,id,"PLANNED; NO DSP COMMAND");
    if(!usable()) return result(UiCommandState::Rejected,id,"ENGINE/SNAPSHOT UNAVAILABLE");
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
        const auto& s=backend_.state();
        if(isPatchSelectable(s.patch_id)) {
            if(model_.getInt("perf.patch_id")!=s.patch_id) model_.setInt("perf.patch_id",s.patch_id);
            if(model_.getInt("layer_a.patch_id")!=s.patch_id) model_.setInt("layer_a.patch_id",s.patch_id);
        }
        if(s.master<=127 && model_.getInt("perf.master_volume")!=s.master) model_.setInt("perf.master_volume",s.master);
        if(patch_.active && rev!=patch_.revision && s.patch_id==patch_.value) {
            patch_.active=false; result(UiCommandState::Confirmed,"perf.patch_id","PATCH CONFIRMED BY SNAPSHOT");
        }
        if(master_.active && rev!=master_.revision && s.master==master_.value) {
            master_.active=false; result(UiCommandState::Confirmed,"perf.master_volume","MASTER CONFIRMED BY SNAPSHOT");
        }
        auto& d=model_.diagnostics();
        if(d.active_voices!=s.active_notes || d.render_max_us!=s.max_render_us || d.deadline_misses!=s.deadline_misses) model_.bumpRevision();
        d.active_voices=static_cast<uint8_t>(s.active_notes); d.render_max_us=s.max_render_us; d.deadline_misses=s.deadline_misses;
    }
    if(patch_.active && uint32_t(now_ms-patch_.requested_ms)>=2000) {
        patch_.active=false;
        result(UiCommandState::Rejected,"perf.patch_id","PATCH NOT CONFIRMED; OWNER STATE RETAINED");
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
}
}
