#include "p4my_splash.h"
#include "ui_theme.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace smk::p4::ui {
namespace {
constexpr uint32_t kBg=0x080F17, kGrid=0x15222B, kSecondary=0x648496;
constexpr uint32_t kAmber=0xEAB65B, kBright=0xFFBD5A, kText=0xE6EDF2;
constexpr float kTwoPi=6.2831853f;
constexpr float kHalfPiScale=2.0f/3.14159265f;

float ramp(float t, float a, float b) {
    float x=(t-a)/(b-a);
    if(x<0) x=0;
    if(x>1) x=1;
    return x*x*(3-2*x);
}
void setOpaIfChanged(lv_obj_t* obj, float value) {
    if(!obj) return;
    const lv_opa_t next=static_cast<lv_opa_t>(value*255.0f);
    if(lv_obj_get_style_opa(obj,LV_PART_MAIN)!=next) {
        lv_obj_set_style_opa(obj,next,0);
    }
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
} // namespace

bool P4mySplash::begin(lv_obj_t* parent,uint32_t now_ms,Config config) {
    close();
    if(!parent || config.duration_ms==0 || (config.fps!=20 && config.fps!=30) || config.exit_ms>1000) return false;
#if LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN
    // Refuse the optional overlay before LVGL assertions on a depleted pool.
    lv_mem_monitor_t memory{};
    lv_mem_monitor(&memory);
    if(memory.free_size<20u*1024u || memory.free_biggest_size<2048u) return false;
#endif
    config_=config;
    metrics_=Metrics{};
    start_ms_=last_ms_=current_now_ms_=now_ms;
    hold_start_ms_=wait_start_ms_=exit_start_ms_=now_ms;
    engine_ready_=false;
    engine_error_=false;
    final_rendered_=false;
    state_=State::Animating;

    root_=group(parent);
    if(!root_) { state_=State::Inactive; return false; }
    lv_obj_add_event_cb(root_,deleted,LV_EVENT_DELETE,this);
    lv_obj_add_event_cb(root_,touched,LV_EVENT_PRESSED,this);
    lv_obj_set_style_bg_color(root_,lv_color_hex(kBg),0);
    lv_obj_set_style_bg_opa(root_,LV_OPA_COVER,0);
    // Modal while visible; stop touch bubbling so underlying Performance controls are never hit.
    lv_obj_add_flag(root_,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(root_,LV_OBJ_FLAG_EVENT_BUBBLE);

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
    const uint32_t clamped=std::min(elapsed_ms,config_.duration_ms);
    const bool is_final=(clamped>=config_.duration_ms);
    if(is_final && final_rendered_) return;

    const auto t0=std::chrono::steady_clock::now();
    const float t=static_cast<float>(clamped)*2700.0f/static_cast<float>(config_.duration_ms);
    setOpaIfChanged(technical_,ramp(t,0,400));

    // Smoothly slowing phase, aligned to the operator reference at 2700 ms.
    const float phase=1.8f*(std::exp(-t/1000.0f)-std::exp(-2.7f));
    for(unsigned w=0;w<3;++w) {
        for(unsigned i=0;i<kPoints;++i) {
            const float x=static_cast<float>(i)*kScreenWidth/static_cast<float>(kPoints-1);
            float y;
            if(w==0) y=240.0f-55.0f*std::cos((x-40.0f)*kTwoPi/430.0f+phase);
            else if(w==1) y=240.0f+86.0f*kHalfPiScale*std::asin(std::cos((x-8.0f)*kTwoPi/286.0f+phase));
            else y=240.0f+52.0f*std::sin(x*kTwoPi/640.0f-0.82f+phase);
            points_[w][i]={lv_value_precise_t(x),lv_value_precise_t(y)};
        }
    }
    ++metrics_.wave_updates;

    for(unsigned i=0;i<5;++i) {
        lv_obj_invalidate(waves_[i]);
        setOpaIfChanged(waves_[i],i<2?ramp(t,200,900):ramp(t,400,1200));
    }
    setOpaIfChanged(logo_,ramp(t,900,1800));
    setOpaIfChanged(subtitle_,ramp(t,1800,2300));
    setOpaIfChanged(credit_,ramp(t,2300,2700));
    final_rendered_=is_final;

    const auto us=static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-t0).count());
    ++metrics_.frames_rendered;
    metrics_.render_total_us+=us;
    if(us>metrics_.render_max_us) metrics_.render_max_us=us;
}

void P4mySplash::seek(uint32_t elapsed_ms) {
    if(elapsed_ms<config_.duration_ms) final_rendered_=false;
    render(elapsed_ms);
}

void P4mySplash::setEngineReady(bool ready, uint32_t now_ms) {
    if(!root_ || engine_ready_ || !ready) return;
    engine_ready_=true;
    metrics_.engine_ready_recorded=true;
    metrics_.engine_ready_ms=now_ms-start_ms_;
}

void P4mySplash::setEngineError(bool error, uint32_t now_ms) {
    if(!root_ || !error || engine_error_) return;
    engine_error_=true;
    beginExit(DismissReason::EngineError,now_ms);
}

void P4mySplash::requestDismiss(DismissReason reason, uint32_t now_ms) {
    if(!root_) return;
    beginExit(reason,now_ms);
}

void P4mySplash::finish(uint32_t now_ms) {
    requestDismiss(DismissReason::ExplicitCommand,now_ms);
}

void P4mySplash::beginExit(DismissReason reason, uint32_t now_ms) {
    if(!root_ || state_==State::Exiting || state_==State::Closed) return;
    metrics_.dismiss_reason=reason;
    exit_start_ms_=now_ms;
    last_ms_=now_ms;
    if(config_.exit_ms==0) {
        metrics_.effective_duration_ms=now_ms-start_ms_;
        close();
        return;
    }
    state_=State::Exiting;
}

void P4mySplash::service(uint32_t now_ms) {
    if(!root_) return;
    current_now_ms_=now_ms;

    if(state_==State::Exiting) {
        if(config_.exit_ms==0 || (now_ms-exit_start_ms_)>=config_.exit_ms) {
            metrics_.effective_duration_ms=now_ms-start_ms_;
            close();
            return;
        }
        const uint32_t period_ms=1000u/config_.fps;
        if((now_ms-last_ms_)<period_ms) return;
        last_ms_=now_ms-((now_ms-last_ms_)%period_ms);
        setOpaIfChanged(root_,1.0f-ramp(static_cast<float>(now_ms-exit_start_ms_),0.0f,static_cast<float>(config_.exit_ms)));
        return;
    }

    if(state_==State::Animating) {
        const uint32_t elapsed=now_ms-start_ms_;
        if(elapsed>=config_.duration_ms) {
            render(config_.duration_ms);
            hold_start_ms_=now_ms;
            state_=State::FinalHold;
            if(config_.final_hold_ms==0) {
                if(engine_ready_) beginExit(DismissReason::AnimationComplete,now_ms);
                else { wait_start_ms_=now_ms; state_=State::WaitingForEngine; }
            }
            return;
        }
        const uint32_t period_ms=1000u/config_.fps;
        if((now_ms-last_ms_)<period_ms) return;
        last_ms_=now_ms-((now_ms-last_ms_)%period_ms);
        render(elapsed);
        return;
    }

    if(state_==State::FinalHold) {
        if((now_ms-hold_start_ms_)>=config_.final_hold_ms) {
            if(engine_ready_) {
                beginExit(DismissReason::AnimationComplete,now_ms);
            } else {
                wait_start_ms_=now_ms;
                state_=State::WaitingForEngine;
            }
        }
        return;
    }

    if(state_==State::WaitingForEngine) {
        if(engine_ready_ || (config_.wait_timeout_ms>0 && (now_ms-wait_start_ms_)>=config_.wait_timeout_ms)) {
            beginExit(DismissReason::AnimationComplete,now_ms);
        }
    }
}

void P4mySplash::deleted(lv_event_t* event) {
    auto* self=static_cast<P4mySplash*>(lv_event_get_user_data(event));
    if(!self) return;
    self->root_=self->technical_=self->logo_=self->subtitle_=self->credit_=nullptr;
    for(auto& wave:self->waves_) wave=nullptr;
    if(self->metrics_.dismiss_reason==DismissReason::None) {
        self->metrics_.dismiss_reason=DismissReason::ParentDeleted;
    }
    self->state_=State::Closed;
}

void P4mySplash::touched(lv_event_t* event) {
    lv_event_stop_bubbling(event);
    auto* self=static_cast<P4mySplash*>(lv_event_get_user_data(event));
    if(!self) return;
    self->requestDismiss(DismissReason::Touch,self->current_now_ms_);
}

void P4mySplash::close() {
    if(root_) {
        lv_obj_delete(root_);
    }
    root_=technical_=logo_=subtitle_=credit_=nullptr;
    for(auto& wave:waves_) wave=nullptr;
    if(state_!=State::Inactive) state_=State::Closed;
}
} // namespace smk::p4::ui
