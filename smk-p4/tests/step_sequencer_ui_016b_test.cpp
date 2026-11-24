#include "fake_nvs.h"
#include "pattern_nvs.h"
#include "performance_scene_nvs.h"
#include "ui_blueprint_performance_adapter.h"
#include "local_performance_backend.h"
#include "../ui-preview/preview_harness.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>

using namespace smk::p4;
using namespace smk::p4::ui;

namespace {
lv_obj_t* findLabel(lv_obj_t* root,const char* text,bool prefix=false) {
    if(lv_obj_check_type(root,&lv_label_class)) {
        const char* actual=lv_label_get_text(root);
        if(prefix?std::strncmp(actual,text,std::strlen(text))==0:std::strcmp(actual,text)==0) return root;
    }
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i) {
        if(auto* label=findLabel(lv_obj_get_child(root,int32_t(i)),text,prefix)) return label;
    }
    return nullptr;
}

lv_obj_t* findButton(lv_obj_t* root,const char* text,bool prefix=false) {
    auto* label=findLabel(root,text,prefix);
    if(!label) std::fprintf(stderr,"Button label missing: %s\n",text);
    assert(label);
    auto* button=lv_obj_get_parent(label);
    assert(lv_obj_check_type(button,&lv_button_class));
    return button;
}

lv_obj_t* findType(lv_obj_t* root,const lv_obj_class_t* type) {
    if(lv_obj_check_type(root,type)) return root;
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)
        if(auto* child=findType(lv_obj_get_child(root,int32_t(i)),type)) return child;
    return nullptr;
}

lv_point_t touch_point;
bool touch_down=false;
void touchRead(lv_indev_t*,lv_indev_data_t* data) {
    data->point=touch_point;
    data->state=touch_down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
}

void pointerEvent(lv_indev_t* input,int x,int y,bool down) {
    touch_point={x,y};
    touch_down=down;
    ++preview::g_sim_tick_ms;
    lv_indev_read(input);
}

void timing() {
    using D=tempo::Division;
    constexpr D rates[]{D::Quarter,D::Sixteenth,D::ThirtySecond,D::EighthTriplet,D::SixteenthTriplet};
    const auto interval=[](D rate,unsigned swing,bool odd) {
        const bool triplet=rate==D::EighthTriplet || rate==D::SixteenthTriplet;
        const uint64_t nominal=uint64_t(tempo::kUnitsPerTick)*tempo::kPpqn*tempo::divisionNumerator(rate)/48;
        return nominal/100*(triplet?100:2*(odd?100-swing:swing));
    };
    for(const unsigned swing:{50u,75u}) for(const auto from:rates) for(const auto to:rates) {
        seq::Engine engine;
        seq::Pattern pattern;
        for(auto& step:pattern.steps) step={60,100,75,true};
        tempo::Position position;
        assert(engine.setPattern(pattern) && engine.setDivision(from) && engine.setSwing(swing));
        assert(engine.play(position) && engine.update(position).velocity==100);
        const auto previous=interval(from,swing,false);
        position.phase=previous/3;
        assert(engine.setDivision(to) && !engine.update(position).velocity);
        position.phase=previous/100*75-1;
        assert(!engine.update(position).release);
        ++position.phase;
        assert(engine.update(position).release);
        position.phase=previous-1;
        assert(!engine.update(position).velocity && engine.status().current_step==0);
        ++position.phase;
        assert(engine.update(position).velocity==100 && engine.status().current_step==1);
        for(unsigned step=2;step<=128;++step) {
            position.phase+=interval(to,swing,(step-1)%2!=0)-1;
            assert(!engine.update(position).velocity);
            ++position.phase;
            assert(engine.update(position).velocity==100 && engine.status().current_step==step%seq::kSteps);
        }
        assert(position.bpm==tempo::kDefaultBpm);
    }
    puts("PASS 50 live division transitions preserve current gate and next onset, then follow the new division without drift");
}
}

int main(int argc,char** argv) {
    assert(argc==2);
    timing();
    auto* display=preview::initHostLvgl();
    assert(lv_display_get_horizontal_resolution(display)==800);
    assert(lv_display_get_vertical_resolution(display)==480);
    auto* input=lv_indev_create();
    lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(input,touchRead);
    lv_indev_set_mode(input,LV_INDEV_MODE_EVENT);

    amy::Runtime runtime;
    LocalAmyPerformanceBackend backend(runtime.commands());
    UiBlueprintPerformanceAdapter adapter(backend);
    UiBlueprint app(adapter);
    assert(app.begin());
    auto* content=app.contentObject();
    const auto frame=[&] { preview::pumpFrame(app); };
    const auto click=[&](lv_obj_t* root,const char* name,bool prefix=false) {
        lv_area_t bounds;
        lv_obj_get_coords(findButton(root,name,prefix),&bounds);
        const int x=(bounds.x1+bounds.x2)/2, y=(bounds.y1+bounds.y2)/2;
        pointerEvent(input,x,y,true);
        pointerEvent(input,x,y,false);
        frame();
    };
    const auto publish=[&] {
        backend.publish(runtime.snapshot(),true,0,0);
        backend.publishMidi(runtime.usbInput().isSmk25(),false,runtime.usbInput().events.load());
        frame();
    };
    const auto& snap=runtime.snapshot();
    const auto block=[&] {
        runtime.apply(preview::g_sim_tick_ms);
        runtime.loadPendingPatch();
        assert(runtime.render());
        publish();
    };
    const auto capture=[&](const char* name) {
        char path[512];
        std::snprintf(path,sizeof(path),"%s/%s.ppm",argv[1],name);
        assert(preview::saveFramebufferPpm(path));
    };

    frame();
    click(app.rootObject(),"RITMO");
    assert(app.navigation().currentScreen()==ScreenId::Sequencer);
    assert(findLabel(content,"ENGINE UNAVAILABLE"));
    assert(runtime.begin());
    publish();
    assert(findLabel(content,"STOPPED") && app.navigation().currentScreen()==ScreenId::Sequencer);
    assert(findLabel(content,"01-") && findLabel(content,"16-"));
    capture("empty");

    const auto cell=[&](int step,int note) {
        const int x=61+45*step+20, y=kContentTop+126+(71-note)*17+8;
        pointerEvent(input,x,y,true);
        pointerEvent(input,x,y,false);
        frame();
    };
    cell(0,60);
    assert(findLabel(content,"COMMAND PENDING"));
    assert(!snap.sequencer_pattern.steps[0].enabled);
    block();
    assert(snap.sequencer_pattern.steps[0].enabled);
    cell(0,60);
    assert(adapter.sequencerResult().state==UiCommandState::Confirmed);
    cell(0,65);
    block();
    assert(findLabel(content,"STEP 01  NOTE F4") && snap.sequencer_pattern.steps[0].note==65);
    click(content,"OCT +");
    assert(findLabel(content,"C5"));
    click(content,"OCT -");
    assert(findLabel(content,"C4"));
    click(content,"ENABLED / OFF");
    block();
    assert(!snap.sequencer_pattern.steps[0].enabled && findLabel(content,"REST / ENABLE"));
    click(content,"REST / ENABLE");
    block();
    assert(snap.sequencer_pattern.steps[0].enabled);

    const auto slider=[&](const char* label,int value) {
        auto* title=findLabel(content,label,true);
        assert(title);
        auto* control=lv_obj_get_child(content,lv_obj_get_index(title)+1);
        assert(lv_obj_check_type(control,&lv_slider_class));
        lv_slider_set_value(control,value,LV_ANIM_OFF);
        lv_obj_send_event(control,LV_EVENT_VALUE_CHANGED,nullptr);
        assert(adapter.sequencerResult().state!=UiCommandState::Requested);
        lv_obj_send_event(control,LV_EVENT_RELEASED,nullptr);
        assert(adapter.sequencerResult().state==UiCommandState::Requested);
        block();
    };
    slider("VELOCITY",81);
    slider("GATE",99);
    assert(snap.sequencer_pattern.steps[0].velocity==81 && snap.sequencer_pattern.steps[0].gate==99);
    capture("selected");
    click(content,"LOAD DEMO PATTERN");
    assert(findLabel(content,"TAP LOAD DEMO AGAIN"));
    click(content,"LOAD DEMO PATTERN");
    block();
    assert(snap.sequencer_pattern.steps[0].note==60 && !snap.sequencer_pattern.steps[1].enabled);
    capture("demo");
    slider("LENGTH",4);
    slider("SWING",75);
    slider("TRANSPOSE",12);
    assert(snap.sequencer.pattern_length==4 && snap.sequencer.swing==75 && snap.sequencer.transpose==12);
    click(content,"DIV ",true);
    block();
    assert(snap.sequencer.division==tempo::Division::ThirtySecond);
    click(content,"SCALE QUANTIZE OFF");
    block();
    assert(snap.sequencer.quantize && findLabel(content,"SCALE QUANTIZE ON"));
    capture("quantize");

    pointerEvent(input,380,20,true);
    pointerEvent(input,380,20,false);
    frame();
    assert(findLabel(app.rootObject(),"GLOBAL TEMPO  /  INTERNAL"));
    click(app.rootObject(),"+1 BPM");
    block();
    assert(snap.sequencer.bpm==121 && snap.arp.tempo.bpm==121);
    click(app.rootObject(),"FECHAR");
    assert(adapter.requestParameterInt("perf.bpm",240).accepted());
    block();
    assert(snap.sequencer.bpm==240 && snap.arp.tempo.bpm==240);
    auto& usb=runtime.usbInput();
    usb.commands.push({amy::CommandType::NoteOn,52,100,0,0,preview::g_sim_tick_ms});
    click(content,"PLAY");
    assert(!snap.sequencer.playing && findLabel(content,"COMMAND PENDING"));
    block();
    assert(snap.sequencer.playing && findLabel(content,"PLAYING"));
    const auto waitFor=[&](uint8_t step) {
        for(int i=0;i<55 && snap.sequencer.current_step!=step;++i) block();
        assert(snap.sequencer.current_step==step);
    };
    waitFor(0);
    const auto highlighted=[&](const char* step) {
        return lv_color_eq(lv_obj_get_style_bg_color(findButton(content,step,true),LV_PART_MAIN),
                           lv_color_hex(palette::kAccentPrimary));
    };
    assert(highlighted("01+"));
    assert(snap.active_notes==2 && snap.last_notes[0]==72);
    capture("playing");
    cell(2,61);
    block();
    assert(snap.sequencer_pattern.steps[2].note==61 && snap.sequencer.playing);
    slider("VELOCITY",127);
    slider("GATE",1);
    waitFor(2);
    assert(highlighted("03+") && !highlighted("01+"));
    assert(!snap.harmony.scale_on && snap.last_notes[0]==74);

    const auto started=std::chrono::steady_clock::now();
    for(int i=0;i<180;++i) {
        block();
        assert(snap.sequencer.current_step<4);
    }
    const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now()-started).count();
    std::printf("MEASURE host render + publication + LVGL service and refresh %.2f us/block\n",double(elapsed)/180);

    click(app.rootObject(),"FX");
    assert(app.navigation().currentScreen()==ScreenId::Effects && snap.sequencer.playing);
    block();
    click(app.rootObject(),"RITMO");
    assert(findLabel(content,"PLAYING") && findLabel(content,"STEP 03  NOTE C#4"));
    click(content,"RESTART");
    block();
    waitFor(0);
    for(unsigned i=0;i<seq::kQueueCapacity;++i) {
        seq::Request request;
        request.token=runtime.commands().nextToken();
        assert(runtime.commands().pushSequencer(request));
    }
    click(content,"DIV ",true);
    assert(findLabel(content,"COMMAND QUEUE FULL"));
    click(content,"STOP");
    assert(runtime.commands().sequencer.overflows()==1);
    block();
    assert(!snap.sequencer.playing && snap.active_notes==1 && findLabel(content,"STOPPED"));
    assert(!highlighted("01+") && !highlighted("03+"));

    seq::Request bad;
    bad.action=seq::Action::SetLength;
    bad.value=-1;
    assert(adapter.requestSequencer(bad).accepted());
    block();
    assert(adapter.sequencerResult().state==UiCommandState::Rejected);
    assert(findLabel(content,"REJECTED / INVALID VALUE"));
    capture("rejected");
    click(content,"PLAY");
    preview::g_sim_tick_ms+=2001;
    frame();
    assert(findLabel(content,"CONFIRMATION TIMEOUT"));
    click(app.rootObject(),"PANIC");
    block();
    assert(!snap.active_notes && !snap.sequencer.playing);
    assert(adapter.requestParameterInt("fx.delay.mode",int(fx::DelayMode::Sync)).accepted());
    block();
    assert(adapter.requestParameterInt("arp.enable",1).accepted());
    block();
    usb.commands.push({amy::CommandType::NoteOn,55,100,0,0,preview::g_sim_tick_ms});
    click(content,"PLAY");
    for(int i=0;i<55 && !(snap.arp.sounding && snap.active_notes>=2);++i) block();
    assert(snap.sequencer.playing && snap.arp.sounding && snap.active_notes>=2);
    assert(snap.fx.config.delay_mode==fx::DelayMode::Sync);
    assert(snap.fx.delay_target_ms==tempo::durationMs(snap.fx.config.delay_division,snap.sequencer.bpm));
    click(app.rootObject(),"FX");
    click(app.rootObject(),"PANIC");
    block();
    assert(!snap.sequencer.playing && !snap.arp.sounding && !snap.active_notes);
    click(app.rootObject(),"RITMO");
    click(content,"PLAY");
    block();
    assert(snap.sequencer.playing);
    assert(adapter.patchNeedsDiscardConfirmation());
    assert(adapter.confirmPatchDiscard(129,0).accepted());
    for(int i=0;i<10 && snap.patch_id!=129;++i) block();
    assert(snap.patch_id==129 && !snap.sequencer.playing && snap.sequencer_pattern.steps[0].enabled);
    click(content,"CLEAR PATTERN");
    assert(findLabel(content,"TAP CLEAR AGAIN"));
    click(content,"CLEAR PATTERN");
    block();
    for(const auto& step:snap.sequencer_pattern.steps) assert(!step.enabled);
    for(int i=0;i<int(seq::kSteps);++i) {
        cell(i,60+i%12);
        block();
        assert(snap.sequencer_pattern.steps[i].enabled && snap.sequencer_pattern.steps[i].note==60+i%12);
    }
    assert(fake_nvs::writes==0);

    const auto freeBytes=[] {
        lv_mem_monitor_t monitor{};
        lv_mem_monitor(&monitor);
        return monitor.free_size;
    };
    click(content,"MIDI / REC");
    assert(findLabel(content,"MIDI / RECORD SETTINGS"));
    const size_t settings_free=freeBytes();
    runtime.usbInput().identifyDevice(0x4353,0x4b4d);
    click(content,"STEP INPUT");
    block();
    assert(findLabel(content,"STEP INPUT +12"));
    const auto midi=[&](uint8_t status,uint8_t note,uint8_t velocity) {
        const uint8_t packet[]{uint8_t(status>>4),status,note,velocity};
        assert(runtime.usbInput().packet(packet,preview::g_sim_tick_ms));
    };
    midi(0x90,72,112);
    block();
    assert(snap.sequencer_pattern.steps[15].note==72);
    assert(findLabel(content,"STEP 16   LAST C5 / VEL 112   SMK25 OK",true));
    capture("midi-settings");
    midi(0x80,72,0);
    click(content,"CLOSE");
    click(content,"04",true);
    block();
    click(content,"MIDI / REC");
    click(content,"STEP RECORD");
    block();
    assert(snap.sequencer.midi.selected==3);
    click(content,"CLOSE");
    midi(0x90,64,83);
    block();
    assert(snap.sequencer.midi.selected==0 && snap.sequencer_pattern.steps[3].velocity==83);
    assert(findLabel(content,"STEP 01  NOTE C4"));
    capture("step-record");
    click(content,"STEP 01  NOTE C4");
    assert(findLabel(content,"EDIT SELECTED STEP"));
    const size_t editor_free=freeBytes();
    click(content,"SEMITONE +");
    click(content,"SEMITONE +");
    click(content,"OCTAVE +");
    for(int i=0;i<3;++i) block();
    assert(snap.sequencer_pattern.steps[0].note==74);
    capture("step-editor");
    click(content,"CLOSE");
    click(content,"REST / NEXT");
    block();
    assert(snap.sequencer.midi.selected==1 && !snap.sequencer_pattern.steps[0].enabled);

    // The queued slider value replaces only its adjacent, unsent predecessor.
    seq::Request edit;
    edit.action=seq::Action::SetStepGate;
    edit.index=1;
    for(int value:{51,52,53}) {
        edit.value=int16_t(value);
        assert(adapter.requestSequencer(edit).accepted());
    }
    midi(0x90,84,109);
    block();
    block();
    assert(snap.sequencer_pattern.steps[1].note==84 && snap.sequencer_pattern.steps[1].velocity==109);
    assert(snap.sequencer_pattern.steps[1].gate==53);
    click(content,"MIDI / REC");
    click(content,"LIVE TRANSPOSE OFF");
    assert(findLabel(content,"ANY CAPTURES ALL MUSICAL NOTES.",true));
    assert(!snap.sequencer.midi.live);
    click(content,"LIVE TRANSPOSE OFF");
    block();
    click(content,"PLAY");
    block();
    midi(0x90,62,90);
    block();
    assert(snap.sequencer.playing && snap.sequencer.midi.effective==14 &&
           findLabel(content,"LIVE TRANSPOSE ON"));
    capture("live-transpose");
    click(app.rootObject(),"FX");
    click(app.rootObject(),"RITMO");
    assert(snap.sequencer.midi.mode==seq::InputMode::StepRecord);
    runtime.usbInput().disconnect();
    block();
    assert(!snap.sequencer.playing && snap.sequencer.midi.mode==seq::InputMode::Normal &&
           findLabel(content,"NORMAL +12"));
    click(content,"MIDI / REC");
    assert(findLabel(content,"STEP 03   LAST -- / VEL 0   USB WAITING",true));
    runtime.usbInput().identifyDevice(0x4353,0x4b4d);
    block();
    assert(snap.sequencer.midi.smk_connected);
    click(content,"CLOSE");
    capture("normal");
    click(content,"MIDI / REC");
    click(content,"STEP INPUT");
    click(content,"STEP RECORD");
    click(content,"NORMAL PLAY");
    for(unsigned i=0;i<3;++i) block();
    assert(snap.sequencer.midi.mode==seq::InputMode::Normal);
    click(content,"CLOSE");
    edit.action=seq::Action::ShiftStepNote;
    edit.index=0;
    edit.value=1;
    for(unsigned i=0;i<=seq::kQueueCapacity;++i)
        assert(adapter.requestSequencer(edit).accepted());
    assert(adapter.requestSequencer(edit).state==UiCommandState::Rejected);
    frame();
    assert(findLabel(content,"EDIT BUFFER FULL"));
    capture("edit-buffer-full");
    for(unsigned i=0;i<=seq::kQueueCapacity;++i) block();
    assert(snap.sequencer_pattern.steps[0].note==79);
    std::printf("MEASURE LVGL panel heap free: settings=%zu bytes, editor=%zu bytes\n",settings_free,editor_free);

    click(app.rootObject(),"FX");
    click(app.rootObject(),"RITMO");
    const size_t mounted=freeBytes();
    click(app.rootObject(),"FX");
    const size_t unmounted=freeBytes();
    for(int i=0;i<40;++i) {
        click(app.rootObject(),"RITMO");
        assert(freeBytes()==mounted);
        click(app.rootObject(),"FX");
        assert(freeBytes()==unmounted);
    }
    std::printf("MEASURE LVGL heap free: sequencer=%zu bytes, FX=%zu bytes\n",mounted,unmounted);
    NvsPatternStore patterns;
    assert(patterns.begin());
    adapter.setPatternStore(patterns);
    click(app.rootObject(),"FX");
    click(app.rootObject(),"RITMO");
    runtime.commands().panic();
    block();
    const auto browser_heap=freeBytes();
    click(content,"PATTERNS");
    assert(findLabel(content,"01 EMPTY") && findLabel(content,"16 EMPTY"));
    const auto writes_before_cancel=fake_nvs::writes;
    click(content,"SAVE");
    for(int i=0;i<10 && std::strcmp(adapter.patternNotice(),"READY TO WRITE / CANCEL AVAILABLE");++i)
        block();
    assert(std::strcmp(adapter.patternNotice(),"READY TO WRITE / CANCEL AVAILABLE")==0);
    adapter.requestPattern({ui::PatternAction::Cancel,0});
    assert(std::strcmp(adapter.patternNotice(),"CANCELLED")==0 && !adapter.patternBusy());
    for(int i=0;i<3;++i) block();
    assert(fake_nvs::writes==writes_before_cancel && patterns.load(0).state==seq::SlotState::Empty);
    click(content,"SAVE");
    assert(adapter.patternBusy());
    assert(findLabel(content,"PATTERN MODIFIED   REQUESTED",true));
    for(int i=0;i<10;++i) block();
    assert(!adapter.patternBusy() && snap.sequencer.active_slot==0 && !snap.sequencer.modified);
    assert(findLabel(content,"01 PATTERN 01 *"));
    capture("pattern-library");
    auto* name=findType(content,&lv_textarea_class);
    assert(name);
    lv_obj_send_event(name,LV_EVENT_CLICKED,nullptr);
    auto* keyboard=findType(content,&lv_keyboard_class);
    assert(keyboard);
    lv_textarea_set_text(name,"Opening");
    lv_obj_send_event(keyboard,LV_EVENT_READY,nullptr);
    click(content,"RENAME");
    for(int i=0;i<10;++i) block();
    assert(findLabel(content,"01 Opening *"));
    click(content,"02 EMPTY");
    click(content,"SAVE");
    for(int i=0;i<10;++i) block();
    assert(snap.sequencer.active_slot==1 && !snap.sequencer.modified);
    click(content,"CLOSE");
    edit.value=40;
    assert(adapter.requestSequencer(edit).accepted());
    block();
    assert(snap.sequencer.modified);
    click(content,"PATTERNS");
    click(content,"01 Opening");
    click(content,"LOAD");
    assert(findLabel(content,"PATTERN MODIFIED   CHOOSE",true));
    capture("pattern-discard");
    click(content,"DISCARD & LOAD");
    block();
    assert(snap.sequencer.active_slot==0 && !snap.sequencer.modified);
    click(content,"CLOSE");
    click(content,"PLAY");
    block();
    click(content,"PATTERNS");
    click(content,"02 PATTERN 02");
    click(content,"SWITCH MODE");
    click(content,"LOAD");
    block();
    assert(snap.sequencer.pending_slot==1);
    assert(findLabel(content,"02 PATTERN 02 >"));
    capture("pattern-next-bar");
    click(content,"CANCEL QUEUE");
    block();
    assert(snap.sequencer.pending_slot==seq::kNoSlot);
    click(content,"CLOSE");
    click(content,"STOP");
    block();
    click(content,"PATTERNS");
    click(content,"02 PATTERN 02");
    click(content,"DELETE");
    click(content,"DELETE");
    for(int i=0;i<10;++i) block();
    assert(findLabel(content,"02 EMPTY"));
    click(content,"LOAD");
    assert(findLabel(content,"PATTERN CLEAN   REJECTED",true));
    capture("pattern-error");
    click(content,"CLOSE");
    capture("pattern-active");
    // LVGL alternates two allocations after a close. Warm both paths and
    // compare the same point in that cycle to catch cumulative leakage.
    size_t settled[2]{};
    for(int i=0;i<28;++i) {
        click(content,"PATTERNS");
        click(content,"CLOSE");
        if(i==2 || i==3) settled[i%2]=freeBytes();
        if(i>=4) assert(freeBytes()==settled[i%2]);
    }
    std::printf("MEASURE pattern browser closed free before=%zu after=%zu bytes\n",browser_heap,freeBytes());
    puts("PASS 017A LVGL browser save/load, dirty confirmation, queued/cancel, rename/delete, errors and lifecycle");

    NvsSceneStore scenes;
    assert(scenes.begin());
    adapter.setSceneStore(scenes);
    setSessionLockMask(0);
    assert(app.navigation().returnToPerformance());
    frame();
    click(content,"SELETIVO >");
    assert(app.navigation().currentScreen()==ScreenId::Scenes);
    assert(findLabel(content,"08 EMPTY"));
    capture("scenes-empty");
    const auto empty_scene_heap=freeBytes();
    click(content,"SAVE");
    click(content,"SAVE SCENE");
    block();
    click(content,"CANCEL QUEUED");
    block();
    assert(adapter.sceneSlot(0)->state==seq::SlotState::Empty);

    for(unsigned index=0;index<scene::kSlots;++index) {
        click(content,"SAVE");
        click(content,"SAVE SCENE");
        for(unsigned i=0;i<8;++i) block();
        assert(adapter.sceneSlot(index)->state==seq::SlotState::Valid);
        assert(adapter.sceneView().reference==index && adapter.sceneView().dirty==scene::Dirty::Clean);
        if(index+1<scene::kSlots) click(content,"NEXT");
    }
    capture("scenes-eight");
    click(content,"RENAME");
    auto* scene_name=findType(content,&lv_textarea_class);
    assert(scene_name);
    lv_textarea_set_text(scene_name,"CODA");
    click(content,"RENAME SCENE");
    for(unsigned i=0;i<8;++i) block();
    assert(std::strcmp(scenes.load(7).config.name,"CODA")==0);

    click(content,"LOAD");
    assert(findLabel(content,"SELECTIVE RECALL 08",true));
    capture("scenes-selective");
    std::printf("MEASURE LVGL free with empty Scenes=%zu and Selective Recall modal=%zu bytes\n",
                empty_scene_heap,freeBytes());
    click(content,"IMMEDIATE");
    click(content,"SCENE TEMPO OFF");
    click(content,"FULL RECALL");
    block();
    assert(snap.scene.phase==scene::RecallPhase::WaitingBoundary);
    capture("scenes-pending");
    click(content,"CANCEL QUEUED");
    block();
    assert(snap.scene.phase==scene::RecallPhase::Cancelled);
    click(content,"LOAD");
    click(content,"NEXT BAR");
    click(content,"FULL RECALL");
    block();
    assert(snap.scene.active==7 && snap.scene.phase==scene::RecallPhase::Confirmed);
    assert(adapter.sceneView().dirty==scene::Dirty::Clean);
    capture("scenes-active");

    assert(backend.setArp(arp::Action::Bpm,180,preview::g_sim_tick_ms));
    block();
    assert(adapter.sceneView().dirty==scene::Dirty::Modified);
    capture("scenes-dirty");
    SceneOperation selective;
    selective.slot=7;
    selective.domains=scene::Patch;
    adapter.requestScene(selective);
    block();
    assert(adapter.sceneView().dirty==scene::Dirty::Modified && adapter.sceneView().selective);
    assert(snap.scene.active_domains==scene::Patch && snap.arp.tempo.bpm==180);
    assert(backend.setArp(arp::Action::Bpm,181,preview::g_sim_tick_ms));
    block();
    assert(adapter.sceneView().dirty==scene::Dirty::Modified && !adapter.sceneView().selective);
    click(content,"DELETE");
    click(content,"DELETE SCENE");
    for(unsigned i=0;i<8;++i) block();
    assert(adapter.sceneSlot(7)->state==seq::SlotState::Empty);
    assert(adapter.sceneView().dirty==scene::Dirty::Invalid);

    scene::Slot interruption;
    interruption.state=seq::SlotState::Valid;
    auto& config=interruption.config;
    std::strcpy(config.name,"PATCH TEST");
    config.domains=scene::Patch|scene::Tempo;
    config.patch_id=snap.patch_id==4?128:4;
    config.bpm=140;
    assert(scenes.commit(7,interruption));
    adapter.setSceneStore(scenes);
    click(content,"LOAD");
    click(content,"FULL RECALL");
    block();
    assert(snap.scene.phase==scene::RecallPhase::WaitingPatch && snap.scene.applied_domains==scene::Patch);
    click(content,"PANIC");
    block();
    assert(findLabel(content,"PARTIAL APPLY") && snap.active_notes==0);
    capture("scenes-partial");
    config.patch_id=UINT16_MAX;
    assert(scenes.commit(7,interruption));
    adapter.setSceneStore(scenes);
    click(content,"LOAD");
    click(content,"FULL RECALL");
    block();
    assert(findLabel(content,"RECALL FAILED"));
    capture("scenes-failed");

    SceneOperation queued;
    queued.slot=0;
    queued.mode=scene::RecallMode::NextBar;
    adapter.requestScene(queued);
    block();
    assert(snap.scene.phase==scene::RecallPhase::WaitingBoundary);
    SceneOperation replacement;
    replacement.slot=1;
    adapter.requestScene(replacement);
    block();
    block();
    if(snap.scene.phase==scene::RecallPhase::WaitingPatch) block();
    assert(snap.scene.phase==scene::RecallPhase::Confirmed && snap.scene.active==1);
    assert(app.navigation().returnToPerformance());
    frame();
    lv_area_t macro_bounds{},quick_bounds{};
    lv_obj_get_coords(lv_obj_get_parent(findLabel(content,"SHAPE")),&macro_bounds);
    lv_obj_get_coords(findButton(content,"SELETIVO >"),&quick_bounds);
    assert(macro_bounds.y2<quick_bounds.y1);
    click(content,"C3 ",true);
    block();
    if(snap.scene.phase==scene::RecallPhase::WaitingPatch) block();
    assert(snap.scene.phase==scene::RecallPhase::Confirmed && snap.scene.active==2);
    capture("scenes-quick");

    const auto pattern_id=scenes.load(2).config.pattern_slot;
    auto revised_pattern=patterns.load(pattern_id);
    assert(revised_pattern.state==seq::SlotState::Valid);
    assert(patterns.commit(pattern_id,revised_pattern));
    adapter.setPatternStore(patterns);
    adapter.requestScene({SceneAction::Load,2});
    block();
    assert(adapter.sceneView().dirty==scene::Dirty::Invalid);
    assert(std::strcmp(adapter.sceneView().notice,"PATTERN REFERENCE CHANGED")==0);

    NvsSceneStore reopened;
    assert(reopened.begin());
    for(unsigned i=0;i<scene::kSlots;++i) assert(reopened.load(i).state==seq::SlotState::Valid);
    puts("PASS 017B LVGL scene catalog, capture, write, rename, recall, cancellation, dirty, delete and partial apply");

    app.shutdown();
    lv_indev_delete(input);
    runtime.end();
    puts("PASS real LVGL/AMY sequencer controls, navigation, ownership, timeout, queue pressure, Panic and lifecycle");
}
