#include "fake_nvs.h"
#include "user_preset_fx_nvs.h"
#include "ui_blueprint_performance_adapter.h"
#include "local_performance_backend.h"
#include "user_presets_v2.h"
#include "../ui-preview/preview_harness.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
using namespace smk::p4;
using namespace smk::p4::ui;
struct MemoryStore:edit::VersionedPresetStore {
    uint8_t bytes[16][2][160]{};bool present[16][2]{};bool fail=false;
    uint8_t v3[16][2][edit::kV3RecordBytes]{};bool has_v3[16][2]{};
    bool read(unsigned,unsigned,uint8_t*,unsigned&) override {return false;}
    bool write(unsigned,unsigned,const uint8_t*,unsigned) override {return false;}
    edit::BankRead readVersioned(unsigned version,unsigned s,unsigned b,uint8_t* p,unsigned& n) override {
        if(version==3){
            if(!has_v3[s][b])return edit::BankRead::Missing;
            if(n<sizeof(v3[s][b]))return edit::BankRead::Error;
            n=sizeof(v3[s][b]);std::memcpy(p,v3[s][b],n);return edit::BankRead::Present;
        }
        if(version==1 || !present[s][b])return edit::BankRead::Missing;
        if(n<160)return edit::BankRead::Error;
        std::memcpy(p,bytes[s][b],160);n=160;return edit::BankRead::Present;
    }
    bool writeV2(unsigned s,unsigned b,const uint8_t* p,unsigned n) override {if(fail)return false;assert(n==160);std::memcpy(bytes[s][b],p,n);present[s][b]=true;return true;}
    bool writeV3(unsigned s,unsigned b,const uint8_t* p,unsigned n) override {
        if(fail || n!=sizeof(v3[s][b]))return false;
        std::memcpy(v3[s][b],p,n);has_v3[s][b]=true;return true;
    }
};
struct ProfileStore:performance::ControllerProfileStore {
    uint8_t bytes[performance::kProfileBytes]{};
    bool load(performance::ControllerProfile& profile) override {
        return performance::decodeProfile(bytes,sizeof(bytes),profile);
    }
    bool save(const performance::ControllerProfile& profile) override {
        return performance::encodeProfile(profile,bytes);
    }
};
static lv_obj_t* button(lv_obj_t* root,const char* text){
    for(unsigned i=0;i<lv_obj_get_child_count(root);++i){auto* child=lv_obj_get_child(root,i);
        if(lv_obj_check_type(child,&lv_label_class) && std::strcmp(lv_label_get_text(child),text)==0)return lv_obj_get_parent(child);
        if(auto* found=button(child,text))return found;
    }return nullptr;
}
static unsigned labels(lv_obj_t* root,const char* text){
    unsigned n=lv_obj_check_type(root,&lv_label_class) && std::strcmp(lv_label_get_text(root),text)==0?1:0;
    for(unsigned i=0;i<lv_obj_get_child_count(root);++i)n+=labels(lv_obj_get_child(root,i),text);
    return n;
}
template <typename ApplyFn>
static void auditInventory(UiBlueprintPerformanceAdapter& a, ApplyFn&& apply) {
    size_t count = 0;
    const ParameterDescriptor* all = ParameterRegistry::all(count);
    assert(count >= 76);
    for (size_t i = 0; i < count; ++i) {
        const auto& d = all[i];
        const FeatureStatus st = a.parameterStatus(d.id);
        assert(st != FeatureStatus::Demo); // Real adapter never reports Demo status
        if (st == FeatureStatus::Available) {
            if (d.capabilities & kCapReadOnly) {
                assert(!a.isParameterEditable(d.id));
                continue;
            }
            assert(a.isParameterEditable(d.id));
            ParameterValue pv{};
            assert(a.model().getValue(d.id, pv));
            const UiCommandResult res = a.requestParameterValue(d.id, pv);
            assert(res.state == UiCommandState::Requested || res.state == UiCommandState::Confirmed);
            apply();
            assert(a.lastCommandState() == UiCommandState::Confirmed);
        } else {
            assert(!a.isParameterEditable(d.id));
            ParameterValue pv{};
            assert(a.model().getValue(d.id, pv));
            const UiCommandResult res = a.requestParameterValue(d.id, pv);
            assert(!res.accepted());
            if (st == FeatureStatus::Planned) {
                assert(res.state == UiCommandState::Unsupported || res.state == UiCommandState::Rejected);
            }
        }
    }
}
static amy::Runtime r;
int main(){
    preview::initHostLvgl();assert(r.begin());MemoryStore store;
    fx::NvsFxPresets fx_store;assert(fx_store.begin());
    LocalAmyPerformanceBackend b(r.commands());b.presetStore(&store);b.fxPresetStore(&fx_store);UiBlueprintPerformanceAdapter a(b);
    auto publish=[&]{b.publish(r.snapshot(),false,0,0);a.service(preview::g_sim_tick_ms+=25);};
    auto apply=[&]{a.service(preview::g_sim_tick_ms+=25);r.apply(preview::g_sim_tick_ms);r.loadPendingPatch();assert(r.render());publish();};
    publish();UiBlueprint app(a);assert(app.begin());

    // 1. Juno patch 4: Gate A, B, C, E parameters + Gate G0 inventory audit
    assert(a.requestSelectPatch(4).accepted());apply();
    auditInventory(a, apply);
    assert(a.model().getFloat("filter.cutoff_hz")>3000);
    assert(a.parameterStatus("filter.cutoff_hz")==FeatureStatus::Available);
    assert(a.parameterStatus("fm.op.ratio")==FeatureStatus::Unavailable);
    assert(a.parameterStatus("fm.feedback")==FeatureStatus::Unavailable);
    assert(a.parameterStatus("fm.op.attack")==FeatureStatus::Unavailable);
    assert(a.parameterStatus("fm.op.decay")==FeatureStatus::Unavailable);
    assert(a.parameterStatus("fm.op.vel_sens")==FeatureStatus::Unavailable);
    assert(a.requestParameterFloat("filter.cutoff_hz",1000).state==UiCommandState::Requested);
    assert(a.model().getFloat("filter.cutoff_hz")!=1000);apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getFloat("filter.cutoff_hz")==1000);
    for(const char* id:{"filter.env_amount","filter.keytrack"}){
        assert(a.parameterStatus(id)==FeatureStatus::Available);
        const float value=std::strcmp(id,"filter.env_amount")==0?3.0f:150.0f;
        assert(a.requestParameterFloat(id,value).state==UiCommandState::Requested);apply();
        assert(a.model().getFloat(id)==value && a.lastCommandState()==UiCommandState::Confirmed);
    }
    for(const char* id:{"env.amp.attack_ms","env.amp.decay_ms","env.amp.sustain","env.amp.release_ms"}){
        assert(a.parameterStatus(id)==FeatureStatus::Available);
    }
    assert(a.parameterStatus("env.amp.curve")==FeatureStatus::Planned);
    const char* component_ids[]={"osc.1.level","osc.sub_level","osc.noise_level","osc.1.pulse_width","osc.1.detune_ct","amp.level","amp.pan","amp.vel_sens"};
    const float component_values[]={35,15,15,30,35,50,-48,30};
    for(unsigned i=0;i<8;++i){
        if(a.parameterStatus(component_ids[i])!=FeatureStatus::Available){assert(!a.requestParameterFloat(component_ids[i],component_values[i]).accepted());continue;}
        assert(a.requestParameterFloat(component_ids[i],component_values[i]).state==UiCommandState::Requested);
        apply();assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getFloat(component_ids[i])==component_values[i]);
    }
    // Gate E LFO & Mod Matrix Slot 1 on Juno
    for(const char* id:{"lfo.1.wave","lfo.1.rate_hz","lfo.1.depth","lfo.1.dest","mod.slot1.enable","mod.slot1.source","mod.slot1.dest","mod.slot1.amount","mod.slot1.polarity"}){
        assert(a.parameterStatus(id)==FeatureStatus::Available);
    }
    assert(a.parameterStatus("lfo.1.phase_deg")==FeatureStatus::Planned && a.parameterStatus("lfo.1.sync")==FeatureStatus::Planned);
    assert(a.isParameterValueSupported("lfo.1.dest",2) && !a.isParameterValueSupported("lfo.1.dest",4));
    assert(a.isParameterValueSupported("mod.slot1.dest",3) && !a.isParameterValueSupported("mod.slot1.dest",2));
    assert(a.requestParameterFloat("lfo.1.rate_hz",6.5f).state==UiCommandState::Requested);apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getFloat("lfo.1.rate_hz")==6.5f);
    assert(a.requestParameterFloat("lfo.1.depth",45.0f).state==UiCommandState::Requested);apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getFloat("lfo.1.depth")==45.0f);
    assert(a.requestParameterInt("lfo.1.wave",1).state==UiCommandState::Requested);apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getInt("lfo.1.wave")==1);
    assert(a.requestParameterInt("lfo.1.dest",2).state==UiCommandState::Requested);apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getInt("lfo.1.dest")==2);
    assert(!a.requestParameterInt("lfo.1.dest",4).accepted());
    assert(a.requestParameterInt("mod.slot1.dest",3).state==UiCommandState::Requested);apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getInt("mod.slot1.dest")==3 && a.model().modSlot(0).dest==3);
    assert(a.requestParameterFloat("mod.slot1.amount",60.0f).state==UiCommandState::Requested);apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getFloat("mod.slot1.amount")==60.0f && a.model().modSlot(0).amount==60);
    assert(a.requestParameterInt("mod.slot1.polarity",0).state==UiCommandState::Requested);apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getInt("mod.slot1.polarity")==0 && a.model().modSlot(0).polarity==0);

    assert(!a.requestSelectPatch(135).accepted()); // dirty guard: no silent loss
    assert(a.requestParameterInt("perf.macro.2",32).accepted());
    assert(a.model().getInt("perf.macro.2")==64);
    apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getInt("perf.macro.2")==32);
    assert(a.model().getFloat("filter.cutoff_hz")==125);
    assert(a.requestParameterInt("perf.macro.2",64).accepted());apply();assert(a.model().getFloat("filter.cutoff_hz")==1000);
    assert(a.requestSoundAction(0).accepted());apply();assert(r.snapshot().edit.compare);
    assert(a.requestSoundAction(0).accepted());apply();assert(a.model().getFloat("filter.cutoff_hz")==1000);
    assert(!a.requestParameterFloat("filter.resonance",8).accepted());
    assert(a.requestSoundAction(2,0,"MY STRINGS 01").accepted());
    r.apply(preview::g_sim_tick_ms);assert(r.snapshot().storage_quiet);publish(); // writes in UI service
    r.apply(preview::g_sim_tick_ms);publish();assert(!b.busy());
    char name[25];uint16_t origin;assert(b.userPreset(0,name,sizeof(name),origin));assert(origin==4 && !std::strcmp(name,"MY STRINGS 01"));
    store.fail=true;assert(a.requestSoundAction(2,0,"MY STRINGS 01").accepted());
    r.apply(preview::g_sim_tick_ms);publish();r.apply(preview::g_sim_tick_ms);publish();
    assert(a.lastCommandState()==UiCommandState::Rejected);store.fail=false;

    // 2. Juno patch 31: contextual Pulse Width
    assert(a.requestSelectPatch(31).accepted());apply();
    assert(a.parameterStatus("osc.1.pulse_width")==FeatureStatus::Available);
    assert(a.requestParameterFloat("osc.1.pulse_width",30).state==UiCommandState::Requested);apply();
    assert(a.model().getFloat("osc.1.pulse_width")==30 && a.lastCommandState()==UiCommandState::Confirmed);
    assert(a.requestSoundAction(1).accepted());apply();

    // 3. DX7 patch 135: Gate D advanced FM + Gate E contextual LFO/Mod + Gate G0 inventory audit
    assert(a.requestSelectPatch(135).accepted());apply();
    auditInventory(a, apply);
    assert(a.requestParameterInt("fm.selected_op",1).accepted());
    assert(a.parameterStatus("filter.cutoff_hz")==FeatureStatus::Unavailable);
    assert(a.parameterStatus("filter.env_amount")==FeatureStatus::Unavailable && a.parameterStatus("filter.keytrack")==FeatureStatus::Unavailable);
    assert(a.parameterStatus("amp.level")==FeatureStatus::Available);
    assert(a.parameterStatus("amp.vel_sens")==FeatureStatus::Unavailable && a.parameterStatus("osc.sub_level")==FeatureStatus::Unavailable);
    for(const char* id:{"fm.depth","fm.algo","fm.feedback","fm.selected_op","fm.op.level","fm.op.ratio","fm.op.attack","fm.op.decay","fm.op.vel_sens"}){
        assert(a.parameterStatus(id)==FeatureStatus::Available);
    }
    assert(a.requestParameterInt("fm.algo",5).accepted());apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getInt("fm.algo")==5 && r.snapshot().edit.applied[edit::FmAlgorithm]==5.0f);
    assert(!a.isParameterValueSupported("lfo.1.dest",2) && a.isParameterValueSupported("lfo.1.dest",3));
    assert(!a.isParameterValueSupported("mod.slot1.dest",3) && a.isParameterValueSupported("mod.slot1.dest",1));
    assert(!a.requestParameterInt("lfo.1.dest",2).accepted());
    assert(!a.requestParameterInt("mod.slot1.dest",3).accepted());
    assert(a.requestParameterFloat("fm.depth",0.7f).accepted());apply();assert(r.snapshot().edit.applied[edit::Depth]==0.7f);
    assert(a.requestParameterFloat("fm.feedback",65.0f).accepted());apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getFloat("fm.feedback")==65.0f);
    assert(a.requestParameterInt("fm.selected_op",1).accepted());
    assert(a.requestParameterFloat("fm.op.attack",120.0f).accepted());apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getFloat("fm.op.attack")==120.0f && r.snapshot().edit.applied[edit::OpAttack0]==120.0f);
    assert(a.requestParameterFloat("fm.op.decay",850.0f).accepted());apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getFloat("fm.op.decay")==850.0f && r.snapshot().edit.applied[edit::OpDecay0]==850.0f);
    assert(a.requestParameterFloat("fm.op.vel_sens",75.0f).accepted());apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getFloat("fm.op.vel_sens")==75.0f && r.snapshot().edit.applied[edit::OpVel0]==75.0f);
    // Verify per-operator independence and contextual N/A when OP2 decay exceeds simplified bounds
    assert(a.requestParameterInt("fm.selected_op",2).accepted());
    assert(a.parameterStatus("fm.op.attack")==FeatureStatus::Available && a.parameterStatus("fm.op.vel_sens")==FeatureStatus::Available);
    assert(a.parameterStatus("fm.op.decay")==FeatureStatus::Unavailable && !a.requestParameterFloat("fm.op.decay",500.0f).accepted());
    assert(a.model().getFloat("fm.op.attack")!=120.0f); // OP2 attack was not overwritten by OP1 edit
    assert(a.requestSoundAction(1).accepted());apply();

    // 4. DX7 patch 153: fixed-frequency operator ratio contextual N/A vs ratio-mode AVAIL
    assert(a.requestSelectPatch(153).accepted());apply();
    unsigned na_ops=0, avail_ops=0;
    for(int op=1;op<=6;++op){
        assert(a.requestParameterInt("fm.selected_op",op).accepted());
        if(a.parameterStatus("fm.op.ratio")==FeatureStatus::Unavailable)++na_ops;
        else if(a.parameterStatus("fm.op.ratio")==FeatureStatus::Available)++avail_ops;
    }
    assert(na_ops>=1 && avail_ops>=1);
    assert(a.requestParameterInt("fm.selected_op",1).accepted());

    // 5. Recall slot 0 & reboot persistence across engine/backend lifecycle
    assert(a.requestSoundAction(3,0).accepted());apply();apply();assert(r.snapshot().patch_id==4 && r.snapshot().edit.applied[edit::Cutoff]==1000);
    app.shutdown();r.end();assert(r.begin());LocalAmyPerformanceBackend reboot(r.commands());reboot.presetStore(&store);reboot.fxPresetStore(&fx_store);
    reboot.publish(r.snapshot(),false,0,0);assert(reboot.editAction(3,0));r.apply(0);r.loadPendingPatch();reboot.publish(r.snapshot(),false,0,0);reboot.service(25);r.apply(1);reboot.publish(r.snapshot(),false,0,0);
    assert(r.snapshot().edit.applied[0]==1000 && r.snapshot().edit.user_slot==0);
    assert(r.snapshot().edit.applied[edit::FilterEnv]==3 && r.snapshot().edit.applied[edit::FilterKeytrack]==150);
    assert(r.snapshot().edit.applied[edit::OscLevel]==35 && r.snapshot().edit.applied[edit::SubLevel]==15 && r.snapshot().edit.applied[edit::NoiseLevel]==15);
    assert(r.snapshot().edit.applied[edit::PatchLevel]==50 && r.snapshot().edit.applied[edit::Pan]==-48 && r.snapshot().edit.applied[edit::Velocity]==30);
    assert(r.snapshot().edit.applied[edit::LfoRate]==6.5f && r.snapshot().edit.applied[edit::LfoDepth]==45.0f && r.snapshot().edit.applied[edit::LfoWave]==1.0f && r.snapshot().edit.applied[edit::LfoDest]==2.0f);
    assert(r.snapshot().edit.applied[edit::ModDest]==3.0f && r.snapshot().edit.applied[edit::ModAmount]==60.0f && r.snapshot().edit.applied[edit::ModPolarity]==0.0f);

    // 6. All 7 SoundEditor tabs UI verification & screenshot capture
    UiBlueprintPerformanceAdapter next(reboot);next.service(preview::g_sim_tick_ms+=25);UiBlueprint ui(next);assert(ui.begin());
    const char* tab_shots[7]={
        "build/editor011/osc.ppm","build/editor011/fm.ppm","build/editor011/filter.ppm",
        "build/editor011/amp.ppm","build/editor011/env.ppm","build/editor011/lfo.ppm","build/editor011/mod.ppm"
    };
    for(unsigned tab=0;tab<7;++tab){
        assert(ui.navigation().navigateTo(ScreenId::SoundEditor,uint8_t(tab)));
        preview::pumpFrame(ui);
        assert(preview::saveFramebufferPpm(tab_shots[tab]));
    }
    // Check OSC tab contextual badges
    assert(ui.navigation().navigateTo(ScreenId::SoundEditor,0));preview::pumpFrame(ui);
    assert(button(ui.contentObject(),"MAIN MIX"));
    auto* pulse_card=button(ui.contentObject(),"PULSE WIDTH");assert(pulse_card && labels(pulse_card,"N/A")>=2 && labels(pulse_card,"PLAN")==0);
    // Check FM tab shows both FM BRIGHTNESS and FM FEEDBACK and FM ALGORITHM
    assert(ui.navigation().navigateTo(ScreenId::SoundEditor,1));preview::pumpFrame(ui);
    assert(button(ui.contentObject(),"FM BRIGHTNESS") && button(ui.contentObject(),"FM FEEDBACK"));
    auto* algo_card=button(ui.contentObject(),"FM ALGORITHM");assert(algo_card);
    auto* op1_btn=button(ui.contentObject(),"OP 1");assert(op1_btn);
    auto* op3_btn=button(ui.contentObject(),"OP 3");assert(op3_btn);
    // Switch to DX7 FM patch 135 to test live interactive FM topology and operator switching
    auto next_apply=[&]{next.service(preview::g_sim_tick_ms+=25);r.apply(preview::g_sim_tick_ms);r.loadPendingPatch();assert(r.render());reboot.publish(r.snapshot(),false,0,0);next.service(preview::g_sim_tick_ms+=25);preview::pumpFrame(ui);};
    assert(next.requestSelectPatch(135).accepted());next_apply();
    assert(ui.navigation().navigateTo(ScreenId::SoundEditor,1));preview::pumpFrame(ui);
    auto* live_op3=button(ui.contentObject(),"OP 3");assert(live_op3 && !lv_obj_has_state(live_op3,LV_STATE_DISABLED));
    lv_obj_send_event(live_op3,LV_EVENT_CLICKED,nullptr);next_apply();
    assert(next.model().getInt("fm.selected_op",1)==3);
    // Check MOD tab Slot 1 interactive button click -> owner confirmation
    assert(ui.navigation().navigateTo(ScreenId::SoundEditor,6));preview::pumpFrame(ui);
    auto* m1_btn=button(ui.contentObject(),"M1 ON");assert(m1_btn && !lv_obj_has_state(m1_btn,LV_STATE_DISABLED));
    auto* m2_btn=button(ui.contentObject(),"M2 ON");assert(m2_btn && lv_obj_has_state(m2_btn,LV_STATE_DISABLED));
    lv_obj_send_event(m1_btn,LV_EVENT_CLICKED,nullptr);
    assert(next.lastCommandState()==UiCommandState::Requested && next.model().modSlot(0).enabled); // still ON until owner confirms
    next.service(preview::g_sim_tick_ms+=25);r.apply(preview::g_sim_tick_ms);reboot.publish(r.snapshot(),false,0,0);preview::pumpFrame(ui);
    assert(next.lastCommandState()==UiCommandState::Confirmed && !next.model().modSlot(0).enabled);
    assert(button(ui.contentObject(),"M1 OFF"));

    // Check User Presets modal
    assert(ui.navigation().navigateTo(ScreenId::SoundEditor,2));preview::pumpFrame(ui);
    auto* open=button(ui.contentObject(),"USER / SAVE AS");assert(open);lv_obj_send_event(open,LV_EVENT_CLICKED,nullptr);preview::pumpFrame(ui);
    assert(preview::saveFramebufferPpm("build/editor011/presets.ppm"));
    auto* close=button(lv_screen_active(),"CLOSE");assert(close);
    const auto screen_children=lv_obj_get_child_count(lv_screen_active());
    lv_obj_send_event(open,LV_EVENT_CLICKED,nullptr);
    assert(lv_obj_get_child_count(lv_screen_active())==screen_children);
    auto* panel=lv_obj_get_parent(close);
    auto* save_as=button(panel,"SAVE AS");assert(save_as);
    const auto before_overwrite=next.presetOperationStatus().revision;
    lv_obj_send_event(save_as,LV_EVENT_CLICKED,nullptr);
    preview::g_sim_tick_ms+=10000;
    preview::pumpFrame(ui);
    lv_obj_send_event(save_as,LV_EVENT_CLICKED,nullptr);
    assert(next.presetOperationStatus().revision==before_overwrite);

    edit::Preset replacement{};
    fx::FxConfig replacement_fx{};
    assert(reboot.transactionCoordinator().load(0,replacement,replacement_fx));
    assert(reboot.transactionCoordinator().save(0,replacement,replacement_fx).status==preset::SaveResult::Status::Success);
    lv_obj_send_event(save_as,LV_EVENT_CLICKED,nullptr);
    assert(next.presetOperationStatus().revision==before_overwrite);
    lv_obj_send_event(close,LV_EVENT_CLICKED,nullptr);preview::pumpFrame(ui);
    assert(ui.navigation().navigateTo(ScreenId::PatchBrowser));preview::pumpFrame(ui);assert(button(ui.contentObject(),"USER PRESETS / SAVE AS"));
    auto* browser_open=button(ui.contentObject(),"USER PRESETS / SAVE AS");
    assert(browser_open);
    lv_obj_send_event(browser_open,LV_EVENT_CLICKED,nullptr);
    preview::pumpFrame(ui);
    auto* browser_close=button(lv_screen_active(),"CLOSE");
    assert(browser_close);
    lv_obj_send_event(browser_close,LV_EVENT_CLICKED,nullptr);
    preview::pumpFrame(ui);
    // 7. Effects Screen UI verification, interactive parameter edit & screenshot capture
    assert(ui.navigation().navigateTo(ScreenId::Effects));preview::pumpFrame(ui);
    assert(preview::saveFramebufferPpm("build/editor011/effects.ppm"));
    assert(button(ui.contentObject(),"CHORUS MODE") && button(ui.contentObject(),"CHORUS DEPTH"));
    assert(button(ui.contentObject(),"DELAY TIME") && button(ui.contentObject(),"DELAY FDBK") && button(ui.contentObject(),"DELAY MIX"));
    assert(button(ui.contentObject(),"REVERB SIZE") && button(ui.contentObject(),"REVERB MIX"));
    assert(button(ui.contentObject(),"TILT TONE"));
    auto* drive_card=button(ui.contentObject(),"BUS DRIVE");assert(drive_card && labels(drive_card,"PLAN")>=1);
    auto* routing_card=button(ui.contentObject(),"FX CHAIN");assert(routing_card && labels(routing_card,"PLAN")>=1);

    // Interactive FX edits with owner round-trip confirmation
    assert(next.requestParameterInt("fx.chorus.mode",2).accepted());
    next.service(preview::g_sim_tick_ms+=25);r.apply(preview::g_sim_tick_ms);reboot.publish(r.snapshot(),false,0,0);next.service(preview::g_sim_tick_ms+=25);preview::pumpFrame(ui);
    assert(next.lastCommandState()==UiCommandState::Confirmed && r.snapshot().fx.config.chorus_mode==2);

    assert(next.requestParameterFloat("fx.delay.time_ms",500.0f).accepted());
    next.service(preview::g_sim_tick_ms+=25);r.apply(preview::g_sim_tick_ms);reboot.publish(r.snapshot(),false,0,0);next.service(preview::g_sim_tick_ms+=25);preview::pumpFrame(ui);
    assert(next.lastCommandState()==UiCommandState::Confirmed && r.snapshot().fx.config.delay_time_ms==500.0f);

    assert(next.requestParameterFloat("fx.reverb.mix",50.0f).accepted());
    next.service(preview::g_sim_tick_ms+=25);r.apply(preview::g_sim_tick_ms);reboot.publish(r.snapshot(),false,0,0);next.service(preview::g_sim_tick_ms+=25);preview::pumpFrame(ui);
    assert(next.lastCommandState()==UiCommandState::Confirmed && std::fabs(r.snapshot().fx.config.reverb_mix-0.5f)<0.01f);

    assert(next.requestParameterFloat("fx.master_tone",30.0f).accepted());
    next.service(preview::g_sim_tick_ms+=25);r.apply(preview::g_sim_tick_ms);reboot.publish(r.snapshot(),false,0,0);next.service(preview::g_sim_tick_ms+=25);preview::pumpFrame(ui);
    assert(next.lastCommandState()==UiCommandState::Confirmed && std::fabs(r.snapshot().fx.config.master_tone-0.3f)<0.01f);

    // The real USB packet parser, UI learn actions and audio owner in one loop.
    using MidiAction=IUiBackend::MidiAction;
    r.usbInput().enableControls();
    next.setMidiInput(r.usbInput());
    ProfileStore profile_store;
    next.setControllerProfileStore(profile_store);
    reboot.publishMidi(true,false,0);
    const auto service=[&]{
        next.service(preview::g_sim_tick_ms+=25);
        r.apply(preview::g_sim_tick_ms);
        r.loadPendingPatch();
        reboot.publish(r.snapshot(),false,0,0);
        next.service(preview::g_sim_tick_ms+=25);
    };
    service();
    assert(ui.navigation().navigateTo(ScreenId::MidiSettings,1));preview::pumpFrame(ui);
    assert(button(ui.contentObject(),"SAVE PROFILE"));
    next.midiPerformanceAction(MidiAction::Learn,6);
    const auto cc=[&](uint8_t value){
        const uint8_t packet[]{0x0b,0xb0,74,value};
        assert(r.usbInput().transfer(packet,sizeof(packet),preview::g_sim_tick_ms));
        service();
    };
    cc(20);
    assert(std::strstr(next.midiLearnStatus(),"CC74"));
    next.midiPerformanceAction(MidiAction::Confirm,0);
    const auto target=r.snapshot().edit.performance_macros[6];
    cc(10);cc(80);
    assert(r.snapshot().edit.performance_macros[6]==target);
    cc(81);service();
    assert(r.snapshot().edit.performance_macros[6]==81);
    next.midiPerformanceAction(MidiAction::SaveProfile,0);
    service();service();
    performance::ControllerProfile stored_profile;
    assert(profile_store.load(stored_profile) && stored_profile.knobs[6].controller==74);
    assert(std::strcmp(next.midiLearnStatus(),"PROFILE SAVED")==0);
    const auto positions=r.snapshot().edit.performance_macros;
    assert(next.requestSoundAction(2,15,"PERFORMANCE V3").accepted());
    service();service();
    edit::Preset saved_performance;
    fx::FxConfig saved_manual_fx;
    assert(reboot.transactionCoordinator().load(15,saved_performance,saved_manual_fx));
    assert(saved_performance.performance_macros==positions);
    assert(next.requestSoundAction(3,15).accepted());
    for(unsigned i=0;i<5;++i)service();
    assert(r.snapshot().edit.performance_macros==positions);
    assert(next.presetOperationStatus().state==ui::PresetOperationState::Confirmed);

    assert(lv_mem_test()==LV_RESULT_OK);ui.shutdown();r.end();
    puts("PASS LVGL9.3: all 7 editor tabs, Effects screen, Gate G0 inventory audit, FM/LFO/Mod/FX owner confirmation, dirty guard, Save/Recall/reboot simulation, heap integrity");
}
