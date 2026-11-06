#include "ui_blueprint_performance_adapter.h"
#include "local_performance_backend.h"
#include "../ui-preview/preview_harness.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace smk::p4;using namespace smk::p4::ui;
static lv_obj_t* label(lv_obj_t* root,const char* text){
    if(lv_obj_check_type(root,&lv_label_class) && std::strstr(lv_label_get_text(root),text))return root;
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto* f=label(lv_obj_get_child(root,int32_t(i)),text))return f;
    return nullptr;
}
int main(){
    preview::initHostLvgl();amy::Runtime r;assert(r.begin());LocalAmyPerformanceBackend b(r.commands());UiBlueprintPerformanceAdapter a(b);
    auto publish=[&]{b.publish(r.snapshot(),true,0,0);a.service(++preview::g_sim_tick_ms);};publish();
    assert(a.parameterStatus("harmony.scale_enable")==FeatureStatus::Available);
    assert(a.parameterStatus("harmony.preserve_outside")==FeatureStatus::Planned);
    assert(a.parameterStatus("harmony.dest_layer")==FeatureStatus::Planned);
    assert(!a.model().getBool("harmony.scale_enable") && !a.model().getBool("harmony.chord_enable"));
    assert(a.requestParameterInt("harmony.preset",1).state==UiCommandState::Requested);
    assert(!a.model().getBool("harmony.scale_enable"));publish();assert(a.lastCommandState()==UiCommandState::Requested);
    r.apply(0);publish();assert(a.lastCommandState()==UiCommandState::Confirmed && a.model().getBool("harmony.scale_enable"));
    assert(a.requestParameterInt("harmony.chord_type",10).state==UiCommandState::Rejected);
    assert(a.requestParameterInt("harmony.voicing",2).state==UiCommandState::Rejected);
    UiBlueprint app(a);assert(app.begin());assert(app.navigation().navigateTo(ScreenId::Harmony,0));preview::pumpFrame(app);
    assert(!label(app.contentObject(),"DEMO PREVIEW"));auto* l=label(app.contentObject(),"C3");assert(l);auto* key=lv_obj_get_parent(l);
    lv_obj_send_event(key,LV_EVENT_PRESSED,nullptr);lv_obj_send_event(key,LV_EVENT_PRESSED,nullptr);lv_obj_send_event(key,LV_EVENT_CLICKED,nullptr);
    r.apply(preview::g_sim_tick_ms);r.render();publish();preview::pumpFrame(app);assert(r.snapshot().active_notes==3);
    assert(label(app.contentObject(),"LAST ADMITTED"));assert(preview::saveFramebufferPpm("build/harmony09/harmony_scale.ppm"));
    lv_obj_send_event(key,LV_EVENT_RELEASED,nullptr);r.apply(preview::g_sim_tick_ms);assert(!r.snapshot().active_notes);
    assert(app.navigation().navigateTo(ScreenId::Harmony,1));preview::pumpFrame(app);assert(preview::saveFramebufferPpm("build/harmony09/harmony_chord.ppm"));
    // Actual stepper skips unsupported ADD9/9TH on its way to diatonic triad.
    assert(a.requestParameterInt("harmony.chord_type",9).accepted());r.apply(preview::g_sim_tick_ms);publish();app.navigation().refreshActive();
    auto* title=label(app.contentObject(),"CHORD TYPE");assert(title);auto* card=lv_obj_get_parent(title);auto* plus=label(card,"+");assert(plus);
    lv_obj_send_event(lv_obj_get_parent(plus),LV_EVENT_CLICKED,nullptr);r.apply(preview::g_sim_tick_ms);publish();assert(r.snapshot().harmony.chord==12);
    for(unsigned s=0;s<unsigned(ScreenId::Count);++s){assert(app.navigation().navigateTo(ScreenId(s)));preview::pumpFrame(app);}
    assert(a.requestParameterInt("harmony.usb_channel",16).accepted());r.apply(preview::g_sim_tick_ms);publish();assert(r.snapshot().harmony.channel==15);
    // A canceled queued config is never confirmed as applied.
    assert(a.requestParameterInt("harmony.root_note",2).accepted());r.commands().panic();r.apply(preview::g_sim_tick_ms);publish();a.service(preview::g_sim_tick_ms+2001);
    assert(a.lastCommandState()==UiCommandState::Rejected && r.snapshot().harmony.root==0);
    app.shutdown();r.end();puts("PASS real LVGL + AMY Harmony: deferred confirmation,14 screens,real key,Off,planned options,stepper,channel,timeout");
}
