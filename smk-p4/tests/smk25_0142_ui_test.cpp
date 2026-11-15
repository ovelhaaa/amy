#include "local_performance_backend.h"
#include "ui_blueprint_performance_adapter.h"
#include "../ui-preview/preview_harness.h"
#include <cassert>
#include <cstring>

using namespace smk::p4;
using namespace smk::p4::ui;

namespace {
lv_obj_t* findButton(lv_obj_t* root,const char* name) {
    if(lv_obj_check_type(root,&lv_button_class)){
        auto* label=lv_obj_get_child(root,0);
        if(label && lv_obj_check_type(label,&lv_label_class) &&
           std::strcmp(lv_label_get_text(label),name)==0) return root;
    }
    for(unsigned i=0;i<lv_obj_get_child_count(root);++i)
        if(auto* found=findButton(lv_obj_get_child(root,i),name)) return found;
    return nullptr;
}

void click(lv_obj_t* root,const char* name) {
    auto* button=findButton(root,name);
    assert(button);
    lv_obj_send_event(button,LV_EVENT_CLICKED,nullptr);
}
}

void testSmk25LegacyUi(amy::Runtime& runtime,LocalAmyPerformanceBackend& backend,
                      UiBlueprintPerformanceAdapter& adapter,UiBlueprint& ui) {
    using Action=IUiBackend::MidiAction;
    auto& usb=runtime.usbInput();
    const auto frame=[&](uint32_t elapsed=20) {
        preview::pumpFrame(ui,elapsed);
        for(unsigned i=0;i<2;++i){
            runtime.apply(preview::g_sim_tick_ms);
            runtime.loadPendingPatch();
            assert(runtime.render());
            backend.publish(runtime.snapshot(),false,0,0);
            preview::pumpFrame(ui);
        }
    };
    const auto packet=[&](uint8_t status,uint8_t number,uint8_t value) {
        const uint8_t bytes[]{uint8_t(status>>4),status,number,value};
        assert(usb.transfer(bytes,sizeof(bytes),preview::g_sim_tick_ms));
    };
    const auto midi=[&](uint8_t status,uint8_t number,uint8_t value,uint32_t elapsed=20) {
        preview::g_sim_tick_ms+=elapsed;
        packet(status,number,value);
        frame();
    };

    assert(ui.navigation().navigateTo(ScreenId::MidiSettings,10));
    auto* content=ui.contentObject();
    click(content,"RESTORE SMK25 DEFAULTS");
    assert(std::strcmp(adapter.midiProfileName(),"P4 CUSTOM PROFILE")==0);
    click(content,"RESTORE SMK25 DEFAULTS");
    assert(std::strstr(adapter.midiProfileName(),"LEGACY S3"));
    usb.identifyDevice(0x4353,0x4b4d);
    frame();

    assert(adapter.requestParameterInt("perf.macro.1",64).accepted());
    frame();
    assert(adapter.requestParameterFloat("fx.chorus.depth",50).accepted());
    frame();
    midi(0xb1,21,100);
    midi(0xb0,21,15);
    midi(0xb0,29,20);
    midi(0xb0,21,80);
    assert(runtime.snapshot().edit.performance_macros[0]==64);
    midi(0xb0,21,81);
    assert(runtime.snapshot().edit.performance_macros[0]==81);
    midi(0xb0,29,80);
    assert(runtime.snapshot().fx.config.chorus_depth==0.5f);
    midi(0xb0,29,90);
    assert(runtime.snapshot().fx.config.chorus_depth>0.69f);
    char observation[128]{};
    adapter.midiObservationText(observation,sizeof(observation));
    assert(std::strstr(observation,"KNOBS B") && std::strstr(observation,"CC 29"));
    adapter.midiPerformanceAction(Action::NextBank,0);
    char binding[128]{};
    adapter.midiKnobText(7,binding,sizeof(binding));
    assert(std::strstr(binding,"EQ LOW") && std::strstr(binding,"CC36"));
    adapter.midiPerformanceAction(Action::NextBank,0);
    assert(adapter.requestParameterInt("perf.macro.1",50).accepted());
    frame();
    midi(0xb0,21,100);
    assert(runtime.snapshot().edit.performance_macros[0]==50);
    midi(0x99,36,87);
    adapter.midiPadText(0,binding,sizeof(binding));
    assert(std::strstr(binding,"DOWN") && std::strstr(binding,"VEL 87"));
    assert(runtime.snapshot().active_notes==0);
    midi(0x99,36,0);
    adapter.midiPadText(0,binding,sizeof(binding));
    assert(std::strstr(binding,"UP"));

    // Dirty editor trigger uses the same confirmation as the touchscreen.
    const auto patch=runtime.snapshot().patch_id;
    midi(0x99,45,100);
    assert(findButton(content,"DISCARD & LOAD") && runtime.snapshot().patch_id==patch);
    click(content,"CANCEL");
    midi(0x89,45,0);
    assert(runtime.snapshot().patch_id==patch);
    midi(0x99,46,100);
    assert(ui.navigation().currentScreen()==ScreenId::Performance);
    midi(0x89,46,0);
    midi(0x99,49,100);
    assert(ui.navigation().currentScreen()==ScreenId::PatchBrowser);
    midi(0x89,49,0);
    midi(0x99,48,100);
    assert(ui.navigation().currentScreen()==ScreenId::Performance);
    midi(0x89,48,0);
    midi(0x99,47,100);
    assert(std::strcmp(adapter.midiLearnStatus(),"ARP ON")==0);
    midi(0x99,47,100);
    assert(runtime.snapshot().arp.config.enabled);
    midi(0x89,47,0);

    assert(ui.navigation().navigateTo(ScreenId::Arpeggiator,10));
    content=ui.contentObject();
    assert(adapter.parameterStatus("arp.bpm")==FeatureStatus::Available);
    assert(adapter.parameterStatus("perf.bpm")==FeatureStatus::Available);
    assert(!adapter.isParameterEditable("arp.clock_source"));
    assert(adapter.requestParameterInt("arp.bpm",90).state==UiCommandState::Requested);
    assert(adapter.model().getInt("arp.bpm")==120);
    frame();
    assert(adapter.model().getInt("perf.bpm")==90);
    click(content,"TAP");
    click(ui.rootObject(),findButton(ui.rootObject()," 90 BPM INT")?" 90 BPM INT":"*90 BPM INT");
    for(unsigned i=0;i<3;++i) {
        preview::g_sim_tick_ms+=500;
        auto* panel=lv_obj_get_child(ui.rootObject(),-1);
        click(panel,"TAP");
    }
    frame();
    assert(adapter.model().getInt("arp.bpm")==120);
    click(lv_obj_get_child(ui.rootObject(),-1),"X");
    midi(0x90,60,100);
    assert(runtime.snapshot().arp.sounding && runtime.snapshot().arp.held_count==1);
    for(unsigned i=0;i<amy::kCommandCapacity;++i)
        assert(runtime.commands().push({amy::CommandType::Master,0,64,0,0,preview::g_sim_tick_ms}));
    click(content,"STOP");
    frame();
    assert(!runtime.snapshot().arp.sounding && !runtime.snapshot().arp.length);
    click(content,"PLAY");
    frame();
    assert(runtime.snapshot().arp.armed);
    midi(0x99,47,100);
    assert(std::strcmp(adapter.midiLearnStatus(),"ARP OFF")==0);
    midi(0x89,47,0);
    assert(adapter.requestParameterInt("perf.bpm",40).accepted());
    frame();
    assert(adapter.requestParameterInt("fx.delay.mode",1).accepted());
    frame();
    midi(0xb0,30,0); // pickup at 1/8, the longest valid division at 40 BPM
    midi(0xb0,30,13); // close to the 1/8 to 1/8T boundary
    assert(runtime.snapshot().fx.config.delay_division==tempo::Division::Eighth);
    midi(0xb0,30,17);
    assert(runtime.snapshot().fx.config.delay_division==tempo::Division::EighthTriplet);
    midi(0xb0,30,13); // stay on the selected division during jitter
    assert(runtime.snapshot().fx.config.delay_division==tempo::Division::EighthTriplet);
    assert(adapter.requestParameterInt("fx.delay.mode",0).accepted());
    frame();
    midi(0x99,50,100);
    assert(ui.navigation().currentScreen()==ScreenId::MidiSettings);
    assert(ui.navigation().subTab(ScreenId::MidiSettings)==1);
    midi(0x89,50,0);

    auto* screen=lv_screen_active();
    midi(0x99,51,100);
    midi(0x89,51,0,600);
    assert(!findButton(screen,"SAVE AS"));
    midi(0x99,51,100);
    midi(0x99,51,100);
    midi(0x89,51,0,1250);
    assert(!findButton(screen,"SAVE AS"));
    midi(0x99,51,100);
    usb.disconnect();
    frame(1300);
    usb.identifyDevice(0x4353,0x4b4d);
    midi(0x89,51,0);
    assert(!findButton(screen,"SAVE AS"));
    const auto macro=runtime.snapshot().edit.performance_macros[0];
    midi(0xb0,21,4);
    assert(runtime.snapshot().edit.performance_macros[0]==macro);

    midi(0x99,51,100);
    for(unsigned i=0;i<amy::MidiControlEvents::kCapacity;++i) packet(0xb0,70,127);
    preview::g_sim_tick_ms+=1300;
    packet(0x89,51,0);
    frame();
    assert(!findButton(screen,"SAVE AS") && usb.controlDrops()>0);
    adapter.midiPadText(15,binding,sizeof(binding));
    assert(std::strstr(binding,"UP"));
    midi(0x99,51,100);
    frame(1300);
    assert(!findButton(screen,"SAVE AS"));
    midi(0x89,51,0);
    assert(findButton(screen,"SAVE AS"));
    click(screen,"CLOSE");

    const auto touch=adapter.requestNoteOn(64,100);
    assert(touch.accepted());
    midi(0x90,67,100);
    assert(runtime.snapshot().active_notes==2);
    midi(0xb0,64,127);
    midi(0x80,67,0);
    assert(runtime.snapshot().active_notes==2);
    midi(0xb0,115,127);
    assert(runtime.snapshot().active_notes==1);
    midi(0xb0,115,127);
    assert(runtime.snapshot().active_notes==1);
    midi(0xb0,115,0);
    assert(adapter.requestNoteOff(64,touch.token).accepted());
    frame();
    assert(runtime.snapshot().active_notes==0);

    midi(0x99,44,100);
    click(ui.contentObject(),"DISCARD & LOAD");
    frame();
    midi(0x89,44,0);
    assert(runtime.snapshot().patch_id!=patch);
    const auto loaded=runtime.snapshot().edit.performance_macros[0];
    midi(0xb0,21,loaded<64 ? 127 : 0);
    assert(runtime.snapshot().edit.performance_macros[0]==loaded);

    adapter.midiPerformanceAction(Action::ReloadProfile,0);
    assert(std::strcmp(adapter.midiProfileName(),"P4 CUSTOM PROFILE")==0);
    adapter.midiPerformanceAction(Action::RestoreLegacy,0);
    adapter.midiPerformanceAction(Action::RestoreLegacy,0);
    adapter.midiPerformanceAction(Action::NextBank,0);
    constexpr unsigned eqTarget=performance::kMidiTargetCount-2;
    assert(adapter.setMidiCustomTarget(0,eqTarget));
    adapter.midiPerformanceAction(Action::SaveProfile,0);
    frame();
    frame();
    assert(std::strcmp(adapter.midiLearnStatus(),"PROFILE SAVED")==0);
    adapter.midiPerformanceAction(Action::UseGeneric,0);
    adapter.midiPerformanceAction(Action::ReloadProfile,0);
    assert(std::strstr(adapter.midiProfileName(),"LEGACY S3"));
    adapter.midiPerformanceAction(Action::NextBank,0);
    assert(adapter.midiCustomTarget(0)==eqTarget);
    adapter.midiKnobText(0,binding,sizeof(binding));
    assert(std::strstr(binding,"CC29") && std::strstr(binding,"EQ LOW"));
    assert(adapter.setMidiCustomTarget(0,performance::kMidiTargetCount-1));
    assert(std::strstr(adapter.midiTargetName(performance::kMidiTargetCount-1),"TEMPO"));
    midi(0xb0,29,0);
    midi(0xb0,29,64);
    assert(runtime.snapshot().arp.tempo.bpm==141);
}
