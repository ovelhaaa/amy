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
        Demo, Clear, Settings, Editor, Close, Normal, StepInput, StepRecord, Live, Thru, Source,
        PitchDown, PitchUp, PitchOctaveDown, PitchOctaveUp, InsertRest, ToggleStep, PrevStep, NextStep, SelectFirst
    };
    enum Parameter { Length, Swing, Transpose, Velocity, Gate, InputChannel, LiveChannel,
                     RecordGate, Reference, ParameterCount };
    struct ButtonContext { SequencerScreen* view; Control control; };
    struct SliderContext { SequencerScreen* view; Parameter param; };

public:
    ScreenId id() const override { return ScreenId::Sequencer; }

    bool build(lv_obj_t* parent,IUiBackend& backend,UiNavigationManager& nav) override {
        parent_=parent;
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
        transport_=createLabel(parent,"",255,6,74,&lv_font_montserrat_14,palette::kTextPrimary);
        midi_=createLabel(parent,"",331,6,158,&lv_font_montserrat_14,palette::kTextPrimary);
        feedback_=createLabel(parent,"",255,25,236,&lv_font_montserrat_14,palette::kWarning);
        button(parent,"LOAD DEMO PATTERN",493,4,170,36,Demo);
        button(parent,"CLEAR PATTERN",669,4,123,36,Clear);
        division_=button(parent,"",8,48,117,38,Division);
        makeSlider(parent,Length,137,45,121,1,16);
        makeSlider(parent,Swing,274,45,121,50,75);
        makeSlider(parent,Transpose,411,45,132,-24,24);
        quantize_=button(parent,"",548,48,150,38,Quantize);
        button(parent,"MIDI / REC",700,48,92,38,Settings);

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
        selection_=lv_obj_get_child(button(parent,"",154,343,165,38,Editor),0);
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
        const int transport_width=status?74:220;
        if(lv_obj_get_width(transport_)!=transport_width) lv_obj_set_width(transport_,transport_width);
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

        const auto& midi=status->midi;
        const bool editing=midi.mode!=seq::InputMode::Normal;
        if(midi_revision_!=midi.revision) {
            midi_revision_=midi.revision;
            if(editing) {
                selected_=midi.selected;
                if(capture_revision_!=midi.capture_revision && midi.captured_note!=seq::kNoStep)
                    octave_=midi.captured_note/12;
                saveSelection();
            }
        }
        capture_revision_=midi.capture_revision;
        char buf[96];
        constexpr const char* modes[]{"NORMAL","STEP INPUT","STEP RECORD"};
        std::snprintf(buf,sizeof(buf),"%s %+d%s",modes[unsigned(midi.mode)],midi.effective,midi.live?" LT":"");
        text(midi_,buf);
        if(shown_mode_!=midi.mode) {
            shown_mode_=midi.mode;
            lv_obj_set_style_text_color(midi_,lv_color_hex(midi.mode==seq::InputMode::StepRecord?
                palette::kAccentPrimary:palette::kTextPrimary),0);
        }
        if(settings_info_) {
            char last[12]="--";
            if(midi.last_note!=seq::kNoStep) noteName(last,sizeof(last),midi.last_note);
            std::snprintf(buf,sizeof(buf),"STEP %02u   LAST %s / VEL %u   %s   MANUAL %+d / LIVE %+d",
                selected_+1,last,midi.last_velocity,midi.smk_connected?"SMK25 OK":
                backend.model().diagnostics().midi_connected?"USB MIDI OK":"USB WAITING",
                status->transpose,midi.live_offset);
            text(settings_info_,buf);
            text(lv_obj_get_child(live_,0),midi.live?"LIVE TRANSPOSE ON":"LIVE TRANSPOSE OFF");
            text(lv_obj_get_child(thru_,0),midi.thru?"AUDITION ON":"AUDITION OFF");
            text(lv_obj_get_child(source_,0),midi.smk_only?"SMK25 ONLY":"ANY USB MIDI");
            updateSlider(InputChannel,midi.channel==seq::kAnyChannel?0:midi.channel+1);
            updateSlider(LiveChannel,midi.transpose_channel==seq::kAnyChannel?0:midi.transpose_channel+1);
            updateSlider(RecordGate,midi.gate);
            updateSlider(Reference,midi.reference);
        }
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
        if(editor_note_) {
            std::snprintf(buf,sizeof(buf),"STEP %02u   %s   %s",selected_+1,note,step.enabled?"ENABLED":"REST");
            text(editor_note_,buf);
        }
        updateSlider(Length,pattern->length);
        updateSlider(Swing,status->swing);
        updateSlider(Transpose,status->transpose);
        updateSlider(Velocity,step.velocity);
        updateSlider(Gate,step.gate);

        text(lv_obj_get_child(enabled_,0), midi.mode==seq::InputMode::StepRecord?"REST / NEXT":
             step.enabled?"ENABLED / OFF":"REST / ENABLE");

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
        constexpr const char* formats[]{"LENGTH %d","SWING %d%%","TRANSPOSE %+d","VELOCITY %d","GATE %d%%",
            "INPUT CH %d","TRANSPOSE CH %d","RECORD GATE %d%%","REFERENCE MIDI %d"};
        char label[24];
        if(!value && (param==InputChannel || param==LiveChannel))
            std::snprintf(label,sizeof(label),"%s CH ANY",param==InputChannel?"INPUT":"TRANSPOSE");
        else std::snprintf(label,sizeof(label),formats[param],value);
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
            request.action=param==Velocity?seq::Action::SetStepVelocity:seq::Action::SetStepGate;
            request.index=uint8_t(selected_);
        } else if(param>=InputChannel) {
            constexpr seq::Action actions[]{seq::Action::SetInputChannel,seq::Action::SetLiveChannel,
                                           seq::Action::SetRecordGate,seq::Action::SetReference};
            request.action=actions[param-InputChannel];
            if(param==InputChannel || param==LiveChannel) value=value?value-1:seq::kAnyChannel;
        } else {
            constexpr seq::Action actions[]{seq::Action::SetLength,seq::Action::SetSwing,seq::Action::SetTranspose};
            request.action=actions[param];
        }
        request.value=int16_t(value);
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
        request.index=uint8_t(selected_);
        const auto& step=pattern->steps[selected_];
        confirm_=Play;
        if(step.enabled && step.note==pitch) {
            selectStep();
            return;
        }
        request.action=seq::Action::SetStepNote;
        request.value=int16_t(pitch);
        request.select=true;
        send(request);
    }

    void selectStep() {
        const auto* status=backend_->sequencerStatus();
        if(status && (status->midi.selected!=selected_ ||
                      backend_->sequencerResult().state==UiCommandState::Requested)) {
            seq::Request request;
            request.action=seq::Action::SelectStep;
            request.value=int16_t(selected_);
            send(request);
        } else refresh(*backend_);
    }

    void closePanel() {
        if(!panel_) return;
        lv_obj_delete(panel_);
        panel_=settings_info_=editor_note_=nullptr;
        for(unsigned i=InputChannel;i<ParameterCount;++i) sliders_[i]=values_[i]=nullptr;
    }

    void openPanel(bool settings) {
        closePanel();
        panel_=rectangle(parent_,60,92,722,244,palette::kPanel);
        lv_obj_add_flag(panel_,LV_OBJ_FLAG_CLICKABLE);
        button(panel_,"CLOSE",610,10,96,40,Close);
        createLabel(panel_,settings?"MIDI / RECORD SETTINGS":"EDIT SELECTED STEP",18,18,400,
                    &lv_font_montserrat_14,palette::kTextPrimary);
        if(settings) {
            settings_info_=createLabel(panel_,"",18,52,680,&lv_font_montserrat_14,palette::kTextPrimary);
            button(panel_,"NORMAL PLAY",18,78,126,44,Normal);
            button(panel_,"STEP INPUT",151,78,124,44,StepInput);
            button(panel_,"STEP RECORD",282,78,126,44,StepRecord);
            live_=button(panel_,"",415,78,174,44,Live);
            thru_=button(panel_,"",18,136,150,46,Thru);
            source_=button(panel_,"",18,190,150,44,Source);
            makeSlider(panel_,InputChannel,192,136,230,0,16);
            makeSlider(panel_,LiveChannel,451,136,240,0,16);
            makeSlider(panel_,RecordGate,192,190,230,1,100);
            makeSlider(panel_,Reference,451,190,240,0,127);
        } else {
            editor_note_=createLabel(panel_,"",18,70,680,&lv_font_montserrat_28,palette::kTextPrimary);
            button(panel_,"SEMITONE -",18,130,156,52,PitchDown);
            button(panel_,"SEMITONE +",188,130,156,52,PitchUp);
            button(panel_,"OCTAVE -",358,130,156,52,PitchOctaveDown);
            button(panel_,"OCTAVE +",528,130,156,52,PitchOctaveUp);
            button(panel_,"INSERT REST",18,190,156,44,InsertRest);
            button(panel_,"ENABLE / REST",188,190,156,44,ToggleStep);
            button(panel_,"PREV STEP",358,190,156,44,PrevStep);
            button(panel_,"NEXT STEP",528,190,156,44,NextStep);
        }
        refresh(*backend_);
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
            selectStep();
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
        case Settings: case Editor: openPanel(control==Settings); return;
        case Close: closePanel(); return;
        case Normal: case StepInput: case StepRecord:
            request.action=seq::Action::SetInputMode;
            request.value=int16_t(control-Normal);
            request.index=uint8_t(selected_);
            break;
        case Source:
            request.action=seq::Action::SetSmkOnly;
            request.value=!status->midi.smk_only;
            break;
        case Live: case Thru:
            request.action=control==Live?seq::Action::SetLive:seq::Action::SetThru;
            request.value=control==Live?!status->midi.live:!status->midi.thru;
            break;
        case PitchDown: case PitchUp: case PitchOctaveDown: case PitchOctaveUp: {
            constexpr int offsets[]{-1,1,-12,12};
            request.action=seq::Action::ShiftStepNote;
            request.index=uint8_t(selected_);
            request.value=int16_t(offsets[control-PitchDown]);
            break;
        }
        case PrevStep: case NextStep: {
            const int direction=control==PrevStep?-1:1;
            if(status->midi.mode==seq::InputMode::Normal) {
                selected_=unsigned((int(selected_)+int(seq::kSteps)+direction)%int(seq::kSteps));
                saveSelection();
                refresh(*backend_);
                return;
            }
            request.action=seq::Action::MoveStep;
            request.value=int16_t(direction);
            break;
        }
        case ToggleStep:
            request.action=seq::Action::ToggleStepEnabled;
            request.index=uint8_t(selected_);
            break;
        case InsertRest:
            request.action=status->midi.mode==seq::InputMode::StepRecord?
                seq::Action::Rest:seq::Action::SetStepEnabled;
            request.index=uint8_t(selected_);
            break;
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
            request.action=status->midi.mode==seq::InputMode::StepRecord?seq::Action::Rest:seq::Action::ToggleStepEnabled;
            request.index=uint8_t(selected_);
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
        if(!message && status && status->midi.last_note!=seq::kNoStep) {
            char note[8];
            noteName(note,sizeof(note),status->midi.last_note);
            std::snprintf(drops,sizeof(drops),"%s  %s / VEL %u",
                status->midi.smk_connected?"SMK25":"USB MIDI",note,status->midi.last_velocity);
            message=drops;
        }
        text(feedback_,message?message:"");
    }

    void saveSelection() { nav_->setSubTab(id(),uint8_t(octave_*seq::kSteps+selected_+1)); }

    lv_obj_t *parent_=nullptr,*panel_=nullptr,*midi_=nullptr,*settings_info_=nullptr,
             *editor_note_=nullptr,*live_=nullptr,*thru_=nullptr,*source_=nullptr;
    uint32_t midi_revision_=0,capture_revision_=0;
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
    seq::InputMode shown_mode_=seq::InputMode::Normal;
    bool shown_playing_=false;
    Control confirm_=Play;
};

} // namespace

ScreenView* createSequencerScreen() {
    return new SequencerScreen();
}

} // namespace smk::p4::ui
