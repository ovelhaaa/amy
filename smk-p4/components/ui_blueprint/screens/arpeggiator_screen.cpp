#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>
#include <cstring>

namespace smk::p4::ui {
namespace {

class ArpeggiatorScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::Arpeggiator; }

    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) override {
        constexpr auto compact=PresentationLevel::StageMinimal;
        createParameterCard(parent,backend,"arp.enable",8,4,128,98,compact);
        createParameterCard(parent,backend,"arp.latch",144,4,128,98,compact);
        createParameterCard(parent,backend,"perf.bpm",280,4,298,98,compact);
        createLabel(parent,"CLOCK: INTERNAL",596,12,196,&lv_font_montserrat_14,palette::kTextSecondary);
        createParameterCard(parent,backend,"arp.rate",8,108,145,106,compact);
        createParameterCard(parent,backend,"arp.direction",161,108,190,106,compact);
        createParameterCard(parent,backend,"arp.octaves",359,108,138,106,compact);
        createParameterCard(parent,backend,"arp.gate_pct",505,108,142,106,compact);
        createParameterCard(parent,backend,"arp.swing_pct",655,108,137,106,compact);

        if(backend.isDemoMode()) {
            createLabel(parent,"DEMO / PREVIEW",8,226,784,&lv_font_montserrat_14,palette::kTextSecondary);
            createStepGrid(parent,backend,nav,StepGridKind::ArpPreview,8,260,784,104);
            return true;
        }

        struct ActionContext { IUiBackend* backend; unsigned action; };
        const auto addButton=[&](const char* text,int x,int y,int width,unsigned action) {
            auto* button=createActionButton(parent,text,x,y,width,36,nullptr,nullptr);
            if(!button) return;
            if(!backend.arpStatus()) lv_obj_add_state(button,LV_STATE_DISABLED);
            attachLvContext<ActionContext>(button,{&backend,action},[](lv_event_t* e) {
                auto* ctx=static_cast<ActionContext*>(lv_event_get_user_data(e));
                if(ctx->action==2) ctx->backend->tapTempo(lv_tick_get());
                else ctx->backend->requestArpTransport(ctx->action==0);
            },LV_EVENT_CLICKED);
        };
        addButton("TAP TEMPO",598,42,194,2);
        addButton("PLAY",8,222,106,0);
        addButton("STOP",122,222,106,1);
        state_=createLabel(parent,"",246,230,546,&lv_font_montserrat_14,palette::kAccentPrimary);
        held_=createLabel(parent,"",8,270,784,&lv_font_montserrat_14,palette::kTextSecondary);
        pattern_=createLabel(parent,"",8,300,784,&lv_font_montserrat_14,palette::kTextPrimary);
        if(pattern_) lv_label_set_long_mode(pattern_,LV_LABEL_LONG_WRAP);
        error_=createLabel(parent,"",8,357,784,&lv_font_montserrat_14,palette::kTextSecondary);
        return true;
    }

    void refresh(IUiBackend& backend) override {
        if(backend.isDemoMode()) return;
        const auto* s=backend.arpStatus();
        if(!s) { setLabel(state_,"ARP OWNER UNAVAILABLE");return; }
        const char* transport=!s->config.enabled?"OFF":s->length?"RUNNING":s->armed?"ARMED":"STOPPED";
        char text[280];
        if(s->step<s->length)
            std::snprintf(text,sizeof(text),"%s   |   STEP %02u / %02u%s",transport,unsigned(s->step)+1,
                          unsigned(s->length),s->sounding?"   |   GATE ON":"");
        else std::snprintf(text,sizeof(text),"%s",transport);
        setLabel(state_,text);
        formatNotes(text,sizeof(text),"HELD",s->held.data(),s->held_count,UINT8_MAX);
        setLabel(held_,text);
        formatNotes(text,sizeof(text),"PATTERN",s->pattern.data(),s->length,s->step);
        setLabel(pattern_,text);
        const char* notice=backend.arpNotice();
        if(!notice && s->usb.error!=arp::Error::None) notice="SMK25 ARP REQUEST REJECTED";
        if(s->rejected)
            std::snprintf(text,sizeof(text),"%s%sDROPPED INPUTS %lu",notice?notice:"",notice?"   |   ":"",
                          static_cast<unsigned long>(s->rejected));
        else std::snprintf(text,sizeof(text),"%s",notice?notice:"");
        setLabel(error_,text);
    }

private:
    static void setLabel(lv_obj_t* label,const char* text) {
        if(label && std::strcmp(lv_label_get_text(label),text)!=0) lv_label_set_text(label,text);
    }
    static void formatNotes(char* text,size_t size,const char* title,const uint8_t* notes,
                            unsigned count,unsigned active) {
        constexpr const char* names[]{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        size_t used=unsigned(std::snprintf(text,size,"%s  %s",title,count?"":"--"));
        for(unsigned i=0;i<count && used<size;++i) {
            const unsigned note=notes[i];
            used+=unsigned(std::snprintf(text+used,size-used,"%s%s%s%d%s",i==16?"\n":" ",
                i==active?"[":"",names[note%12],int(note/12)-1,i==active?"]":""));
        }
    }
    lv_obj_t *state_=nullptr,*held_=nullptr,*pattern_=nullptr,*error_=nullptr;
};

} // namespace

ScreenView* createArpeggiatorScreen() { return new ArpeggiatorScreen(); }

} // namespace smk::p4::ui
