#include "ui_screens.h"
#include "ui_widgets.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

namespace smk::p4::ui {

namespace {

constexpr int kGridX=61, kGridY=126, kPitchRows=12, kRowHeight=17;
constexpr int kColumnWidth=45, kGridWidth=seq::kSteps*kColumnWidth;
constexpr tempo::Division kDivisions[]{
    tempo::Division::Quarter,tempo::Division::Eighth,tempo::Division::Sixteenth,
    tempo::Division::ThirtySecond,tempo::Division::EighthTriplet,tempo::Division::SixteenthTriplet
};
constexpr const char* kDivisionNames[]{"1/4","1/8","1/16","1/32","1/8T","1/16T"};
constexpr const char* kPitchNames[]{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

void text(lv_obj_t* label,const char* value) {
    if(std::strcmp(lv_label_get_text(label),value)) lv_label_set_text(label,value);
}

void show(lv_obj_t* obj,bool visible) {
    if(visible) lv_obj_remove_flag(obj,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(obj,LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t* rectangle(lv_obj_t* parent,int x,int y,int w,int h,uint32_t color) {
    auto* obj=lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(obj,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(obj,x,y);
    lv_obj_set_size(obj,w,h);
    lv_obj_set_style_bg_color(obj,lv_color_hex(color),0);
    lv_obj_set_style_bg_opa(obj,LV_OPA_COVER,0);
    return obj;
}

class SequencerScreen final : public ScreenView {
    enum Control : uint8_t {
        Play, Stop, Restart, Division, Quantize, OctaveDown, OctaveUp, Enabled,
        Demo, Clear, SelectFirst
    };
    enum Parameter { Length, Swing, Transpose, Velocity, Gate, ParameterCount };
    struct ButtonContext { SequencerScreen* view; Control control; };
    struct SliderContext { SequencerScreen* view; Parameter param; };

public:
    ScreenId id() const override { return ScreenId::Sequencer; }

    bool build(lv_obj_t* parent,IUiBackend& backend,UiNavigationManager& nav) override {
        backend_=&backend;
        nav_=&nav;
        const int saved=nav.subTab(id());
        if(saved>0 && saved<=11*int(seq::kSteps)) {
            octave_=(saved-1)/int(seq::kSteps);
            selected_=unsigned(saved-1)%seq::kSteps;
        }
        if(!backend.sequencerStatus() && backend.isDemoMode()) {
            createLabel(parent,"SEQUENCER ENGINE UNAVAILABLE",12,16,530,&lv_font_montserrat_14,palette::kTextPrimary);
            return true;
        }

        play_=button(parent,"PLAY",8,4,66,36,Play);
        button(parent,"STOP",78,4,66,36,Stop);
        button(parent,"RESTART",148,4,96,36,Restart);
        transport_=createLabel(parent,"",255,6,231,&lv_font_montserrat_14,palette::kTextPrimary);
        feedback_=createLabel(parent,"",255,25,236,&lv_font_montserrat_14,palette::kWarning);
        button(parent,"LOAD DEMO PATTERN",493,4,170,36,Demo);
        button(parent,"CLEAR PATTERN",669,4,123,36,Clear);
        division_=button(parent,"",8,48,117,38,Division);
        makeSlider(parent,Length,137,45,121,1,16);
        makeSlider(parent,Swing,274,45,121,50,75);
        makeSlider(parent,Transpose,411,45,132,-24,24);
        quantize_=button(parent,"",560,48,232,38,Quantize);

        auto* grid=rectangle(parent,kGridX,kGridY,kGridWidth,kPitchRows*kRowHeight,palette::kBackground);
        lv_obj_add_flag(grid,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(grid,[](lv_event_t* event) {
            auto* self=static_cast<SequencerScreen*>(lv_event_get_user_data(event));
            auto* pointer=lv_indev_active();
            if(!pointer || lv_indev_get_type(pointer)!=LV_INDEV_TYPE_POINTER) return;
            lv_point_t point;
            lv_indev_get_point(pointer,&point);
            lv_area_t bounds;
            lv_obj_get_coords(lv_event_get_current_target_obj(event),&bounds);
            self->chooseCell(point.x-bounds.x1,point.y-bounds.y1);
        },LV_EVENT_CLICKED,this);

        for(int row=0;row<kPitchRows;++row) {
            const int pitch=11-row;
            const bool black=pitch==1 || pitch==3 || pitch==6 || pitch==8 || pitch==10;
            rectangle(grid,0,row*kRowHeight,kGridWidth,kRowHeight-1,
                      pitch==0?palette::kPanelHighlight:black?palette::kKeyBlack:palette::kPanel);
            pitches_[row]=createLabel(parent,"",8,kGridY+row*kRowHeight+1,50,&lv_font_montserrat_14,
                                      pitch==0?palette::kAccentPrimary:palette::kTextSecondary);
        }
        for(unsigned i=0;i<seq::kSteps;++i) {
            const auto x=int(i)*kColumnWidth;
            headers_[i]=button(parent,"",kGridX+x,93,kColumnWidth-1,31,Control(SelectFirst+i));
            if(i>0) rectangle(grid,x-1,0,i%4?1:2,kPitchRows*kRowHeight,palette::kBorder);
            notes_[i]=rectangle(grid,x+4,0,kColumnWidth-9,kRowHeight-4,palette::kActive);
            show(notes_[i],false);
        }
        playhead_=rectangle(grid,0,0,3,kPitchRows*kRowHeight,palette::kAccentPrimary);
        show(playhead_,false);

        button(parent,"OCT -",8,343,68,38,OctaveDown);
        button(parent,"OCT +",81,343,68,38,OctaveUp);
        selection_=createLabel(parent,"",160,354,165,&lv_font_montserrat_14,palette::kTextPrimary);
        makeSlider(parent,Velocity,328,338,137,1,127);
        makeSlider(parent,Gate,486,338,135,1,100);
        enabled_=button(parent,"",646,343,146,38,Enabled);
        refresh(backend);
        return true;
    }

    void refresh(IUiBackend& backend) override {
        if(!play_) return;
        const auto* status=backend.sequencerStatus();
        const auto* pattern=backend.sequencerPattern();
        const auto result=backend.sequencerResult();
        text(transport_,status?(status->playing?"PLAYING":"STOPPED"):"ENGINE UNAVAILABLE");
        const bool playing=status && status->playing;
        if(playing!=shown_playing_) {
            lv_obj_set_style_bg_color(play_,lv_color_hex(playing?palette::kActive:palette::kPanelHighlight),0);
            lv_obj_set_style_text_color(lv_obj_get_child(play_,0),
                lv_color_hex(playing?palette::kBackground:palette::kTextPrimary),0);
            shown_playing_=playing;
        }
        showFeedback(result,status);
        if(!status || !pattern) {
            show(playhead_,false);
            shown_playhead_=seq::kNoStep;
            return;
        }

        char buf[40];
        unsigned div=0;
        while(div+1<std::size(kDivisions) && kDivisions[div]!=status->division) ++div;
        std::snprintf(buf,sizeof(buf),"DIV %s",kDivisionNames[div]);
        text(lv_obj_get_child(division_,0),buf);
        text(lv_obj_get_child(quantize_,0),status->quantize?"SCALE QUANTIZE ON":"SCALE QUANTIZE OFF");

        const auto& step=pattern->steps[selected_];
        char note[8];
        noteName(note,sizeof(note),step.note);
        std::snprintf(buf,sizeof(buf),"STEP %02u  NOTE %s",selected_+1,note);
        text(selection_,buf);
        text(lv_obj_get_child(enabled_,0),step.enabled?"ENABLED / OFF":"REST / ENABLE");
        updateSlider(Length,pattern->length);
        updateSlider(Swing,status->swing);
        updateSlider(Transpose,status->transpose);
        updateSlider(Velocity,step.velocity);
        updateSlider(Gate,step.gate);

        const bool octave_changed=shown_octave_!=octave_;
        if(octave_changed) {
            for(int row=0;row<kPitchRows;++row) {
                const int pitch=octave_*12+11-row;
                if(pitch<=127) noteName(buf,sizeof(buf),pitch); else buf[0]=0;
                text(pitches_[row],buf);
            }
        }
        const bool selection_changed=shown_selected_!=selected_;
        for(unsigned i=0;i<seq::kSteps;++i) {
            const auto& cell=pattern->steps[i];
            const bool current=status->current_step==i;
            const bool was_current=shown_playhead_==i;
            if(octave_changed || cell.note!=shown_steps_[i].note || cell.enabled!=shown_steps_[i].enabled ||
               shown_length_!=pattern->length || current!=was_current || selection_changed) {
                const int row=octave_*12+11-cell.note;
                const bool visible=cell.enabled && row>=0 && row<kPitchRows;
                const char marker=!cell.enabled?'-':visible?'+':row<0?'^':'v';
                std::snprintf(buf,sizeof(buf),"%02u%c",i+1,marker);
                text(lv_obj_get_child(headers_[i],0),buf);
                const auto color=current?palette::kAccentPrimary:i==selected_?palette::kActive:
                    i>=pattern->length?palette::kBackground:palette::kPanelHighlight;
                lv_obj_set_style_bg_color(headers_[i],lv_color_hex(color),0);
                lv_obj_set_style_text_color(lv_obj_get_child(headers_[i],0),
                    lv_color_hex(current || i==selected_?palette::kBackground:palette::kTextPrimary),0);
                show(notes_[i],visible);
                if(visible) {
                    lv_obj_set_y(notes_[i],row*kRowHeight+2);
                    const auto note_color=i>=pattern->length?palette::kTextSecondary:
                        i==selected_?palette::kTextPrimary:palette::kActive;
                    lv_obj_set_style_bg_color(notes_[i],lv_color_hex(note_color),0);
                }
                shown_steps_[i]=cell;
            }
        }
        if(status->current_step!=shown_playhead_) {
            show(playhead_,status->current_step!=seq::kNoStep);
            if(status->current_step!=seq::kNoStep)
                lv_obj_set_x(playhead_,status->current_step*kColumnWidth);
        }
        shown_octave_=octave_;
        shown_selected_=selected_;
        shown_length_=pattern->length;
        shown_playhead_=status->current_step;
    }

private:
    static void noteName(char* buf,size_t size,int note) {
        std::snprintf(buf,size,"%s%d",kPitchNames[note%12],note/12-1);
    }

    lv_obj_t* button(lv_obj_t* parent,const char* label,int x,int y,int w,int h,Control control) {
        auto* obj=createActionButton(parent,label,x,y,w,h,nullptr,nullptr);
        attachLvContext<ButtonContext>(obj,{this,control},[](lv_event_t* event) {
            auto* ctx=static_cast<ButtonContext*>(lv_event_get_user_data(event));
            ctx->view->press(ctx->control);
        },LV_EVENT_CLICKED);
        return obj;
    }

    void makeSlider(lv_obj_t* parent,Parameter param,int x,int y,int width,int minimum,int maximum) {
        values_[param]=createLabel(parent,"",x,y,width+4,&lv_font_montserrat_14,palette::kTextPrimary);
        auto* slider=sliders_[param]=lv_slider_create(parent);
        lv_obj_set_pos(slider,x+7,y+23);
        lv_obj_set_size(slider,width-14,14);
        lv_obj_set_ext_click_area(slider,11);
        lv_obj_add_style(slider,&theme().slider_main,LV_PART_MAIN);
        lv_obj_add_style(slider,&theme().slider_indicator,LV_PART_INDICATOR);
        lv_obj_add_style(slider,&theme().slider_knob,LV_PART_KNOB);
        lv_slider_set_range(slider,minimum,maximum);
        attachLvContext<SliderContext>(slider,{this,param},[](lv_event_t* event) {
            auto* ctx=static_cast<SliderContext*>(lv_event_get_user_data(event));
            const auto code=lv_event_get_code(event);
            if(code!=LV_EVENT_VALUE_CHANGED && code!=LV_EVENT_RELEASED) return;
            const auto value=lv_slider_get_value(lv_event_get_current_target_obj(event));
            if(code==LV_EVENT_VALUE_CHANGED) ctx->view->formatSlider(ctx->param,value);
            if(code==LV_EVENT_RELEASED) ctx->view->commitSlider(ctx->param,value);
        },LV_EVENT_ALL);
    }

    void formatSlider(Parameter param,int value) {
        constexpr const char* formats[]{"LENGTH %d","SWING %d%%","TRANSPOSE %+d","VELOCITY %d","GATE %d%%"};
        char label[24];
        std::snprintf(label,sizeof(label),formats[param],value);
        text(values_[param],label);
    }

    void updateSlider(Parameter param,int value) {
        if(lv_obj_has_state(sliders_[param],LV_STATE_PRESSED)) return;
        if(lv_slider_get_value(sliders_[param])!=value) lv_slider_set_value(sliders_[param],value,LV_ANIM_OFF);
        formatSlider(param,value);
    }

    void commitSlider(Parameter param,int value) {
        const auto* status=backend_->sequencerStatus();
        const auto* pattern=backend_->sequencerPattern();
        if(!status || !pattern) return;
        seq::Request request;
        if(param==Velocity || param==Gate) {
            request.action=seq::Action::SetStep;
            request.index=uint8_t(selected_);
            request.step=pattern->steps[selected_];
            auto& field=param==Velocity?request.step.velocity:request.step.gate;
            if(field==value) return;
            field=uint8_t(value);
        } else {
            constexpr seq::Action actions[]{seq::Action::SetLength,seq::Action::SetSwing,seq::Action::SetTranspose};
            const int confirmed[]{pattern->length,status->swing,status->transpose};
            if(confirmed[param]==value) return;
            request.action=actions[param];
            request.value=int16_t(value);
        }
        send(request);
    }

    void chooseCell(int x,int y) {
        const auto* pattern=backend_->sequencerPattern();
        if(!pattern || x<0 || x>=kGridWidth || y<0 || y>=kPitchRows*kRowHeight) return;
        const int pitch=octave_*12+11-y/kRowHeight;
        if(pitch>127) return;
        selected_=unsigned(x/kColumnWidth);
        saveSelection();
        seq::Request request;
        request.action=seq::Action::SetStep;
        request.index=uint8_t(selected_);
        request.step=pattern->steps[selected_];
        confirm_=Play;
        if(request.step.enabled && request.step.note==pitch) {
            refresh(*backend_);
            return;
        }
        request.step.note=uint8_t(pitch);
        request.step.enabled=true;
        send(request);
    }

    void press(Control control) {
        const auto* status=backend_->sequencerStatus();
        const auto* pattern=backend_->sequencerPattern();
        if(!status || !pattern) return;
        const bool confirmed=confirm_==control;
        confirm_=Play;
        if(control>=SelectFirst) {
            selected_=unsigned(control-SelectFirst);
            const auto& step=pattern->steps[selected_];
            if(step.enabled) octave_=step.note/12;
            saveSelection();
            refresh(*backend_);
            return;
        }
        if(control==OctaveDown || control==OctaveUp) {
            octave_=std::clamp(octave_+(control==OctaveUp?1:-1),0,10);
            saveSelection();
            refresh(*backend_);
            return;
        }

        seq::Request request;
        switch(control) {
        case Play: request.action=seq::Action::Play; break;
        case Stop: request.action=seq::Action::Stop; break;
        case Restart: request.action=seq::Action::Restart; break;
        case Division: {
            request.action=seq::Action::SetDivision;
            unsigned i=0;
            while(i+1<std::size(kDivisions) && kDivisions[i]!=status->division) ++i;
            request.value=int16_t(kDivisions[(i+1)%std::size(kDivisions)]);
            break;
        }
        case Quantize:
            request.action=seq::Action::SetQuantize;
            request.value=!status->quantize;
            break;
        case Enabled:
            request.action=seq::Action::SetStep;
            request.index=uint8_t(selected_);
            request.step=pattern->steps[selected_];
            request.step.enabled=!request.step.enabled;
            break;
        case Demo:
        case Clear: {
            const bool edited=std::any_of(pattern->steps.begin(),pattern->steps.end(),[](seq::Step step) {
                return step.enabled || step.note!=60 || step.velocity!=100 || step.gate!=50;
            });
            if(edited && !confirmed) {
                confirm_=control;
                refresh(*backend_);
                return;
            }
            request.action=seq::Action::SetPattern;
            request.pattern.length=pattern->length;
            if(control==Demo) {
                request.pattern.length=seq::kSteps;
                request.pattern.steps={{{60,108,78,true},{},{67,92,48,true},{64,100,62,true},
                                        {69,106,82,true},{},{67,89,36,true},{},
                                        {72,117,88,true},{71,84,45,true},{67,94,70,true},{},
                                        {65,98,55,true},{64,88,38,true},{62,95,64,true},{}}};
                octave_=5;
                saveSelection();
            }
            break;
        }
        default: return;
        }
        send(request);
    }

    void send(const seq::Request& request) {
        confirm_=Play;
        backend_->requestSequencer(request);
        refresh(*backend_);
    }

    void showFeedback(UiCommandResult result,const seq::Status* status) {
        const char* message=result.notice;
        char drops[54];
        if(confirm_==Demo) message="TAP LOAD DEMO AGAIN";
        if(confirm_==Clear) message="TAP CLEAR AGAIN";
        if(!message && status && status->playing && status->voice_drops) {
            std::snprintf(drops,sizeof(drops),"VOICE DROPS %lu",static_cast<unsigned long>(status->voice_drops));
            message=drops;
        }
        text(feedback_,message?message:"");
    }

    void saveSelection() { nav_->setSubTab(id(),uint8_t(octave_*seq::kSteps+selected_+1)); }

    IUiBackend* backend_=nullptr;
    UiNavigationManager* nav_=nullptr;
    lv_obj_t *play_=nullptr,*division_=nullptr,*quantize_=nullptr,*transport_=nullptr,
             *feedback_=nullptr,*selection_=nullptr,*enabled_=nullptr,*playhead_=nullptr;
    lv_obj_t* sliders_[ParameterCount]{};
    lv_obj_t* values_[ParameterCount]{};
    lv_obj_t* pitches_[kPitchRows]{};
    lv_obj_t* headers_[seq::kSteps]{};
    lv_obj_t* notes_[seq::kSteps]{};
    std::array<seq::Step,seq::kSteps> shown_steps_{};
    int octave_=5,shown_octave_=-1;
    unsigned selected_=0,shown_selected_=seq::kSteps,shown_length_=0;
    uint8_t shown_playhead_=seq::kNoStep;
    bool shown_playing_=false;
    Control confirm_=Play;
};

} // namespace

ScreenView* createSequencerScreen() {
    return new SequencerScreen();
}

} // namespace smk::p4::ui
