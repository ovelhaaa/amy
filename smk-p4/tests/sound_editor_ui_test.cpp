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
    bool read(unsigned,unsigned,uint8_t*,unsigned&) override {return false;}
    bool write(unsigned,unsigned,const uint8_t*,unsigned) override {return false;}
    edit::BankRead readVersioned(unsigned version,unsigned s,unsigned b,uint8_t* p,unsigned& n) override {
        if(version==1 || !present[s][b])return edit::BankRead::Missing;
        if(n<160)return edit::BankRead::Error;
        std::memcpy(p,bytes[s][b],160);n=160;return edit::BankRead::Present;
    }
    bool writeV2(unsigned s,unsigned b,const uint8_t* p,unsigned n) override {if(fail)return false;assert(n==160);std::memcpy(bytes[s][b],p,n);present[s][b]=true;return true;}
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
static amy::Runtime r;
int main(){
    preview::initHostLvgl();assert(r.begin());MemoryStore store;
    LocalAmyPerformanceBackend b(r.commands());b.presetStore(&store);UiBlueprintPerformanceAdapter a(b);
    auto publish=[&]{b.publish(r.snapshot(),false,0,0);a.service(preview::g_sim_tick_ms+=25);};
    auto apply=[&]{a.service(preview::g_sim_tick_ms+=25);r.apply(preview::g_sim_tick_ms);r.loadPendingPatch();assert(r.render());publish();};
    publish();UiBlueprint app(a);assert(app.begin());
    assert(a.requestSelectPatch(4).accepted());apply();
    assert(a.model().getFloat("filter.cutoff_hz")>3000);
    assert(a.parameterStatus("filter.cutoff_hz")==FeatureStatus::Available);
    assert(a.parameterStatus("fm.op.ratio")==FeatureStatus::Unavailable);
    assert(a.requestParameterFloat("filter.cutoff_hz",1000).state==UiCommandState::Requested);
    assert(a.model().getFloat("filter.cutoff_hz")!=1000);apply();
    assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getFloat("filter.cutoff_hz")==1000);
    for(const char* id:{"filter.env_amount","filter.keytrack"}){
        assert(a.parameterStatus(id)==FeatureStatus::Available);
        const float value=std::strcmp(id,"filter.env_amount")==0?3.0f:150.0f;
        assert(a.requestParameterFloat(id,value).state==UiCommandState::Requested);apply();
        assert(a.model().getFloat(id)==value && a.lastCommandState()==UiCommandState::Confirmed);
    }
    const char* component_ids[]={"osc.1.level","osc.sub_level","osc.noise_level","osc.1.pulse_width","osc.1.detune_ct","amp.level","amp.pan","amp.vel_sens"};
    const float component_values[]={35,15,15,30,35,50,-48,30};
    for(unsigned i=0;i<8;++i){
        if(a.parameterStatus(component_ids[i])!=FeatureStatus::Available){assert(!a.requestParameterFloat(component_ids[i],component_values[i]).accepted());continue;}
        assert(a.requestParameterFloat(component_ids[i],component_values[i]).state==UiCommandState::Requested);
        apply();assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getFloat(component_ids[i])==component_values[i]);
    }
    assert(!a.requestSelectPatch(135).accepted()); // no silent loss
    assert(a.requestParameterInt("perf.macro.2",32).accepted());apply();assert(a.model().getFloat("filter.cutoff_hz")==500);
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
    assert(a.requestSelectPatch(31).accepted());apply();
    assert(a.parameterStatus("osc.1.pulse_width")==FeatureStatus::Available);
    assert(a.requestParameterFloat("osc.1.pulse_width",30).state==UiCommandState::Requested);apply();
    assert(a.model().getFloat("osc.1.pulse_width")==30 && a.lastCommandState()==UiCommandState::Confirmed);
    assert(a.requestSoundAction(1).accepted());apply();
    assert(a.requestSelectPatch(135).accepted());apply();
    assert(a.requestParameterInt("fm.selected_op",1).accepted());
    assert(a.parameterStatus("filter.cutoff_hz")==FeatureStatus::Unavailable);
    assert(a.parameterStatus("filter.env_amount")==FeatureStatus::Unavailable && a.parameterStatus("filter.keytrack")==FeatureStatus::Unavailable);
    assert(a.parameterStatus("amp.level")==FeatureStatus::Available);
    assert(a.parameterStatus("amp.vel_sens")==FeatureStatus::Unavailable && a.parameterStatus("osc.sub_level")==FeatureStatus::Unavailable);
    assert(a.requestParameterFloat("fm.depth",0.7f).accepted());apply();assert(r.snapshot().edit.applied[edit::Depth]==0.7f);
    assert(a.requestSoundAction(1).accepted());apply();
    assert(a.requestSoundAction(3,0).accepted());apply();apply();assert(r.snapshot().patch_id==4 && r.snapshot().edit.applied[edit::Cutoff]==1000);
    // Persisted bytes survive an engine/backend lifecycle, no pointer or map retained.
    app.shutdown();r.end();assert(r.begin());LocalAmyPerformanceBackend reboot(r.commands());reboot.presetStore(&store);
    reboot.publish(r.snapshot(),false,0,0);assert(reboot.editAction(3,0));r.apply(0);r.loadPendingPatch();reboot.publish(r.snapshot(),false,0,0);reboot.service(25);r.apply(1);reboot.publish(r.snapshot(),false,0,0);
    assert(r.snapshot().edit.applied[0]==1000 && r.snapshot().edit.user_slot==0);
    assert(r.snapshot().edit.applied[edit::FilterEnv]==3 && r.snapshot().edit.applied[edit::FilterKeytrack]==150);
    assert(r.snapshot().edit.applied[edit::OscLevel]==35 && r.snapshot().edit.applied[edit::SubLevel]==15 && r.snapshot().edit.applied[edit::NoiseLevel]==15);
    assert(r.snapshot().edit.applied[edit::PatchLevel]==50 && r.snapshot().edit.applied[edit::Pan]==-48 && r.snapshot().edit.applied[edit::Velocity]==30);
    UiBlueprintPerformanceAdapter next(reboot);next.service(preview::g_sim_tick_ms+=25);UiBlueprint ui(next);assert(ui.begin());
    for(unsigned tab=0;tab<7;++tab){assert(ui.navigation().navigateTo(ScreenId::SoundEditor,uint8_t(tab)));preview::pumpFrame(ui);}
    assert(ui.navigation().navigateTo(ScreenId::SoundEditor,0));preview::pumpFrame(ui);
    assert(button(ui.contentObject(),"MAIN MIX"));
    auto* pulse_card=button(ui.contentObject(),"PULSE WIDTH");assert(pulse_card && labels(pulse_card,"N/A")>=2 && labels(pulse_card,"PLAN")==0);
    assert(preview::saveFramebufferPpm("build/editor011/osc.ppm"));
    assert(ui.navigation().navigateTo(ScreenId::SoundEditor,2));preview::pumpFrame(ui);
    assert(preview::saveFramebufferPpm("build/editor011/filter.ppm"));
    auto* open=button(ui.contentObject(),"USER / SAVE AS");assert(open);lv_obj_send_event(open,LV_EVENT_CLICKED,nullptr);preview::pumpFrame(ui);
    assert(preview::saveFramebufferPpm("build/editor011/presets.ppm"));
    auto* close=button(lv_screen_active(),"CLOSE");assert(close);lv_obj_send_event(close,LV_EVENT_CLICKED,nullptr);preview::pumpFrame(ui);
    assert(ui.navigation().navigateTo(ScreenId::PatchBrowser));preview::pumpFrame(ui);assert(button(ui.contentObject(),"USER PRESETS / SAVE AS"));
    assert(lv_mem_test()==LV_RESULT_OK);ui.shutdown();r.end();
    puts("PASS LVGL9.3: editor tabs, owner confirmation, coalesced macros, dirty guard, Save/Recall/reboot simulation, User panel, heap integrity");
}
