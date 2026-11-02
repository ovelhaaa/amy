#pragma once
#include "performance_backend.h"
#include "touch_note.h"
#include "lvgl.h"

namespace smk::p4 {
class PerformanceUi {
public:
    explicit PerformanceUi(PerformanceBackend& backend) : backend_(backend) {}
    bool begin();
    void service(uint32_t now_ms);
private:
    enum class Action : uint8_t { Previous,Next,Browse,Back,PagePrevious,PageNext,OctaveDown,OctaveUp,Panic,Row,Macro,Master,Key };
    struct Context { PerformanceUi* ui=nullptr; Action action=Action::Panic; uint8_t index=0; };
    static void event(lv_event_t* event);
    void handle(Action action,uint8_t index,lv_event_code_t code,lv_obj_t* target);
    Context* context(Action action,uint8_t index=0);
    lv_obj_t* label(lv_obj_t* parent,const char* text,int x,int y,int width,const lv_font_t* font=&lv_font_montserrat_20);
    lv_obj_t* button(lv_obj_t* parent,const char* text,int x,int y,int width,int height,Action action,uint8_t index=0);
    void header(lv_obj_t* parent,const char* title);
    void browse(bool visible);
    void select(uint16_t id,uint32_t now_ms);
    void updateState(uint32_t now_ms);
    void updateOctave();
    void info(const char* text,uint32_t now_ms);
    void sendPending(uint32_t now_ms);
    static void enable(lv_obj_t* object,bool enabled);
    static void text(lv_obj_t* object,char* buffer,size_t capacity,const char* value);

    PerformanceBackend& backend_;
    TouchNote touch_;
    lv_obj_t *performance_=nullptr,*browser_=nullptr,*patch_=nullptr,*info_=nullptr,*tempo_=nullptr;
    lv_obj_t *midi_=nullptr,*audio_=nullptr,*connection_=nullptr,*master_=nullptr,*master_value_=nullptr,*octave_=nullptr;
    lv_obj_t *previous_=nullptr,*next_=nullptr,*browse_button_=nullptr,*page_previous_=nullptr,*page_next_=nullptr,*page_label_=nullptr;
    lv_obj_t *panic_[2]{},*macro_[8]{},*macro_name_[8]{},*macro_value_[8]{},*rows_[8]{},*row_labels_[8]{},*white_label_[8]{};
    lv_obj_t* keys_[13]{};
    Context contexts_[64]{};
    unsigned contexts_used_=0;
    uint8_t pending_macros_[8]{},macro_dirty_=0,editing_=0,pending_master_=0,base_note_=60,page_=0;
    uint16_t pending_patch_=0,shown_patch_=UINT16_MAX;
    bool patch_dirty_=false,master_dirty_=false,master_editing_=false,browser_visible_=false,rows_loaded_[8]{};
    bool failed_=false,suppress_=false,was_online_=false;
    uint32_t patch_time_ms_=0,last_send_ms_=0,shown_revision_=UINT32_MAX,info_ms_=0;
    uint32_t midi_count_=0,midi_changed_ms_=0,warning_ms_=0,misses_=0,gaps_=0;
    char patch_text_[64]{},info_text_[96]{},tempo_text_[20]{},master_text_[12]{},octave_text_[96]{},page_text_[48]{};
    char macro_names_[8][16]{},macro_values_[8][8]{},row_text_[8][56]{},white_text_[8][8]{};
};
}
