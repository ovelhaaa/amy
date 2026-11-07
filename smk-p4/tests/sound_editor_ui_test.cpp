#include "ui_blueprint_performance_adapter.h"
#include "local_performance_backend.h"
#include "../ui-preview/preview_harness.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace smk::p4;
using namespace smk::p4::ui;
struct MemoryStore:edit::PresetStore {
    uint8_t bytes[16][2][128]{};bool present[16][2]{};
    bool read(unsigned s,unsigned b,uint8_t* p,unsigned& n) override {if(!present[s][b] || n<128)return false;std::memcpy(p,bytes[s][b],128);n=128;return true;}
    bool write(unsigned s,unsigned b,const uint8_t* p,unsigned n) override {assert(n==128);std::memcpy(bytes[s][b],p,n);present[s][b]=true;return true;}
};
static lv_obj_t* button(lv_obj_t* root,const char* text){
    for(unsigned i=0;i<lv_obj_get_child_count(root);++i){auto* child=lv_obj_get_child(root,i);
        if(lv_obj_check_type(child,&lv_label_class) && std::strcmp(lv_label_get_text(child),text)==0)return lv_obj_get_parent(child);
        if(auto* found=button(child,text))return found;
    }return nullptr;
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
    assert(a.requestSelectPatch(135).accepted());apply();
    assert(a.requestParameterInt("fm.selected_op",1).accepted());
    assert(a.parameterStatus("filter.cutoff_hz")==FeatureStatus::Unavailable);
    assert(a.requestParameterFloat("fm.depth",0.7f).accepted());apply();assert(r.snapshot().edit.applied[edit::Depth]==0.7f);
    assert(a.requestSoundAction(1).accepted());apply();
    assert(a.requestSoundAction(3,0).accepted());apply();apply();assert(r.snapshot().patch_id==4 && r.snapshot().edit.applied[edit::Cutoff]==1000);
    // Persisted bytes survive an engine/backend lifecycle, no pointer or map retained.
    app.shutdown();r.end();assert(r.begin());LocalAmyPerformanceBackend reboot(r.commands());reboot.presetStore(&store);
    reboot.publish(r.snapshot(),false,0,0);assert(reboot.editAction(3,0));r.apply(0);r.loadPendingPatch();reboot.publish(r.snapshot(),false,0,0);reboot.service(25);r.apply(1);reboot.publish(r.snapshot(),false,0,0);
    assert(r.snapshot().edit.applied[0]==1000 && r.snapshot().edit.user_slot==0);
    UiBlueprintPerformanceAdapter next(reboot);next.service(preview::g_sim_tick_ms+=25);UiBlueprint ui(next);assert(ui.begin());
    for(unsigned tab=0;tab<7;++tab){assert(ui.navigation().navigateTo(ScreenId::SoundEditor,uint8_t(tab)));preview::pumpFrame(ui);}
    assert(ui.navigation().navigateTo(ScreenId::SoundEditor,2));preview::pumpFrame(ui);
    assert(preview::saveFramebufferPpm("build/editor011/filter.ppm"));
    auto* open=button(ui.contentObject(),"USER / SAVE AS");assert(open);lv_obj_send_event(open,LV_EVENT_CLICKED,nullptr);preview::pumpFrame(ui);
    assert(preview::saveFramebufferPpm("build/editor011/presets.ppm"));
    auto* close=button(lv_screen_active(),"CLOSE");assert(close);lv_obj_send_event(close,LV_EVENT_CLICKED,nullptr);preview::pumpFrame(ui);
    assert(ui.navigation().navigateTo(ScreenId::PatchBrowser));preview::pumpFrame(ui);assert(button(ui.contentObject(),"USER PRESETS / SAVE AS"));
    assert(lv_mem_test()==LV_RESULT_OK);ui.shutdown();r.end();
    puts("PASS LVGL9.3: editor tabs, owner confirmation, coalesced macros, dirty guard, Save/Recall/reboot simulation, User panel, heap integrity");
}
