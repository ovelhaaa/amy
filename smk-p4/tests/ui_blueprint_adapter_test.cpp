#include "ui_blueprint_performance_adapter.h"
#include "local_performance_backend.h"
#include "../ui-preview/preview_harness.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <initializer_list>
using namespace smk::p4;
using namespace smk::p4::ui;
struct Mock final : PerformanceBackend {
    PerformanceState s{}; bool ready=true,accept=true,error=false; uint32_t rev=1;
    unsigned ons=0,offs=0,panics=0,patches=0,masters=0;
    bool expanded=false,patch_available=true;
    uint16_t requested_patch=0; uint8_t requested_master=0;
    void service(uint32_t) override {}
    bool connected() const override { return ready; }
    bool busy() const override { return false; }
    const PerformanceState& state() const override { return s; }
    uint32_t stateRevision() const override { return rev; }
    const char* status() const override { return ready?"READY":"OFFLINE"; }
    bool midiError() const override { return error; }
    uint16_t patchCount() const override { return expanded?3:2; }
    uint16_t patchAt(uint16_t i) const override { return i==0?128:i==1?135:expanded && i==2?138:UINT16_MAX; }
    bool patchInfo(uint16_t i,PatchInfo& p,uint32_t) override { p.id=patchAt(i);p.available=i<patchCount() && patch_available;return i<patchCount(); }
    bool setPatch(uint16_t id,uint32_t) override { ++patches;requested_patch=id;return accept; }
    bool setMaster(uint8_t v,uint32_t) override { ++masters;requested_master=v;return accept; }
    bool setMacro(uint8_t,uint8_t,uint32_t) override { assert(false); return false; }
    bool noteOn(uint8_t,uint8_t,uint32_t) override { ++ons;return accept; }
    void noteOff(uint8_t) override { ++offs; }
    void heldNote(uint8_t) override { assert(false); }
    void panic() override { ++panics; }
    Mock() { s.patch_id=128;s.master=64; }
};
static void contract() {
    Mock m; UiBlueprintPerformanceAdapter a(m); a.service(100);
    assert(a.requestSelectPatch(135).state==UiCommandState::Requested);
    assert(a.model().activePatch().id==128);
    a.service(101); assert(a.lastCommandState()==UiCommandState::Requested); // identical revision
    ++m.rev;a.service(102);assert(a.lastCommandState()==UiCommandState::Requested); // new publish, old value
    m.s.patch_id=135;++m.rev;a.service(103);assert(a.model().activePatch().id==135 && a.lastCommandState()==UiCommandState::Confirmed);
    assert(a.requestSelectPatch(128).state==UiCommandState::Requested);
    m.s.patch_id=128;++m.rev;a.service(104);assert(a.model().activePatch().id==128);
    const auto calls=m.patches;
    assert(a.requestSelectPatch(0).state==UiCommandState::Unsupported && m.patches==calls);
    assert(a.requestSelectPatch(135,1).state==UiCommandState::Unsupported);
    assert(a.requestParameterInt("perf.master_volume",100).state==UiCommandState::Requested);
    assert(a.model().getInt("perf.master_volume")==64);
    m.s.master=100;a.service(105);assert(a.model().getInt("perf.master_volume")==64); // stale revision
    ++m.rev;a.service(106);assert(a.model().getInt("perf.master_volume")==100);
    m.accept=false;assert(a.requestParameterInt("perf.master_volume",99).state==UiCommandState::Rejected);
    assert(a.requestSelectPatch(135).state==UiCommandState::Rejected && a.model().activePatch().id==128);
    assert(!a.requestNoteOn(60,100).accepted());m.accept=true;
    assert(!a.requestParameterFloat("perf.master_volume",std::numeric_limits<float>::quiet_NaN()).accepted());
    for(const char* id:{"perf.macro.1","perf.macro.8","perf.layer_mode","fx.reverb.mix","seq.running","harmony.scale_enable","midi.learn"}) {
        assert(a.requestParameterInt(id,1).state==UiCommandState::Unsupported);
        assert(!a.isParameterEditable(id));
    }
    auto x=a.requestNoteOn(60,100),y=a.requestNoteOn(60,90);assert(x.token && y.token!=x.token);
    const auto ons=m.ons;const auto offs=m.offs;
    a.requestNoteOff(60,x.token);assert(m.offs==offs && a.isNoteHeld(60));
    a.requestNoteOff(60,y.token);assert(m.offs==offs+1 && m.ons==ons);
    auto old=a.requestNoteOn(60,100);a.requestPanic();auto fresh=a.requestNoteOn(60,100);
    assert(!a.requestNoteOff(60,old.token).accepted() && a.isNoteHeld(60));
    a.requestNoteOff(60,fresh.token);
    old=a.requestNoteOn(60,100);assert(a.requestSelectPatch(135).accepted());
    fresh=a.requestNoteOn(60,100);assert(!a.requestNoteOff(60,old.token).accepted() && a.isNoteHeld(60));
    a.requestNoteOff(60,fresh.token);
    a.service(200);x=a.requestNoteOn(61,100);a.service(300);y=a.requestNoteOn(61,100);
    a.requestNoteOff(61,x.token);a.service(10199);assert(a.isNoteHeld(61));
    a.service(10200);assert(!a.isNoteHeld(61));assert(!a.requestNoteOff(61,y.token).accepted());
    fresh=a.requestNoteOn(61,100);assert(fresh.token!=y.token);a.releaseAllUiNotes();
    x=a.requestNoteOn(60,100);assert(a.requestParameterInt("perf.octave_shift",1).accepted());
    assert(!a.isNoteHeld(60) && !a.requestNoteOff(60,x.token).accepted());
    x=a.requestNoteOn(72,100);m.s.flags=smk::link::kUsbConnected; a.service(10201);assert(a.model().diagnostics().midi_connected);
    m.s.flags=0;a.service(10202);assert(!a.model().diagnostics().midi_connected && a.isNoteHeld(72)); // USB loss preserves touch
    m.ready=false;m.s.flags=smk::link::kUsbConnected;a.service(10203);
    assert(a.model().diagnostics().midi_connected && !a.isNoteHeld(72) && !a.requestNoteOn(60,100).accepted());
    Mock empty;empty.rev=0;UiBlueprintPerformanceAdapter no_snapshot(empty);no_snapshot.service(1);assert(!no_snapshot.requestNoteOn(60,100).accepted());
    a.releaseAllUiNotes();m.ready=true;++m.rev;a.service(11000);
    for(unsigned i=0;i<UiBlueprintPerformanceAdapter::kGestureCapacity;++i) assert(a.requestNoteOn(63,100).accepted());
    assert(!a.requestNoteOn(64,100).accepted());const auto before_off=m.offs;a.releaseAllUiNotes();assert(m.offs==before_off+1);
    std::puts("PASS adapter mock: real contract, requested/confirmed/rejected, stale snapshot, catalog, ledger, octave, lease, disconnect and unsupported controls");
}
static lv_obj_t* button(lv_obj_t* root,const char* text) {
    if(lv_obj_check_type(root,&lv_label_class) && std::strcmp(lv_label_get_text(root),text)==0) return lv_obj_get_parent(root);
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i) if(auto* found=button(lv_obj_get_child(root,static_cast<int32_t>(i)),text)) return found;
    return nullptr;
}
static void lifecycle() {
    Mock m;UiBlueprintPerformanceAdapter a(m);a.service(1);
    UiBlueprint app(a);assert(app.begin());auto& nav=app.navigation();
    assert(button(app.rootObject(),"AUD: OFF") && button(app.rootObject(),"MIDI: WAIT"));
    m.s.flags=smk::link::kAudioLive|smk::link::kUsbConnected;a.service(2);preview::pumpFrame(app);
    assert(button(app.rootObject(),"AUD: RUN") && button(app.rootObject(),"MIDI: OK"));
    m.error=true;preview::pumpFrame(app);assert(button(app.rootObject(),"MIDI: ERR"));m.error=false;
    auto* key=button(app.contentObject(),"C4");assert(key);
    lv_obj_send_event(key,LV_EVENT_PRESSED,nullptr);lv_obj_send_event(key,LV_EVENT_PRESSED,nullptr);
    assert(m.ons==1);lv_obj_send_event(key,LV_EVENT_CLICKED,nullptr);
    lv_obj_send_event(key,LV_EVENT_LONG_PRESSED,nullptr);lv_obj_send_event(key,LV_EVENT_LONG_PRESSED_REPEAT,nullptr);assert(m.ons==1);
    lv_obj_send_event(key,LV_EVENT_PRESS_LOST,nullptr);assert(m.offs==1);
    lv_obj_send_event(key,LV_EVENT_PRESSED,nullptr);assert(a.isNoteHeld(60));
    lv_obj_delete(key);assert(!a.isNoteHeld(60)); // child-first external deletion
    assert(nav.navigateTo(ScreenId::Performance,0));key=button(app.contentObject(),"C4");assert(key);
    lv_obj_send_event(key,LV_EVENT_PRESSED,nullptr);
    auto* keyboard=lv_obj_get_parent(key);lv_obj_delete(keyboard);assert(!a.isNoteHeld(60)); // parent DELETE before child callbacks
    assert(nav.navigateTo(ScreenId::Performance,0));key=button(app.contentObject(),"C4");assert(key);
    lv_obj_send_event(key,LV_EVENT_PRESSED,nullptr);assert(nav.navigateTo(ScreenId::Harmony));assert(!a.isNoteHeld(60));
    unsigned ons=m.ons;key=button(app.contentObject(),"C3");assert(key);lv_obj_send_event(key,LV_EVENT_PRESSED,nullptr);assert(m.ons==ons);
    assert(nav.navigateTo(ScreenId::Layers));key=button(app.contentObject(),"C3");assert(key);lv_obj_send_event(key,LV_EVENT_PRESSED,nullptr);assert(m.ons==ons);
    assert(nav.navigateTo(ScreenId::Performance,1));key=button(app.contentObject(),"C3");assert(key);
    lv_obj_send_event(key,LV_EVENT_PRESSED,nullptr);a.requestParameterInt("perf.octave_shift",1);
    nav.refreshActive();lv_obj_send_event(key,LV_EVENT_RELEASED,nullptr);assert(!a.isNoteHeld(48));
    assert(nav.navigateTo(ScreenId::GlobalSettings));
    auto* reset=button(app.contentObject(),"RESET DEMO RAM DEFAULTS");assert(reset);
    m.s.master=101;m.s.patch_id=135;++m.rev;a.service(2);
    lv_obj_send_event(reset,LV_EVENT_CLICKED,nullptr);
    assert(a.model().getInt("perf.master_volume")==101 && a.model().activePatch().id==135);
    // All 14 screens repeatedly, actual adapter, 128KB LVGL pool, no startup DSP writes.
    assert(nav.navigateTo(ScreenId::Performance,0));preview::pumpFrame(app);
    // Warm font/image caches across every screen before measuring repeated cycles.
    for(unsigned s=0;s<14;++s) { assert(nav.navigateTo(static_cast<ScreenId>(s)));preview::pumpFrame(app); }
    assert(nav.navigateTo(ScreenId::Performance,0));preview::pumpFrame(app);
    lv_mem_monitor_t before{},after{};lv_mem_monitor(&before);
    const auto patches=m.patches,masters=m.masters;
    for(unsigned cycle=0;cycle<50;++cycle) for(unsigned s=0;s<14;++s) {
        assert(nav.navigateTo(static_cast<ScreenId>(s)));preview::pumpFrame(app);
        assert(lv_mem_test()==LV_RESULT_OK);
    }
    assert(nav.navigateTo(ScreenId::Performance,0));preview::pumpFrame(app);lv_mem_monitor(&after);
    std::fprintf(stderr,"memory before=%zu after=%zu patches=%u/%u masters=%u/%u\n",before.free_size,after.free_size,patches,m.patches,masters,m.masters);
    // TLSF alignment can change a small block at equal logical state; detect
    // continued growth independently with a second equally-sized window.
    assert(before.free_size<=after.free_size+64 && m.patches==patches && m.masters==masters);
    for(unsigned cycle=0;cycle<50;++cycle) for(unsigned s=0;s<14;++s) { assert(nav.navigateTo(static_cast<ScreenId>(s)));preview::pumpFrame(app); }
    assert(nav.navigateTo(ScreenId::Performance,0));preview::pumpFrame(app);
    lv_mem_monitor_t repeat{};lv_mem_monitor(&repeat);
    assert(repeat.free_size==after.free_size && repeat.frag_pct<=5);
    std::printf("PASS adapter LVGL: delete orders, PRESS_LOST, duplicate suppression, preview isolation, octave/navigation; 1400 screens free=%zu peak=%zu largest=%zu frag=%u%% second_window_growth=0\n",after.free_size,after.max_used,after.free_biggest_size,after.frag_pct);
    assert(preview::saveFramebufferPpm("build/p404/live_mock_stage.ppm"));
    key=button(app.contentObject(),"C5");if(!key) key=button(app.contentObject(),"C4");assert(key);
    lv_obj_send_event(key,LV_EVENT_PRESSED,nullptr);app.shutdown();
    for(unsigned n=0;n<128;++n) assert(!a.isNoteHeld(static_cast<uint8_t>(n)));
}
static void realRuntime() {
    amy::Runtime r;assert(r.begin());LocalAmyPerformanceBackend p(r.commands());UiBlueprintPerformanceAdapter a(p);
    assert(a.model().diagnostics().max_voices==amy::kVoices); // Before the first snapshot.
    auto publish=[&](uint32_t t){p.publish(r.snapshot(),false,100,0);a.service(t);};publish(0);
    assert(a.model().diagnostics().max_voices==amy::kVoices);
    for(bool touch_first:{true,false}) {
        auto g=a.requestNoteOn(60,100);assert(g.accepted());
        uint8_t on[]={9,0x90,60,90},off[]={8,0x80,60,0};
        assert(r.usbInput().transfer(on,4,0));r.apply(0);assert(r.snapshot().active_notes==2);assert(r.render());
        if(touch_first) a.requestNoteOff(60,g.token);else assert(r.usbInput().transfer(off,4,1));
        r.apply(1);assert(r.snapshot().active_notes==1);
        if(touch_first) assert(r.usbInput().transfer(off,4,2));else a.requestNoteOff(60,g.token);
        r.apply(2);assert(r.snapshot().active_notes==0);publish(3);
    }
    uint8_t pedal[]={11,0xb0,64,127},usb_on[]={9,0x90,60,100},usb_off[]={8,0x80,60,0};
    assert(r.usbInput().transfer(pedal,4,3) && r.usbInput().transfer(usb_on,4,3));r.apply(3);
    assert(r.usbInput().transfer(usb_off,4,3));r.apply(3);
    auto touch=a.requestNoteOn(60,100);assert(touch.accepted());r.apply(3);assert(r.snapshot().active_notes==2);
    a.requestPanic();r.apply(3);assert(!r.snapshot().active_notes);
    size_t count=0;const auto* entries=UiModel::catalog(count);
    for(const auto& p:amy::kPatches) {
        bool found=false;
        for(size_t i=0;i<count;++i) if(entries[i].id==p.id) {
            found=true;assert(std::strcmp(entries[i].name,p.name)==0);
            assert(entries[i].family==p.family && entries[i].category==p.category);
        }
        assert(found && a.isPatchSelectable(p.id) && a.patchStatus(p.id)==FeatureStatus::Available);
    }
    assert(!a.isPatchSelectable(4) && a.patchStatus(4)==FeatureStatus::Planned);
    a.requestSelectPatch(135);assert(a.model().activePatch().id==128);r.apply(3);publish(4);assert(a.model().activePatch().id==135);
#ifdef SMKP4_SOUND_LIBRARY
    for(uint16_t id:{uint16_t(138),uint16_t(142)}) {
        const auto old=a.model().activePatch().id;
        assert(a.requestSelectPatch(id).state==UiCommandState::Requested);
        assert(a.model().activePatch().id==old);
        r.apply(4);publish(4);
        assert(a.model().activePatch().id==id && a.lastCommandState()==UiCommandState::Confirmed);
    }
    a.requestSelectPatch(135);r.apply(4);publish(4);
#endif
    auto old=a.requestNoteOn(60,100);r.apply(4);a.requestPanic();r.apply(5);publish(6);
    auto fresh=a.requestNoteOn(60,100);assert(!a.requestNoteOff(60,old.token).accepted());r.apply(6);assert(r.snapshot().active_notes==1);
    // Old mailbox publication does not invalidate adapter gesture tokens.
    auto stale=r.snapshot();p.publish(stale,true,0,0);a.service(7);a.requestNoteOff(60,fresh.token);r.apply(7);assert(!r.snapshot().active_notes);
    fresh=a.requestNoteOn(62,100);r.apply(7);r.apply(10007);publish(10007);assert(!a.isNoteHeld(62) && !r.snapshot().active_notes);
    fresh=a.requestNoteOn(62,100);r.apply(10007);assert(r.snapshot().active_notes==1);a.releaseAllUiNotes();r.apply(10008);
    uint8_t on[]={9,0x90,60,90};assert(r.usbInput().transfer(on,4,10008));r.apply(10008);
    fresh=a.requestNoteOn(60,100);r.apply(10008);r.usbInput().disconnect();r.apply(10009);assert(r.snapshot().active_notes==1);
    a.requestNoteOff(60,fresh.token);r.apply(10010);assert(!r.snapshot().active_notes);
    publish(10011);
    UiBlueprint app(a);assert(app.begin());preview::pumpFrame(app);
    assert(preview::saveFramebufferPpm("build/p404/real_amy_host_stage.ppm"));
#ifdef SMKP4_SOUND_LIBRARY
    for(uint16_t id:{uint16_t(138),uint16_t(142),uint16_t(128),uint16_t(135)}) {
        auto* next=button(app.contentObject(),"NEXT >");assert(next);
        const auto old=a.model().activePatch().id;
        lv_obj_send_event(next,LV_EVENT_CLICKED,nullptr);
        assert(a.model().activePatch().id==old);
        r.apply(10011);publish(10011);preview::pumpFrame(app);
        assert(a.model().activePatch().id==id);
    }
    auto* prev=button(app.contentObject(),"< PREV");assert(prev);
    lv_obj_send_event(prev,LV_EVENT_CLICKED,nullptr);r.apply(10011);publish(10011);preview::pumpFrame(app);
    assert(a.model().activePatch().id==128);
    a.requestSelectPatch(138);r.apply(10011);publish(10011);preview::pumpFrame(app);
    assert(button(app.contentObject(),"E.PIANO  |  FM 6-OP  |  ROM DX7"));
    assert(preview::saveFramebufferPpm("build/p404/library07_stage.ppm"));
#endif
    assert(app.navigation().navigateTo(ScreenId::Performance,1));preview::pumpFrame(app);
    assert(preview::saveFramebufferPpm("build/p404/real_amy_host_keys.ppm"));
    assert(app.navigation().navigateTo(ScreenId::PatchBrowser));preview::pumpFrame(app);
    assert(preview::saveFramebufferPpm("build/p404/real_amy_host_browser.ppm"));
#ifdef SMKP4_SOUND_LIBRARY
    auto* avail=button(app.contentObject(),"P4 ROM (AVAIL)");assert(avail);
    lv_obj_send_event(avail,LV_EVENT_CLICKED,nullptr);preview::pumpFrame(app);
    assert(preview::saveFramebufferPpm("build/p404/library07_browser.ppm"));
#endif
    assert(app.navigation().navigateTo(ScreenId::Diagnostics));preview::pumpFrame(app);
    assert(preview::saveFramebufferPpm("build/p404/real_amy_host_diagnostics.ppm"));app.shutdown();
    // Saturated production queue: rejects On; Off/Panic still reach owner.
    publish(10012);fresh=a.requestNoteOn(65,100);r.apply(10012);
    for(unsigned i=0;i<amy::kCommandCapacity;++i) assert(p.setMaster(70,10012));
    assert(a.requestParameterInt("perf.master_volume",10).state==UiCommandState::Rejected);
    assert(!a.requestNoteOn(66,100).accepted());a.requestNoteOff(65,fresh.token);r.apply(10013);assert(!r.snapshot().active_notes);
    assert(a.requestSelectPatch(128).state==UiCommandState::Requested);r.apply(10013);publish(10014);assert(a.model().activePatch().id==128);
    fresh=a.requestNoteOn(65,100);r.apply(10014);
    for(unsigned i=0;i<amy::kCommandCapacity;++i) assert(p.setMaster(70,10014));
    assert(a.requestSelectPatch(135).state==UiCommandState::Rejected);r.apply(10015);assert(!r.snapshot().active_notes);
    assert(!a.requestNoteOff(65,fresh.token).accepted());
    r.end();std::puts("PASS real AMY + production adapter: USB/Touch same pitch both release orders, patch, panic generations, stale mailbox, lease/rearm and USB disconnect");
}
static void discovery() {
    Mock m;m.expanded=true;UiBlueprintPerformanceAdapter a(m);a.service(1);
    assert(a.isPatchSelectable(138) && !a.isPatchSelectable(142));
    m.patch_available=false;assert(!a.isPatchSelectable(138));
    assert(a.requestSelectPatch(138).state==UiCommandState::Unsupported);
    m.patch_available=true;assert(a.requestSelectPatch(138).state==UiCommandState::Requested);
    assert(a.model().activePatch().id==128);
    a.service(2002);assert(a.lastCommandState()==UiCommandState::Rejected);
    assert(a.model().activePatch().id==128);
}
int main() { discovery();contract();preview::initHostLvgl();lifecycle();realRuntime(); }
