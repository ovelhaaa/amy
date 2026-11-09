#include "performance_ui.h"
#include "app_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstdio>
#include <cstring>

namespace smk::p4 {
namespace {
constexpr uint32_t kBackground=0x101820,kPanel=0x192630,kText=0xe6edf2,kMuted=0x80919f,kAccent=0xeab65b;
constexpr uint8_t kWhiteNotes[]={0,2,4,5,7,9,11,12};
constexpr uint8_t kBlackNotes[]={1,3,6,8,10};
constexpr uint8_t kBlackBoundary[]={1,2,4,5,6};
constexpr const char* kWhiteNames[]={"C","D","E","F","G","A","B","C"};
uint32_t nowMs() { return static_cast<uint32_t>(esp_timer_get_time()/1000); }
void panel(lv_obj_t* object) {
    lv_obj_set_style_bg_color(object,lv_color_hex(kBackground),0);
    lv_obj_set_style_border_width(object,0,0); lv_obj_set_style_pad_all(object,0,0);
    lv_obj_remove_flag(object,LV_OBJ_FLAG_SCROLLABLE);
}
}
void PerformanceUi::enable(lv_obj_t* object,bool enabled) {
    if(!object) return;
    if(enabled && lv_obj_has_state(object,LV_STATE_DISABLED)) lv_obj_remove_state(object,LV_STATE_DISABLED);
    else if(!enabled && !lv_obj_has_state(object,LV_STATE_DISABLED)) lv_obj_add_state(object,LV_STATE_DISABLED);
}
void PerformanceUi::text(lv_obj_t* object,char* buffer,size_t capacity,const char* value) {
    if(!object || capacity == 0 || std::strncmp(buffer,value,capacity)==0) return;
    size_t i = 0;
    while(i + 1 < capacity && value[i] != '\0') { buffer[i] = value[i]; ++i; }
    buffer[i] = '\0';
    lv_label_set_text_static(object,buffer);
}
PerformanceUi::Context* PerformanceUi::context(Action action,uint8_t index) {
    if(contexts_used_>=64) { failed_=true; return nullptr; }
    auto* result=&contexts_[contexts_used_++]; *result={this,action,index}; return result;
}
lv_obj_t* PerformanceUi::label(lv_obj_t* parent,const char* value,int x,int y,int width,const lv_font_t* font) {
    if(!parent) { failed_=true; return nullptr; }
    auto* object=lv_label_create(parent);
    if(!object) { failed_=true; return nullptr; }
    lv_obj_set_pos(object,x,y); lv_obj_set_size(object,width,lv_font_get_line_height(font)+2);
    lv_obj_set_style_text_font(object,font,0); lv_obj_set_style_text_color(object,lv_color_hex(kText),0);
    lv_label_set_long_mode(object,LV_LABEL_LONG_MODE_CLIP); lv_label_set_text(object,value); return object;
}
lv_obj_t* PerformanceUi::button(lv_obj_t* parent,const char* value,int x,int y,int width,int height,Action action,uint8_t index) {
    if(!parent) { failed_=true; return nullptr; }
    auto* object=lv_button_create(parent);
    if(!object) { failed_=true; return nullptr; }
    lv_obj_set_pos(object,x,y); lv_obj_set_size(object,width,height);
    lv_obj_set_style_bg_color(object,lv_color_hex(0x293b48),0);
    lv_obj_set_style_shadow_width(object,0,0); lv_obj_set_style_radius(object,6,0);
    lv_obj_set_style_border_width(object,1,0); lv_obj_set_style_border_color(object,lv_color_hex(0x3b4d59),0);
    lv_obj_remove_flag(object,LV_OBJ_FLAG_SCROLLABLE);
    auto* caption=label(object,value,0,0,width-8);
    if(caption) { lv_obj_set_style_text_align(caption,LV_TEXT_ALIGN_CENTER,0); lv_obj_center(caption); }
    auto* callback=context(action,index);
    if(callback) lv_obj_add_event_cb(object,event,LV_EVENT_ALL,callback); else enable(object,false);
    return object;
}
void PerformanceUi::header(lv_obj_t* parent,const char* title) {
    label(parent,"SMK-P4",18,10,120);
    label(parent,title,150,14,160,&lv_font_montserrat_14);
    auto* panic=button(parent,"PANIC",674,4,110,38,Action::Panic);
    if(panic) lv_obj_set_style_bg_color(panic,lv_color_hex(0xa73939),0);
    panic_[parent==performance_?0:1]=panic;
}
bool PerformanceUi::begin() {
    auto* screen=lv_screen_active();
    performance_=lv_obj_create(screen); browser_=lv_obj_create(screen);
    if(!performance_ || !browser_) { ESP_LOGE("PERFORMANCE","Screen allocation failed"); return false; }
    panel(performance_); panel(browser_);
    lv_obj_set_size(performance_,800,480); lv_obj_set_pos(performance_,0,0);
    lv_obj_set_size(browser_,800,480); lv_obj_set_pos(browser_,0,0);
    header(performance_,"PERFORMANCE"); header(browser_,"FACTORY PATCHES");
    midi_=label(performance_,"MIDI OFF",320,14,88,&lv_font_montserrat_14);
    audio_=label(performance_,"AUDIO --",416,14,100,&lv_font_montserrat_14);
    connection_=label(performance_,"LINK OFF",530,14,124,&lv_font_montserrat_14);
    patch_=label(performance_,backend_.local()?"INITIALIZING AMY":"WAITING FOR S3",18,57,464,&lv_font_montserrat_28);
    previous_=button(performance_,"<",496,50,66,44,Action::Previous);
    next_=button(performance_,">",570,50,66,44,Action::Next);
    browse_button_=button(performance_,"BROWSE",644,50,140,44,Action::Browse);
    info_=label(performance_,backend_.status(),18,104,650,&lv_font_montserrat_14);
    tempo_=label(performance_,"-- BPM",690,104,94,&lv_font_montserrat_14);
    for(uint8_t i=0;i<8;++i) {
        const int x=18+(i%4)*192,y=126+(i/4)*92;
        auto* card=lv_obj_create(performance_);
        if(!card) { failed_=true; continue; }
        panel(card); lv_obj_set_pos(card,x,y); lv_obj_set_size(card,188,82);
        lv_obj_set_style_bg_color(card,lv_color_hex(kPanel),0); lv_obj_set_style_radius(card,8,0);
        macro_name_[i]=label(card,"--",12,8,116);
        macro_value_[i]=label(card,"--",136,8,42);
        macro_[i]=lv_slider_create(card);
        if(!macro_[i]) { failed_=true; continue; }
        lv_obj_set_pos(macro_[i],16,46); lv_obj_set_size(macro_[i],156,18);
        lv_slider_set_range(macro_[i],0,127); lv_obj_set_ext_click_area(macro_[i],12);
        lv_obj_set_style_bg_color(macro_[i],lv_color_hex(kAccent),LV_PART_INDICATOR);
        auto* callback=context(Action::Macro,i);
        if(callback) lv_obj_add_event_cb(macro_[i],event,LV_EVENT_ALL,callback);
    }
    label(performance_,"MASTER",20,327,112);
    master_=lv_slider_create(performance_);
    if(!master_) failed_=true;
    else {
        lv_obj_set_pos(master_,142,334); lv_obj_set_size(master_,300,20);
        lv_slider_set_range(master_,0,127); lv_obj_set_ext_click_area(master_,12);
        auto* callback=context(Action::Master);
        if(callback) lv_obj_add_event_cb(master_,event,LV_EVENT_ALL,callback);
    }
    master_value_=label(performance_,"--",474,327,90);
    button(performance_,"OCT -",610,318,76,44,Action::OctaveDown);
    button(performance_,"OCT +",702,318,76,44,Action::OctaveUp);
    octave_=label(performance_,"",20,370,754,&lv_font_montserrat_14);
    for(uint8_t i=0;i<8;++i) {
        keys_[i]=button(performance_,"",18+i*92,392,88,76,Action::Key,kWhiteNotes[i]);
        if(!keys_[i]) continue;
        lv_obj_set_style_bg_color(keys_[i],lv_color_hex(0xe0e8ec),0);
        lv_obj_set_style_bg_color(keys_[i],lv_color_hex(kAccent),LV_STATE_PRESSED);
        lv_obj_remove_flag(keys_[i],LV_OBJ_FLAG_PRESS_LOCK);
        white_label_[i]=lv_obj_get_child(keys_[i],0);
        if(white_label_[i]) {
            lv_obj_set_style_text_color(white_label_[i],lv_color_hex(0x17222b),0);
            lv_obj_align(white_label_[i],LV_ALIGN_BOTTOM_MID,0,-4);
        }
    }
    for(uint8_t i=0;i<5;++i) {
        keys_[8+i]=button(performance_,"",18+kBlackBoundary[i]*92-23,392,46,46,Action::Key,kBlackNotes[i]);
        if(!keys_[8+i]) continue;
        lv_obj_set_style_bg_color(keys_[8+i],lv_color_hex(0x17222b),0);
        lv_obj_set_style_bg_color(keys_[8+i],lv_color_hex(kAccent),LV_STATE_PRESSED);
        lv_obj_remove_flag(keys_[8+i],LV_OBJ_FLAG_PRESS_LOCK);
    }
    page_label_=label(browser_,"",20,53,750);
    for(uint8_t i=0;i<8;++i) {
        rows_[i]=button(browser_,"LOADING...",20,82+i*41,760,37,Action::Row,i);
        if(rows_[i]) { row_labels_[i]=lv_obj_get_child(rows_[i],0); enable(rows_[i],false); }
    }
    button(browser_,"PERFORMANCE",20,430,190,40,Action::Back);
    page_previous_=button(browser_,"< PAGE",514,430,124,40,Action::PagePrevious);
    page_next_=button(browser_,"PAGE >",650,430,130,40,Action::PageNext);
    lv_obj_add_flag(browser_,LV_OBJ_FLAG_HIDDEN);
    if(failed_) { ESP_LOGE("PERFORMANCE","Widget allocation failed"); return false; }
    updateOctave(); service(nowMs());
    ESP_LOGI("PERFORMANCE","Initialized; eight backend macros, master, factory browser and mono touch. No startup musical writes.");
    return !failed_;
}
void PerformanceUi::info(const char* value,uint32_t now_ms) { text(info_,info_text_,sizeof(info_text_),value); info_ms_=now_ms; }
void PerformanceUi::select(uint16_t id,uint32_t now_ms) {
    pending_patch_=id; patch_dirty_=true; patch_time_ms_=now_ms; macro_dirty_=0;
    touch_.releaseAll(); info("SELECTING PATCH...",now_ms);
}
void PerformanceUi::browse(bool visible) {
    touch_.releaseAll(); browser_visible_=visible;
    if(visible) {
        lv_obj_add_flag(performance_,LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(browser_,LV_OBJ_FLAG_HIDDEN);
        char value[48]; std::snprintf(value,sizeof(value),"ROM PATCHES    PAGE %u / %u",page_+1,(backend_.patchCount()+7)/8);
        text(page_label_,page_text_,sizeof(page_text_),value);
        for(unsigned i=0;i<8;++i) { rows_loaded_[i]=false; enable(rows_[i],false); text(row_labels_[i],row_text_[i],sizeof(row_text_[i]),"LOADING..."); }
    } else { lv_obj_add_flag(browser_,LV_OBJ_FLAG_HIDDEN); lv_obj_remove_flag(performance_,LV_OBJ_FLAG_HIDDEN); }
}
void PerformanceUi::updateOctave() {
    const unsigned octave=base_note_/12-1;
    char value[96]; std::snprintf(value,sizeof(value),"TOUCH MONO  C%u-C%u  /  VELOCITY %u  /  MAX HOLD 10s",octave,octave+1,config::kTouchVelocity);
    text(octave_,octave_text_,sizeof(octave_text_),value);
    for(unsigned i=0;i<8;++i) {
        std::snprintf(value,sizeof(value),"%s%u",kWhiteNames[i],octave+(i==7?1:0));
        text(white_label_[i],white_text_[i],sizeof(white_text_[i]),value);
    }
}
void PerformanceUi::event(lv_event_t* e) {
    auto* callback=static_cast<Context*>(lv_event_get_user_data(e));
    if(callback) callback->ui->handle(callback->action,callback->index,lv_event_get_code(e),static_cast<lv_obj_t*>(lv_event_get_target(e)));
}
void PerformanceUi::handle(Action action,uint8_t index,lv_event_code_t code,lv_obj_t* target) {
    if(suppress_) return;
    const uint32_t now_ms=nowMs();
    if(action==Action::Key) {
        if(code==LV_EVENT_PRESSED) touch_.press(uint8_t(base_note_+index),now_ms);
        else if(code==LV_EVENT_RELEASED || code==LV_EVENT_PRESS_LOST) touch_.release(uint8_t(base_note_+index));
        return;
    }
    if(action==Action::Macro || action==Action::Master) {
        if(action==Action::Macro) {
            if(code==LV_EVENT_PRESSED) editing_|=uint8_t(1u<<index);
            if(code==LV_EVENT_RELEASED || code==LV_EVENT_PRESS_LOST) editing_&=uint8_t(~(1u<<index));
        } else {
            if(code==LV_EVENT_PRESSED) master_editing_=true;
            if(code==LV_EVENT_RELEASED || code==LV_EVENT_PRESS_LOST) master_editing_=false;
        }
        if(code==LV_EVENT_VALUE_CHANGED) {
            const auto value=static_cast<uint8_t>(lv_slider_get_value(target));
            char display[12];
            if(action==Action::Macro) {
                pending_macros_[index]=value; macro_dirty_|=uint8_t(1u<<index);
                std::snprintf(display,sizeof(display),"%u",value); text(macro_value_[index],macro_values_[index],sizeof(macro_values_[index]),display);
            } else {
                pending_master_=value; master_dirty_=true;
                std::snprintf(display,sizeof(display),"%u%%",unsigned(value)*100/127); text(master_value_,master_text_,sizeof(master_text_),display);
            }
            info("SENDING CONTROL...",now_ms);
        }
        return;
    }
    if(code!=LV_EVENT_CLICKED) return;
    switch(action) {
        case Action::Panic:
            if(backend_.connected()) { touch_.releaseAll(); macro_dirty_=0; patch_dirty_=master_dirty_=false; backend_.panic(); info("PANIC REQUESTED",now_ms); }
            break;
        case Action::Previous:
        case Action::Next: {
            const uint16_t current=patch_dirty_?pending_patch_:backend_.state().patch_id;
            uint16_t index=0;
            for(uint16_t i=0;i<backend_.patchCount();++i) if(backend_.patchAt(i)==current) index=i;
            const uint16_t count=backend_.patchCount();
            if(count) select(backend_.patchAt((index+count+(action==Action::Next?1:-1))%count),now_ms);
            break;
        }
        case Action::Browse: page_=0; for(uint16_t i=0;i<backend_.patchCount();++i) if(backend_.patchAt(i)==backend_.state().patch_id) page_=i/8; browse(true); break;
        case Action::Back: browse(false); break;
        case Action::PagePrevious: if(page_) { --page_; browse(true); } break;
        case Action::PageNext: if((page_+1)*8<backend_.patchCount()) { ++page_; browse(true); } break;
        case Action::Row: if(rows_loaded_[index]) { select(backend_.patchAt(page_*8+index),now_ms); browse(false); } break;
        case Action::OctaveDown: touch_.releaseAll(); if(base_note_>config::kTouchMinBaseNote) base_note_-=12; updateOctave(); break;
        case Action::OctaveUp: touch_.releaseAll(); if(base_note_<config::kTouchMaxBaseNote) base_note_+=12; updateOctave(); break;
        default: break;
    }
}
void PerformanceUi::updateState(uint32_t now_ms) {
    const auto& s=backend_.state();
    if(shown_patch_!=s.patch_id) { shown_patch_=s.patch_id; macro_dirty_=0; touch_.releaseAll(); }
    char value[96]; std::snprintf(value,sizeof(value),"%03u  %s",s.patch_id,s.patch_name);
    text(patch_,patch_text_,sizeof(patch_text_),value);
    if(backend_.local()) std::snprintf(value,sizeof(value),"4 VOICES");
    else std::snprintf(value,sizeof(value),"%.1f BPM",static_cast<double>(s.bpm_centi)/100.0);
    text(tempo_,tempo_text_,sizeof(tempo_text_),value);
    if(midi_count_!=s.midi_events) { midi_count_=s.midi_events; midi_changed_ms_=now_ms; }
    if(misses_!=s.deadline_misses || gaps_!=s.pcm_starvations) { misses_=s.deadline_misses; gaps_=s.pcm_starvations; warning_ms_=now_ms; }
    suppress_=true;
    for(unsigned i=0;i<8;++i) {
        text(macro_name_[i],macro_names_[i],sizeof(macro_names_[i]),s.macros[i].name[0]?s.macros[i].name:"UNAVAILABLE");
        if(!(editing_&(1u<<i)) && !(macro_dirty_&(1u<<i))) {
            if(macro_[i]) lv_slider_set_value(macro_[i],s.macros[i].value,LV_ANIM_OFF);
            std::snprintf(value,sizeof(value),"%u",s.macros[i].value);
            text(macro_value_[i],macro_values_[i],sizeof(macro_values_[i]),value);
        }
    }
    if(!master_editing_ && !master_dirty_) {
        if(master_) lv_slider_set_value(master_,s.master,LV_ANIM_OFF);
        std::snprintf(value,sizeof(value),"%u%%",unsigned(s.master)*100/127); text(master_value_,master_text_,sizeof(master_text_),value);
    }
    suppress_=false;
}
void PerformanceUi::sendPending(uint32_t now_ms) {
    if(!backend_.connected() || backend_.busy()) return;
    if(patch_dirty_) {
        if(now_ms-patch_time_ms_<config::kPatchDebounceMs) return;
        if(backend_.setPatch(pending_patch_,now_ms)) { patch_dirty_=false; last_send_ms_=now_ms; }
        return;
    }
    if(now_ms-last_send_ms_>=config::kControlSendIntervalMs) {
        for(uint8_t i=0;i<8;++i) if(macro_dirty_&(1u<<i)) {
            if(backend_.setMacro(i,pending_macros_[i],now_ms)) { macro_dirty_&=uint8_t(~(1u<<i)); last_send_ms_=now_ms; }
            return;
        }
        if(master_dirty_ && backend_.setMaster(pending_master_,now_ms)) { master_dirty_=false; last_send_ms_=now_ms; return; }
    }
    if(browser_visible_) for(uint8_t i=0;i<8;++i) if(!rows_loaded_[i]) {
        const uint16_t index=page_*8+i;
        if(index>=backend_.patchCount()) {
            text(row_labels_[i],row_text_[i],sizeof(row_text_[i]),"UNAVAILABLE"); continue;
        }
        PatchInfo patch;
        if(backend_.patchInfo(index,patch,now_ms)) {
            char value[56]; std::snprintf(value,sizeof(value),"%03u    %s%s",patch.id,patch.name,patch.available?"":" (UNAVAILABLE)");
            text(row_labels_[i],row_text_[i],sizeof(row_text_[i]),value); rows_loaded_[i]=patch.available;
        }
        return;
    }
}

void PerformanceUi::service(uint32_t now_ms) {
    backend_.service(now_ms);
    if(const char* notice=backend_.takeNotice()) info(notice,now_ms);
    const bool online=backend_.connected();
    const auto& s=backend_.state();
    const bool playable=online && ((s.flags&link::kAudioLive) || backend_.local());
    if(!online) {
        if(was_online_) {
            macro_dirty_=0; patch_dirty_=master_dirty_=false; editing_=0; master_editing_=false; touch_.disconnect();
            for(auto& loaded:rows_loaded_) loaded=false; // Fetch the new backend's catalog after reconnect.
        }
        info(backend_.status(),now_ms);
    }
    was_online_=online;
    if(online && shown_revision_!=backend_.stateRevision()) { shown_revision_=backend_.stateRevision(); updateState(now_ms); }
    enable(previous_,online); enable(next_,online); enable(browse_button_,online); enable(master_,online);
    for(unsigned i=0;i<2;++i) enable(panic_[i],backend_.connected());
    for(unsigned i=0;i<8;++i) { enable(macro_[i],online && s.macros[i].supported); enable(rows_[i],online && rows_loaded_[i]); }
    for(auto* key:keys_) enable(key,playable);
    enable(page_previous_,online && page_>0); enable(page_next_,online && (page_+1)*8<backend_.patchCount());
    const bool activity=online && (s.flags&link::kUsbConnected) && uint32_t(now_ms-midi_changed_ms_)<150;
    const char* midi=backend_.midiError()?"MIDI ERROR":online && (s.flags&link::kUsbConnected)?(activity?"MIDI *":"MIDI ON"):"MIDI OFF";
    const bool warning=online && (misses_ || gaps_) && uint32_t(now_ms-warning_ms_)<2000;
    const char* audio=!online?"AUDIO --":backend_.local() && !(s.flags&link::kAudioLive)?"AUDIO RAM":!playable?"AUDIO STOP":warning?"AUDIO WARN":"AUDIO RUN";
    // Only change labels/styles when the visual state actually changes.
    if(midi_ && std::strcmp(lv_label_get_text(midi_),midi)!=0) {
        lv_label_set_text_static(midi_,midi);
        lv_obj_set_style_text_color(midi_,lv_color_hex(activity?0x6fd49b:kMuted),0);
    }
    if(audio_ && std::strcmp(lv_label_get_text(audio_),audio)!=0) lv_label_set_text_static(audio_,audio);
    const char* connection=backend_.local()?(online?"AMY LOCAL":"AMY ERROR"):(online?"LINK ON":"LINK OFF");
    if(connection_ && std::strcmp(lv_label_get_text(connection_),connection)!=0) lv_label_set_text_static(connection_,connection);
    touch_.service(backend_,now_ms,playable);
    sendPending(now_ms);
    if(online && uint32_t(now_ms-info_ms_)>=1800) {
        char value[96];
        if(backend_.local()) std::snprintf(value,sizeof(value),"%s | NOTES %lu | DSP %lu us | DMA UNMEASURED",
            backend_.status(),static_cast<unsigned long>(s.active_notes),static_cast<unsigned long>(s.max_render_us));
        else std::snprintf(value,sizeof(value),"S3 CONTROL | NOTES %lu | RENDER %lu us | PCM GAPS %lu",
            static_cast<unsigned long>(s.active_notes),static_cast<unsigned long>(s.max_render_us),static_cast<unsigned long>(s.pcm_starvations));
        text(info_,info_text_,sizeof(info_text_),value);
    }
}
}
