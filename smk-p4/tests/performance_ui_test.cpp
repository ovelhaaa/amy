#include "performance_ui.h"
#include "local_performance_backend.h"
#include "esp_timer.h"
#include <cassert>
#include <cstdio>
using namespace smk::p4;
static lv_obj_t* at(int x,int y) {
    for(unsigned i=1;i<mock_count;++i) if(mock_objects[i].callback && mock_objects[i].x==x && mock_objects[i].y==y) return &mock_objects[i];
    assert(false); return nullptr;
}
int main() {
    amy::Runtime runtime;
    LocalAmyPerformanceBackend backend(runtime.commands());
    PerformanceUi ui(backend); assert(ui.begin()); assert(!runtime.commands().depth());
    assert(lv_obj_has_state(at(18,392),LV_STATE_DISABLED));
    assert(runtime.begin());
    bool output_live=false;
    const auto service=[&](uint32_t ms) {
        mock_time_us=ms*1000;
        backend.publish(runtime.snapshot(),output_live,100,0); ui.service(ms); runtime.apply(ms);
    };
    service(0); assert(!runtime.commands().depth());
    lv_obj_t* audio=nullptr;
    for(unsigned i=1;i<mock_count;++i) if(mock_objects[i].x==416 && mock_objects[i].y==14) audio=&mock_objects[i];
    assert(audio && std::strcmp(audio->text,"AUDIO RAM")==0);
    output_live=true;
    mock_event(at(18,392),LV_EVENT_PRESSED); service(1); assert(runtime.snapshot().active_notes==1);
    assert(std::strcmp(audio->text,"AUDIO RUN")==0);
    mock_event(at(18,392),LV_EVENT_PRESS_LOST); service(2); assert(!runtime.snapshot().active_notes);
    mock_event(at(18,392),LV_EVENT_PRESSED); service(3);
    mock_event(at(702,318),LV_EVENT_CLICKED); service(4); assert(!runtime.snapshot().active_notes);
    mock_event(at(18,392),LV_EVENT_PRESSED); service(5);
    mock_event(at(674,4),LV_EVENT_CLICKED); service(6); assert(!runtime.snapshot().active_notes);
    service(7); assert(!runtime.snapshot().active_notes);
    lv_obj_t* midi=nullptr;
    for(unsigned i=1;i<mock_count;++i) if(mock_objects[i].x==320 && mock_objects[i].y==14) midi=&mock_objects[i];
    assert(midi && std::strcmp(midi->text,"MIDI OFF")==0);
    backend.publishMidi(true,false,1); service(10); assert(std::strcmp(midi->text,"MIDI *")==0);
    service(300); assert(std::strcmp(midi->text,"MIDI ON")==0);
    backend.publishMidi(false,true,1); service(310); assert(std::strcmp(midi->text,"MIDI ERROR")==0);
    backend.publishMidi(false,false,1); service(320); assert(std::strcmp(midi->text,"MIDI OFF")==0);
    mock_event(at(570,50),LV_EVENT_CLICKED); service(500); service(1000);
    assert(runtime.snapshot().patch_id==135);
    service(1001); mock_event(at(18,392),LV_EVENT_PRESSED); service(1002);
    assert(runtime.snapshot().active_notes==1);
    mock_event(at(18,392),LV_EVENT_RELEASED); service(1003); assert(!runtime.snapshot().active_notes);
    auto* volume=at(142,334); volume->value=30; mock_event(volume,LV_EVENT_VALUE_CHANGED); service(1100);
    assert(runtime.snapshot().master==30);
    mock_event(at(644,50),LV_EVENT_CLICKED);
    for(unsigned i=0;i<12;++i) service(1200+i);
    assert(!lv_obj_has_state(at(20,82),LV_STATE_DISABLED));
    assert(lv_obj_has_state(at(20,82+41*amy::kPatchCount),LV_STATE_DISABLED));
    backend.panic(); runtime.apply(1300);
    // Recreating widgets sends no musical commands or reset to the owner.
    mock_count=1; for(auto& o:mock_objects) o={};
    PerformanceUi second(backend); assert(second.begin()); assert(!runtime.commands().depth());
    runtime.end(); service(1400); assert(lv_obj_has_state(at(18,392),LV_STATE_DISABLED));
    std::puts("PASS: real PerformanceUi callbacks with mock LVGL: boot, keyboard/release/leave, octave, Panic, ROM browser, patch, volume, UI recreation and engine error");
}
