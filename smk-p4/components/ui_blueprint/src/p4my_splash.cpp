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
    lv_obj_set_style_line_rounded(obj,false,0);
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
// A8 artwork is flash-resident; LVGL blends its alpha directly at native size.
constexpr lv_image_dsc_t maskDescriptor(const uint8_t* data,uint16_t width,uint16_t height,uint16_t stride) {
    lv_image_dsc_t image{};
    image.header.magic=LV_IMAGE_HEADER_MAGIC;
    image.header.cf=LV_COLOR_FORMAT_A8;
    image.header.w=width; image.header.h=height; image.header.stride=stride;
    image.data_size=uint32_t(stride)*height; image.data=data;
    return image;
}
#include "p4my_wordmark.inc"
lv_obj_t* maskImage(lv_obj_t* parent,const lv_image_dsc_t& source,int x,int y,uint32_t color) {
    auto* obj=lv_image_create(parent);
    lv_image_set_src(obj,&source);
    lv_obj_set_pos(obj,x,y);
    lv_obj_set_style_image_recolor(obj,lv_color_hex(color),0);
    lv_obj_set_style_image_recolor_opa(obj,LV_OPA_COVER,0);
    return obj;
}
struct Mark { int16_t x1,y1,x2,y2; uint32_t color; uint8_t opacity; };
constexpr Mark kMarks[]={
    {283,101,278,94,kSecondary,105},
    {293,88,291,84,kSecondary,105},
    {309,82,305,75,kAmber,165},
    {321,71,320,68,kSecondary,105},
    {338,69,335,61,kSecondary,105},
    {352,60,351,56,kSecondary,105},
    {368,61,367,53,kSecondary,105},
    {384,55,383,51,kSecondary,105},
    {400,58,400,50,kAmber,165},
    {416,55,417,51,kSecondary,105},
    {432,61,433,53,kSecondary,105},
    {448,60,449,56,kSecondary,105},
    {462,69,465,61,kSecondary,105},
    {479,71,480,68,kSecondary,105},
    {491,82,495,75,kAmber,165},
    {507,88,509,84,kSecondary,105},
    {517,101,522,94,kSecondary,105},
    {517,379,522,386,kSecondary,105},
    {507,392,509,396,kSecondary,105},
    {491,398,495,405,kAmber,165},
    {479,409,480,412,kSecondary,105},
    {462,411,465,419,kSecondary,105},
    {448,420,449,424,kSecondary,105},
    {432,419,433,427,kSecondary,105},
    {416,425,417,429,kSecondary,105},
    {400,422,400,430,kAmber,165},
    {384,425,383,429,kSecondary,105},
    {368,419,367,427,kSecondary,105},
    {352,420,351,424,kSecondary,105},
    {338,411,335,419,kSecondary,105},
    {321,409,320,412,kSecondary,105},
    {309,398,305,405,kAmber,165},
    {293,392,291,396,kSecondary,105},
    {283,379,278,386,kSecondary,105},
    {70,50,70,65,kSecondary,140},
    {77,50,77,60,kSecondary,140},
    {84,50,84,65,kAmber,145},
    {91,50,91,60,kSecondary,140},
    {98,50,98,65,kSecondary,140},
    {70,420,70,435,kSecondary,140},
    {77,420,77,430,kSecondary,140},
    {84,420,84,435,kAmber,145},
    {91,420,91,430,kSecondary,140},
    {98,420,98,435,kSecondary,140},
    {720,50,720,65,kSecondary,140},
    {727,50,727,60,kSecondary,140},
    {734,50,734,65,kAmber,145},
    {741,50,741,60,kSecondary,140},
    {748,50,748,65,kSecondary,140},
    {720,420,720,435,kSecondary,140},
    {727,420,727,430,kSecondary,140},
    {734,420,734,435,kAmber,145},
    {741,420,741,430,kSecondary,140},
    {748,420,748,435,kSecondary,140},
    {95,290,130,290,kBright,245},
    {670,290,705,290,kBright,245},
};
void drawTechnical(lv_event_t* event) {
    auto* obj=lv_event_get_current_target_obj(event);
    auto* layer=lv_event_get_layer(event);
    lv_area_t area; lv_obj_get_coords(obj,&area);
    lv_draw_line_dsc_t dsc; lv_draw_line_dsc_init(&dsc);
    lv_obj_init_draw_line_dsc(obj,LV_PART_MAIN,&dsc);
    const lv_opa_t parent_opacity=dsc.opa;
    auto segment=[&](int x1,int y1,int x2,int y2,uint32_t color,uint8_t alpha) {
        dsc.p1={lv_value_precise_t(area.x1+x1),lv_value_precise_t(area.y1+y1)};
        dsc.p2={lv_value_precise_t(area.x1+x2),lv_value_precise_t(area.y1+y2)};
        dsc.color=lv_color_hex(color); dsc.opa=LV_OPA_MIX2(parent_opacity,alpha);
        lv_draw_line(layer,&dsc);
    };
    for(int x=0;x<kScreenWidth;x+=40) segment(x,0,x,kScreenHeight,kGrid,170);
    for(int y=0;y<kScreenHeight;y+=40) segment(0,y,kScreenWidth,y,kGrid,170);
    for(const auto& mark:kMarks) segment(mark.x1,mark.y1,mark.x2,mark.y2,mark.color,mark.opacity);
}
}
bool P4mySplash::begin(lv_obj_t* parent,uint32_t now_ms,Config config) {
    close();
    if(!parent || config.duration_ms==0 || (config.fps!=20 && config.fps!=30) || config.exit_ms>1000) return false;
#if LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN
    // Refuse the optional overlay before LVGL assertions on a depleted pool.
    // Keep conservative initialization/fragmentation headroom for this optional UI.
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
    lv_obj_set_style_line_width(technical_,1,0);
    lv_obj_add_event_cb(technical_,drawTechnical,LV_EVENT_DRAW_MAIN,nullptr);
    for(unsigned i=0;i<5;++i) {
        waves_[i]=line(root_,points_[i<2?i:2],kPoints,i<2?kSecondary:(i==4?kAmber:kBright),i<2?1:(i==2?13:(i==3?6:1)),i<2?100:(i==2?24:(i==3?48:255)));
        lv_line_set_points_mutable(waves_[i],points_[i<2?i:2],kPoints);
        lv_obj_set_size(waves_[i],kScreenWidth,kScreenHeight);
    }
    logo_=group(root_);
    maskImage(logo_,kGlowImage,kGlowX,kGlowY,kAmber);
    maskImage(logo_,kPImage,kPX,kPY,kText);
    maskImage(logo_,kFourImage,kFourX,kFourY,kBright);
    maskImage(logo_,kMyImage,kMyX,kMyY,kText);
    subtitle_=text(root_,"DIGITAL PERFORMANCE SYNTH",282,kText);
    lv_obj_set_style_text_letter_space(subtitle_,8,0);
    credit_=maskImage(root_,kCreditImage,kCreditX,kCreditY,0x80919F);
    render(0);
    return true;
}
void P4mySplash::render(uint32_t elapsed_ms) {
    if(!root_) return;
    const float t=static_cast<float>(elapsed_ms)*2700.0f/config_.duration_ms;
    opacity(technical_,ramp(t,0,400));
    for(unsigned w=0;w<3;++w) {
        // Smoothly slowing phase, aligned to the operator reference at 2700 ms.
        const float phase=1.8f*(std::exp(-t/1000)-std::exp(-2.7f));
        for(unsigned i=0;i<kPoints;++i) {
            const float x=static_cast<float>(i)*kScreenWidth/static_cast<float>(kPoints-1);
            float y;
            if(w==0) y=240-55*std::cos((x-40)*6.2831853f/430+phase);
            else if(w==1) y=240+86*(2.0f/3.14159265f)*std::asin(std::cos((x-8)*6.2831853f/286+phase));
            else y=240+52*std::sin(x*6.2831853f/640-0.82f+phase);
            points_[w][i]={lv_value_precise_t(x),lv_value_precise_t(y)};
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
