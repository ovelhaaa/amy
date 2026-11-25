#include "p4my_splash.h"
#include "ui_theme.h"
#include <cmath>

namespace smk::p4::ui {
namespace {
constexpr uint32_t kBg=0x080F17, kGrid=0x15222B, kSecondary=0x648496;
constexpr uint32_t kAmber=0xEAB65B, kBright=0xFFBD5A, kText=0xE6EDF2;
float ramp(float t, float a, float b) {
    float x=(t-a)/(b-a);
    if(x<0) x=0;
    if(x>1) x=1;
    return x*x*(3-2*x);
}
void opacity(lv_obj_t* obj, float value) {
    lv_obj_set_style_opa(obj, static_cast<lv_opa_t>(value*255), 0);
}
lv_obj_t* group(lv_obj_t* parent) {
    auto* obj=lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj,kScreenWidth,kScreenHeight);
    lv_obj_remove_flag(obj,LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}
lv_obj_t* line(lv_obj_t* parent, const lv_point_precise_t* points, unsigned count,
               uint32_t color, unsigned width, unsigned alpha=255) {
    auto* obj=lv_line_create(parent);
    lv_line_set_points(obj,points,count);
    lv_obj_set_style_line_color(obj,lv_color_hex(color),0);
    lv_obj_set_style_line_width(obj,width,0);
    lv_obj_set_style_line_opa(obj,alpha,0);
    lv_obj_set_style_line_rounded(obj,true,0);
    return obj;
}
lv_obj_t* text(lv_obj_t* parent,const char* value,int y,uint32_t color) {
    auto* obj=lv_label_create(parent);
    lv_label_set_text(obj,value);
    lv_obj_set_style_text_font(obj,&lv_font_montserrat_14,0);
    lv_obj_set_style_text_color(obj,lv_color_hex(color),0);
    lv_obj_set_style_text_letter_space(obj,3,0);
    lv_obj_align(obj,LV_ALIGN_TOP_MID,0,y);
    return obj;
}
// Original geometric monoline lettering; static points include a gentle lean.
const lv_point_precise_t p[]={{260,240},{274,148},{309,148},{325,157},{322,180},{308,190},{269,190}};
const lv_point_precise_t four[]={{384,240},{398,148},{344,211},{412,211}};
const lv_point_precise_t m[]={{423,240},{433,181},{448,181},{458,192},{471,181},{487,181},{498,193},{490,240}};
const lv_point_precise_t mStem[]={{458,192},{451,240}};
const lv_point_precise_t y[]={{518,181},{530,225},{552,181},{518,266},{503,270}};
const lv_point_precise_t rail[]={{38,386},{190,386}};
const lv_point_precise_t rail2[]={{610,96},{762,96}};
}
bool P4mySplash::begin(lv_obj_t* parent,uint32_t now_ms,Config config) {
    close();
    if(!parent || config.duration_ms==0 || (config.fps!=20 && config.fps!=30) || config.exit_ms>1000) return false;
#if LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN
    // Refuse the optional overlay before LVGL assertions on a depleted pool.
    // Host footprint is ~12 KB; reserve initialization/fragmentation headroom.
    lv_mem_monitor_t memory{};
    lv_mem_monitor(&memory);
    if(memory.free_size<20u*1024u || memory.free_biggest_size<2048u) return false;
#endif
    config_=config;
    start_ms_=last_ms_=now_ms;
    exiting_=false;
    root_=group(parent);
    if(!root_) return false;
    lv_obj_add_event_cb(root_,deleted,LV_EVENT_DELETE,this);
    lv_obj_set_style_bg_color(root_,lv_color_hex(kBg),0);
    lv_obj_set_style_bg_opa(root_,LV_OPA_COVER,0);
    // Modal while visible; the existing Performance tree remains underneath.
    lv_obj_add_flag(root_,LV_OBJ_FLAG_CLICKABLE);
    technical_=group(root_);
    for(int x=40;x<kScreenWidth;x+=40) {
        auto* obj=lv_obj_create(technical_); lv_obj_remove_style_all(obj);
        lv_obj_set_pos(obj,x,0); lv_obj_set_size(obj,1,kScreenHeight);
        lv_obj_set_style_bg_color(obj,lv_color_hex(kGrid),0); lv_obj_set_style_bg_opa(obj,75,0);
    }
    for(int y0=40;y0<kScreenHeight;y0+=40) {
        auto* obj=lv_obj_create(technical_); lv_obj_remove_style_all(obj);
        lv_obj_set_pos(obj,0,y0); lv_obj_set_size(obj,kScreenWidth,1);
        lv_obj_set_style_bg_color(obj,lv_color_hex(kGrid),0); lv_obj_set_style_bg_opa(obj,75,0);
    }
    for(int i=0;i<4;++i) {
        const int size=i%2 ? 38 : 64;
        auto* obj=lv_obj_create(technical_); lv_obj_remove_style_all(obj);
        lv_obj_set_size(obj,size,size); lv_obj_set_pos(obj,i<2?72-size/2:728-size/2,240-size/2);
        lv_obj_set_style_radius(obj,LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_border_width(obj,1,0); lv_obj_set_style_border_color(obj,lv_color_hex(kSecondary),0);
        lv_obj_set_style_border_opa(obj,80,0);
    }
    line(technical_,rail,2,kSecondary,1,90); line(technical_,rail2,2,kSecondary,1,90);
    text(technical_,"P4 / DIGITAL INSTRUMENT",44,0x80919F);
    for(unsigned i=0;i<5;++i) {
        waves_[i]=line(root_,points_[i<2?i:2],kPoints,i<2?kSecondary:(i==4?kBright:kAmber),i<2?1:(i==2?13:(i==3?6:2)),i==2?12:(i==3?35:190));
        lv_line_set_points_mutable(waves_[i],points_[i<2?i:2],kPoints);
        lv_obj_set_size(waves_[i],kScreenWidth,kScreenHeight);
    }
    logo_=group(root_);
    line(logo_,p,7,kText,9); line(logo_,four,4,kAmber,9);
    line(logo_,m,8,kText,9); line(logo_,mStem,2,kText,9); line(logo_,y,5,kText,9);
    subtitle_=text(root_,"DIGITAL PERFORMANCE SYNTH",327,kText);
    credit_=text(root_,"powered by AMY",411,0x80919F);
    lv_obj_set_style_text_letter_space(credit_,1,0);
    render(0);
    return true;
}
void P4mySplash::render(uint32_t elapsed_ms) {
    if(!root_) return;
    const float t=static_cast<float>(elapsed_ms)*2700.0f/config_.duration_ms;
    opacity(technical_,ramp(t,0,400));
    for(unsigned w=0;w<3;++w) {
        // Integrated exponentially slowing phase: no discontinuity at final state.
        const float phase=1.8f*(1-std::exp(-t/1000));
        for(unsigned i=0;i<kPoints;++i) {
            const float x=static_cast<float>(i)/static_cast<float>(kPoints-1);
            const float a=x*16.0f+phase+static_cast<float>(w)*1.7f;
            const float signal=w==1 ? (2.0f/3.14159265f)*std::asin(std::sin(a)) : std::sin(a);
            const float envelope=std::sin(x*3.14159265f);
            points_[w][i].x=24+x*752;
            points_[w][i].y=260+signal*envelope*(w==2?66:92);
        }
    }
    for(unsigned i=0;i<5;++i) {
        lv_obj_invalidate(waves_[i]);
        opacity(waves_[i],i<2?ramp(t,200,900):ramp(t,400,1200));
    }
    opacity(logo_,ramp(t,900,1800));
    opacity(subtitle_,ramp(t,1800,2300));
    opacity(credit_,ramp(t,2300,2700));
}
void P4mySplash::seek(uint32_t elapsed_ms) { render(elapsed_ms); }
void P4mySplash::finish(uint32_t now_ms) {
    if(!root_ || exiting_) return;
    exiting_=true; exit_start_ms_=now_ms;
}
void P4mySplash::service(uint32_t now_ms) {
    if(!root_) return;
    if(exiting_ && (config_.exit_ms==0 || now_ms-exit_start_ms_>=config_.exit_ms)) { close(); return; }
    const uint32_t period_ms=1000u/config_.fps;
    if(now_ms-last_ms_<period_ms) return;
    last_ms_=now_ms-((now_ms-last_ms_)%period_ms);
    render(now_ms-start_ms_);
    if(exiting_) opacity(root_,1-ramp(static_cast<float>(now_ms-exit_start_ms_),0,static_cast<float>(config_.exit_ms)));
}
void P4mySplash::deleted(lv_event_t* event) {
    auto* self=static_cast<P4mySplash*>(lv_event_get_user_data(event));
    self->root_=nullptr; // Parent deletion also cancels the lifecycle safely.
}
void P4mySplash::close() {
    if(root_) lv_obj_delete(root_);
    root_=technical_=logo_=subtitle_=credit_=nullptr;
    for(auto& wave:waves_) wave=nullptr;
    exiting_=false;
}
} // namespace smk::p4::ui
