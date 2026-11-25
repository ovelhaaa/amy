#include "fake_nvs.h"
#include "pattern_nvs.h"
#include "performance_scene_nvs.h"
#include "ui_blueprint_performance_adapter.h"
#include "local_performance_backend.h"
#include "../ui-preview/preview_harness.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace smk::p4;
using namespace smk::p4::ui;

namespace {

lv_obj_t* findLabel(lv_obj_t* root, const char* text, bool prefix = false) {
    if (!root) return nullptr;
    if (lv_obj_check_type(root, &lv_label_class)) {
        const char* actual = lv_label_get_text(root);
        if (actual && (prefix ? std::strncmp(actual, text, std::strlen(text)) == 0
                              : std::strcmp(actual, text) == 0)) {
            return root;
        }
    }
    const uint32_t cnt = lv_obj_get_child_count(root);
    for (uint32_t i = 0; i < cnt; ++i) {
        if (auto* l = findLabel(lv_obj_get_child(root, int32_t(i)), text, prefix)) return l;
    }
    return nullptr;
}

lv_obj_t* findButton(lv_obj_t* root, const char* text, bool prefix = false) {
    if(lv_obj_check_type(root,&lv_button_class)) {
        for(unsigned i=0;i<lv_obj_get_child_count(root);++i) {
            auto* child=lv_obj_get_child(root,int(i));
            if(lv_obj_check_type(child,&lv_label_class)) {
                const auto* label=lv_label_get_text(child);
                if(prefix?std::strncmp(label,text,std::strlen(text))==0:std::strcmp(label,text)==0) return root;
            }
        }
    }
    for(unsigned i=0;i<lv_obj_get_child_count(root);++i)
        if(auto* button=findButton(lv_obj_get_child(root,int(i)),text,prefix)) return button;
    return nullptr;
}

void clickBtn(lv_obj_t* root, const char* text, UiBlueprint& app, bool prefix = false) {
    lv_obj_t* btn = findButton(root, text, prefix);
    if(!btn) { std::printf("MISSING BUTTON %s\n",text); std::fflush(stdout); }
    assert(btn);
    lv_obj_send_event(btn, LV_EVENT_CLICKED, nullptr);
    preview::pumpFrame(app, 25);
}

lv_obj_t* findSlider(lv_obj_t* root,int maximum) {
    if(lv_obj_check_type(root,&lv_slider_class) && lv_slider_get_max_value(root)==maximum) return root;
    for(unsigned i=0;i<lv_obj_get_child_count(root);++i)
        if(auto* child=findSlider(lv_obj_get_child(root,int(i)),maximum)) return child;
    return nullptr;
}

uint32_t countObjects(lv_obj_t* obj) {
    if (!obj) return 0;
    uint32_t count = 1;
    const uint32_t children = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < children; ++i) {
        count += countObjects(lv_obj_get_child(obj, int32_t(i)));
    }
    return count;
}

struct LatencyStats {
    double median_us = 0.0;
    double p95_us    = 0.0;
    double max_us    = 0.0;
};

LatencyStats computeLatency(std::vector<double>& samples) {
    LatencyStats s{};
    if (samples.empty()) return s;
    std::sort(samples.begin(), samples.end());
    s.median_us = samples[samples.size() / 2];
    const size_t p95_idx = std::min(samples.size() - 1, (samples.size() * 95) / 100);
    s.p95_us = samples[p95_idx];
    s.max_us = samples.back();
    return s;
}

} // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    const char* out_dir = (argc >= 2) ? argv[1] : "smk-p4/docs/ui_018c3_shots";

    amy::Runtime runtime;
    assert(runtime.begin());
    LocalAmyPerformanceBackend perf(runtime.commands());
    UiBlueprintPerformanceAdapter adapter(perf);

    NvsPatternStore patterns;
    assert(patterns.begin());
    NvsSceneStore scenes;
    assert(scenes.begin());
    adapter.setPatternStore(patterns);
    adapter.setSceneStore(scenes);

    assert(preview::initHostLvgl() != nullptr);
    UiBlueprint app(adapter);
    assert(app.begin());

    auto make_seq_req = [](seq::Action a) {
        seq::Request r{};
        r.action = a;
        return r;
    };

    auto sync_runtime = [&](uint32_t dt_ms = 25) {
        preview::g_sim_tick_ms += dt_ms;
        runtime.apply(preview::g_sim_tick_ms);
        assert(runtime.render());
        perf.publish(runtime.snapshot(), runtime.snapshot().active_notes == 0, 120, 0);
        preview::pumpFrame(app, dt_ms);
    };

    sync_runtime();

    const auto capture = [&](const char* name) {
        char path[256]{};
        std::snprintf(path, sizeof(path), "%s/%s.ppm", out_dir, name);
        preview::pumpFrame(app, 25);
        auto* badge=findLabel(app.rootObject(),"P4 LIVE");
        if(badge) { lv_label_set_text(badge,"HOST SIM"); lv_refr_now(nullptr); }
        assert(preview::saveFramebufferPpm(path));
        if(badge) lv_label_set_text(badge,"P4 LIVE");
        std::printf("CAPTURED: %s\n", path);
    };

    auto& nav=app.navigation();
    auto* root=app.contentObject();
    auto click=[&](const char* label) { clickBtn(root,label,app); sync_runtime(); };
    auto request=[&](seq::Action action,int value=0) {
        auto r=make_seq_req(action); r.value=int16_t(value);
        assert(adapter.requestSequencer(r).accepted()); sync_runtime();
    };
    assert(nav.navigateTo(ScreenId::Sequencer)); sync_runtime();
    capture("06_seq_stopped");
    click("EDIT MODE");
    clickBtn(root,"STEP 01",app,true); sync_runtime();
    click("LOAD DEMO PATTERN");
    click("CLOSE");
    capture("11_edit_piano_roll");
    for(unsigned i=0;i<seq::kSteps;++i) {
        if(i==8) click("1-8 / 9-16");
        char label[4]; std::snprintf(label,sizeof(label),"%02u",i+1);
        clickBtn(root,label,app,true); sync_runtime();
        assert((nav.subTab(ScreenId::Sequencer)-1)%seq::kSteps==i);
    }
    auto* velocity=findSlider(root,127); assert(velocity);
    lv_slider_set_value(velocity,91,LV_ANIM_OFF);
    lv_obj_send_event(velocity,LV_EVENT_RELEASED,nullptr); sync_runtime();
    assert(adapter.sequencerPattern()->steps[15].velocity==91);
    auto* gate=findSlider(root,100); assert(gate);
    lv_slider_set_value(gate,42,LV_ANIM_OFF);
    lv_obj_send_event(gate,LV_EVENT_RELEASED,nullptr); sync_runtime();
    assert(adapter.sequencerPattern()->steps[15].gate==42);
    capture("13_velocity_gate");
    click("PLAY MODE"); click("PLAY");
    assert(adapter.sequencerStatus()->playing);
    for(unsigned i=0;i<64;++i) sync_runtime();
    assert(adapter.sequencerStatus()->current_step<seq::kSteps);
    capture("07_seq_running");
    request(seq::Action::SetLiveChannel,0);
    click("LIVE TRANSPOSE OFF");
    assert(adapter.sequencerStatus()->midi.live);
    capture("08_live_transpose");
    click("SCALE QUANTIZE OFF");
    assert(adapter.sequencerStatus()->quantize);
    capture("09_quantize");
    click("EDIT MODE");
    const auto pattern=*adapter.sequencerPattern();
    const auto packed=nav.subTab(ScreenId::Sequencer);
    click("1-8 / 9-16");
    assert(nav.subTab(ScreenId::Sequencer)!=packed);
    capture("12_selected_second_page");
    clickBtn(root,"STEP ",app,true); sync_runtime(); capture("13_velocity_gate_inspector"); click("CLOSE");

    request(seq::Action::SetLive,0);
    click("MIDI / REC"); capture("17_midi_settings");
    click("STEP INPUT"); assert(adapter.sequencerStatus()->midi.mode==seq::InputMode::StepInput);
    capture("14_step_input");
    click("STEP RECORD"); assert(adapter.sequencerStatus()->midi.mode==seq::InputMode::StepRecord);
    capture("15_step_record");
    for(unsigned i=0;i<100;++i) {
        const uint8_t note=uint8_t(60+i%12);
        const uint8_t on[]{0x09,0x90,note,100}, off[]{0x08,0x80,note,0};
        const auto selected=adapter.sequencerStatus()->midi.selected;
        assert(runtime.usbInput().transfer(on,sizeof(on),preview::g_sim_tick_ms)); sync_runtime();
        assert(adapter.sequencerPattern()->steps[selected].note==note);
        assert(adapter.sequencerStatus()->midi.selected==(selected+1)%adapter.sequencerPattern()->length);
        assert(runtime.usbInput().transfer(off,sizeof(off),preview::g_sim_tick_ms)); sync_runtime();
    }
    click("CLOSE");
    assert(adapter.sequencerStatus()->midi.mode==seq::InputMode::StepRecord);
    clickBtn(root,"STEP ",app,true); sync_runtime();
    click("INSERT REST"); capture("16_rest_next"); click("CLOSE");
    click("PLAY MODE");
    assert(adapter.sequencerStatus()->playing);
    assert(adapter.requestParameterBool("arp.enable",true).accepted()); sync_runtime();
    assert(nav.navigateTo(ScreenId::Arpeggiator)); sync_runtime();
    capture("02_arp_armed");
    click("PLAY ARP"); click("STOP ARP");
    assert(adapter.sequencerStatus()->playing);
    capture("01_arp_stopped");
    click("PLAY ARP"); click("CLEAR LATCH");
    request(seq::Action::SetInputMode,0);
    request(seq::Action::SetLive,0);
    const uint8_t on[]{0x09,0x90,60,100}, off[]{0x08,0x80,60,0};
    assert(runtime.usbInput().transfer(on,sizeof(on),preview::g_sim_tick_ms));
    for(unsigned i=0;i<30;++i) sync_runtime();
    capture("03_arp_running");
    assert(adapter.requestParameterBool("arp.latch",true).accepted()); sync_runtime();
    assert(runtime.usbInput().transfer(off,sizeof(off),preview::g_sim_tick_ms)); sync_runtime();
    capture("04_arp_latched");
    click("DOWN"); capture("05_arp_direction");
    assert(nav.navigateTo(ScreenId::Patterns)); sync_runtime();
    capture("18_patterns_16_slots");
    auto before=*adapter.sequencerPattern();
    clickBtn(root,"16 EMPTY",app,true); sync_runtime();
    assert(*adapter.sequencerPattern()==before);
    assert(nav.patternSelection()==15);
    capture("19_preselected_slot");
    seq::Slot slot;
    slot.state=seq::SlotState::Valid;
    slot.settings.pattern=*adapter.sequencerPattern();
    std::snprintf(slot.name,sizeof(slot.name),"HOST GROOVE");
    assert(patterns.commit(0,slot)); assert(patterns.commit(1,slot));
    adapter.setPatternStore(patterns); nav.refreshActive();
    clickBtn(root,"01 HOST",app,true); sync_runtime();
    click("LOAD"); capture("22_discard_confirmation");
    click("DISCARD & LOAD"); sync_runtime(); capture("20_immediate_load");
    click("SWITCH MODE");
    clickBtn(root,"02 HOST",app,true); sync_runtime(); click("LOAD");
    for(unsigned i=0;i<3;++i) sync_runtime();
    assert(adapter.sequencerStatus()->pending_slot==1);
    capture("21_next_bar");
    assert(nav.navigateTo(ScreenId::Sequencer)); sync_runtime(); capture("10_seq_pending");
    assert(nav.navigateTo(ScreenId::Patterns)); sync_runtime(); click("CANCEL QUEUE");
    request(seq::Action::Stop); assert(adapter.requestArpTransport(false).accepted()); sync_runtime();
    for(unsigned i=0;i<30;++i) sync_runtime();
    click("SAVE"); capture("23_save_confirmation"); click("CONFIRM"); capture("23b_storage_requested");
    for(unsigned i=0;i<12;++i) sync_runtime();
    assert(std::strstr(adapter.patternNotice(),"CONFIRMED"));
    PatternOperation invalid;
    invalid.slot=seq::kPatternSlots;
    adapter.requestPattern(invalid); sync_runtime(); capture("24_failed_operation");
    assert(nav.navigateTo(ScreenId::Patterns)); sync_runtime();
    for(unsigned i=0;i<100;++i) {
        click("SAVE");
        assert(findLabel(root,"CANCEL"));
        click("CANCEL");
        assert(!findLabel(root,"CONFIRM"));
    }
    request(seq::Action::Play);
    assert(adapter.requestArpTransport(true).accepted()); sync_runtime();
    assert(nav.navigateTo(ScreenId::Effects)); sync_runtime(); capture("25_fx_while_running");
    // No musical mutations or transport commands may be caused by navigation.
    assert(nav.navigateTo(ScreenId::Performance)); sync_runtime();
    const auto objects=countObjects(lv_screen_active());
    lv_mem_monitor_t initial{}; lv_mem_monitor(&initial);
    const auto generation=runtime.snapshot().generation;
    const auto command=adapter.sequencerStatus()->command.token;
    const auto phase=runtime.snapshot().arp.tempo.phase;
    std::vector<double> times;
    for(unsigned i=0;i<500;++i) {
        const auto start=std::chrono::steady_clock::now();
        assert(nav.navigateTo(ScreenId::Sequencer));
        nav.setSequencerEdit(i%2); assert(nav.navigateTo(ScreenId::Sequencer));
        if(i<200) { assert(nav.navigateTo(ScreenId::Patterns)); assert(nav.navigateTo(ScreenId::Arpeggiator)); }
        assert(nav.navigateTo(ScreenId::Effects));
        assert(nav.navigateTo(ScreenId::Performance));
        sync_runtime(1);
        times.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());
    }
    assert(adapter.sequencerStatus()->playing);
    assert(runtime.snapshot().generation==generation);
    assert(adapter.sequencerStatus()->command.token==command);
    assert(runtime.snapshot().arp.tempo.phase>phase);
    lv_mem_monitor_t final{}; lv_mem_monitor(&final);
    const auto stats=computeLatency(times);
    std::printf("C3 STRESS objs=%u->%u free=%zu->%zu largest=%zu->%zu peak=%zu frag=%u->%u median=%.1f p95=%.1f max=%.1f us\n",
        objects,countObjects(lv_screen_active()),initial.free_size,final.free_size,initial.free_biggest_size,final.free_biggest_size,
        final.max_used,initial.frag_pct,final.frag_pct,stats.median_us,stats.p95_us,stats.max_us);
    assert(objects==countObjects(lv_screen_active()));
    assert(initial.free_size==final.free_size);
    (void)pattern;
    nav.shutdown();
    std::puts("PASS C3 rhythm functional / host simulation");
}


