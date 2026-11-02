#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cassert>
struct lv_font_t { int height=20; };
inline lv_font_t lv_font_montserrat_14{14},lv_font_montserrat_20{20},lv_font_montserrat_28{28};
using lv_event_code_t=int;
constexpr int LV_EVENT_ALL=0,LV_EVENT_PRESSED=1,LV_EVENT_RELEASED=2,LV_EVENT_PRESS_LOST=3,LV_EVENT_CLICKED=4,LV_EVENT_VALUE_CHANGED=5;
constexpr int LV_OBJ_FLAG_SCROLLABLE=1,LV_OBJ_FLAG_HIDDEN=2,LV_OBJ_FLAG_PRESS_LOCK=4,LV_STATE_DISABLED=1,LV_STATE_PRESSED=2;
constexpr int LV_LABEL_LONG_MODE_CLIP=0,LV_TEXT_ALIGN_CENTER=0,LV_ALIGN_BOTTOM_MID=0,LV_PART_INDICATOR=0,LV_ANIM_OFF=0;
struct lv_obj_t;
struct lv_event_t { lv_obj_t* target; lv_event_code_t code; void* user; };
struct lv_obj_t { char text[256]{}; int x=0,y=0,value=0,state=0,flags=0; lv_obj_t* children[64]{}; unsigned count=0; void(*callback)(lv_event_t*)=nullptr; void* user=nullptr; };
inline lv_obj_t mock_objects[512]{}; inline unsigned mock_count=1;
inline lv_obj_t* lv_screen_active() { return &mock_objects[0]; }
inline lv_obj_t* lv_obj_create(lv_obj_t* parent) { assert(mock_count<512); auto* p=&mock_objects[mock_count++]; if(parent) { assert(parent->count<64); parent->children[parent->count++]=p; } return p; }
inline lv_obj_t* lv_label_create(lv_obj_t* p) { return lv_obj_create(p); }
inline lv_obj_t* lv_button_create(lv_obj_t* p) { return lv_obj_create(p); }
inline lv_obj_t* lv_slider_create(lv_obj_t* p) { return lv_obj_create(p); }
inline int lv_color_hex(uint32_t c) { return c; }
inline int lv_font_get_line_height(const lv_font_t* f) { return f->height; }
inline void lv_obj_set_pos(lv_obj_t* p,int x,int y) { p->x=x; p->y=y; }
inline void lv_obj_set_size(lv_obj_t*,int,int) {}
inline void lv_obj_add_flag(lv_obj_t* p,int flag) { p->flags|=flag; }
inline void lv_obj_remove_flag(lv_obj_t* p,int flag) { p->flags&=~flag; }
inline void lv_obj_add_state(lv_obj_t* p,int state) { p->state|=state; }
inline void lv_obj_remove_state(lv_obj_t* p,int state) { p->state&=~state; }
inline bool lv_obj_has_state(lv_obj_t* p,int state) { return p->state&state; }
inline void lv_label_set_text(lv_obj_t* p,const char* text) { std::strncpy(p->text,text,255); }
inline void lv_label_set_text_static(lv_obj_t* p,const char* text) { lv_label_set_text(p,text); }
inline const char* lv_label_get_text(lv_obj_t* p) { return p->text; }
inline void lv_label_set_long_mode(lv_obj_t*,int) {}
inline void lv_obj_set_ext_click_area(lv_obj_t*,int) {}
inline void lv_obj_center(lv_obj_t*) {}
inline void lv_obj_align(lv_obj_t*,int,int,int) {}
inline lv_obj_t* lv_obj_get_child(lv_obj_t* p,int index) { return p->children[index]; }
inline void lv_slider_set_range(lv_obj_t*,int,int) {}
inline void lv_slider_set_value(lv_obj_t* p,int v,int) { p->value=v; }
inline int lv_slider_get_value(lv_obj_t* p) { return p->value; }
inline void lv_obj_add_event_cb(lv_obj_t* p,void(*fn)(lv_event_t*),int,void* user) { p->callback=fn; p->user=user; }
inline void* lv_event_get_user_data(lv_event_t* e) { return e->user; }
inline lv_event_code_t lv_event_get_code(lv_event_t* e) { return e->code; }
inline void* lv_event_get_target(lv_event_t* e) { return e->target; }
inline void mock_event(lv_obj_t* p,int code) { assert(p && p->callback); lv_event_t e{p,code,p->user}; p->callback(&e); }
inline void lv_obj_set_style_bg_color(lv_obj_t*,int,int) {}
inline void lv_obj_set_style_border_color(lv_obj_t*,int,int) {}
inline void lv_obj_set_style_border_width(lv_obj_t*,int,int) {}
inline void lv_obj_set_style_pad_all(lv_obj_t*,int,int) {}
inline void lv_obj_set_style_radius(lv_obj_t*,int,int) {}
inline void lv_obj_set_style_shadow_width(lv_obj_t*,int,int) {}
inline void lv_obj_set_style_text_align(lv_obj_t*,int,int) {}
inline void lv_obj_set_style_text_color(lv_obj_t*,int,int) {}
inline void lv_obj_set_style_text_font(lv_obj_t*,const lv_font_t*,int) {}
