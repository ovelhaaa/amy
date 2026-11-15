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
        createParameterCard(parent,backend,"arp.enable",8,4,116,72,compact);
        createParameterCard(parent,backend,"arp.latch",130,4,116,72,compact);
        createParameterCard(parent,backend,"arp.restart",252,4,206,72,compact);
        createParameterCard(parent,backend,"arp.rate",8,130,142,82,compact);
        createParameterCard(parent,backend,"arp.octaves",156,130,146,82,compact);
        constexpr auto timing=PresentationLevel::StageStacked;
        createParameterCard(parent,backend,"arp.gate_pct",308,130,146,82,timing);
        createParameterCard(parent,backend,"arp.swing_pct",460,130,146,82,timing);
        createParameterCard(parent,backend,"perf.bpm",612,130,180,82,timing);

        if(backend.isDemoMode()) {
            createLabel(parent,"DEMO / PREVIEW",8,226,784,&lv_font_montserrat_14,palette::kTextSecondary);
            createStepGrid(parent,backend,nav,StepGridKind::ArpPreview,8,260,784,104);
            return true;
        }

        struct ActionContext { IUiBackend* backend; unsigned action; };
        const auto addButton=[&](const char* text,int x,int y,int width,unsigned action) -> lv_obj_t* {
            auto* button=createActionButton(parent,text,x,y,width,36,nullptr,nullptr);
            if(!button) return nullptr;
            if(!backend.arpStatus()) lv_obj_add_state(button,LV_STATE_DISABLED);
            attachLvContext<ActionContext>(button,{&backend,action},[](lv_event_t* e) {
                auto* ctx=static_cast<ActionContext*>(lv_event_get_user_data(e));
                if(ctx->action==2) ctx->backend->tapTempo(lv_tick_get());
                else if(ctx->action==3) ctx->backend->requestClearLatch();
                else if(ctx->action>=4) ctx->backend->requestParameterInt("arp.direction",int(ctx->action-4));
                else ctx->backend->requestArpTransport(ctx->action==0);
            },LV_EVENT_CLICKED);
            return button;
        };
        createLabel(parent,"INTERNAL",466,4,92,&lv_font_montserrat_14,palette::kTextSecondary);
        addButton("PLAY",466,30,72,0);
        addButton("STOP",544,30,72,1);
        addButton("CLEAR LATCH",622,30,170,3);
        constexpr const char* directions[]{"UP","DOWN","UP / DOWN","RANDOM","AS PLAYED"};
        for(unsigned i=0;i<5;++i) {
            modes_[i]=addButton(directions[i],8+131*i,84,123,4+i);
            if(modes_[i]) lv_obj_set_style_bg_color(modes_[i],lv_color_hex(palette::kActive),LV_STATE_CHECKED);
        }
        addButton("TAP",663,84,129,2);
        state_=createLabel(parent,"",8,222,784,&lv_font_montserrat_14,palette::kAccentPrimary);
        held_=createLabel(parent,"",8,250,784,&lv_font_montserrat_14,palette::kTextSecondary);
        for(unsigned i=0;i<8;++i) {
            cells_[i]=lv_obj_create(parent);
            if(!cells_[i]) continue;
            applyPanelStyle(cells_[i],false);
            lv_obj_remove_flag(cells_[i],LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_pos(cells_[i],8+98*i,275);
            lv_obj_set_size(cells_[i],92,67);
            cell_labels_[i]=createLabel(cells_[i],"",2,2,76,&lv_font_montserrat_14,palette::kTextPrimary);
            if(cell_labels_[i]) lv_obj_set_height(cell_labels_[i],48);
        }
        error_=createLabel(parent,"",8,357,784,&lv_font_montserrat_14,palette::kTextSecondary);
        return true;
    }

    void refresh(IUiBackend& backend) override {
        if(backend.isDemoMode()) return;
        const auto* s=backend.arpStatus();
        if(!s) { setLabel(state_,"ARP OWNER UNAVAILABLE");return; }
        const char* transport=!s->config.enabled?"OFF":s->length?"RUNNING":s->armed?"ARMED":"STOPPED";
        char text[280];
        const bool latched=s->config.latch && !s->held_count && s->length;
        if(s->step<s->length)
            std::snprintf(text,sizeof(text),"%s   |   STEP %02u / %02u   |   GATE %s%s%s",transport,
                          unsigned(s->step)+1,unsigned(s->length),s->sounding?"ON":"OFF",
                          s->config.mode==arp::Mode::UpDown?(s->descending?"   |   DOWN":"   |   UP"):"",
                          latched?"   |   LATCHED":"");
        else std::snprintf(text,sizeof(text),"%s%s",transport,latched?"   |   LATCHED":"");
        setLabel(state_,text);
        formatNotes(text,sizeof(text),"HELD",s->held.data(),s->held_count,UINT8_MAX);
        setLabel(held_,text);
        if(mode_!=unsigned(s->config.mode)) {
            mode_=unsigned(s->config.mode);
            for(unsigned i=0;i<5;++i) if(modes_[i]) {
                if(i==mode_) lv_obj_add_state(modes_[i],LV_STATE_CHECKED);
                else lv_obj_remove_state(modes_[i],LV_STATE_CHECKED);
                if(auto* label=lv_obj_get_child(modes_[i],0))
                    lv_obj_set_style_text_color(label,lv_color_hex(i==mode_?palette::kBackground:palette::kTextPrimary),0);
            }
        }
        const unsigned page=s->step<s->length?s->step/8:0;
        for(unsigned i=0;i<8;++i) {
            if(!cells_[i]) continue;
            const unsigned index=page*8+i;
            if(index>=s->length) {
                lv_obj_add_flag(cells_[i],LV_OBJ_FLAG_HIDDEN);
                continue;
            }
            lv_obj_remove_flag(cells_[i],LV_OBJ_FLAG_HIDDEN);
            const bool active=index==s->step;
            const auto color=active?(s->sounding?palette::kActive:palette::kAccentPrimary):palette::kTextSecondary;
            lv_obj_set_style_border_color(cells_[i],lv_color_hex(color),0);
            char name[32];
            formatNotes(name,sizeof(name),"",&s->pattern[index],1,UINT8_MAX);
            std::snprintf(text,sizeof(text),"%02u%s\n%s",index+1,
                          (s->upper_octaves&(uint32_t(1)<<index))?"  +OCT":"",name);
            setLabel(cell_labels_[i],text);
        }
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
    lv_obj_t *state_=nullptr,*held_=nullptr,*error_=nullptr;
    lv_obj_t* modes_[5]{};
    lv_obj_t* cells_[8]{};
    lv_obj_t* cell_labels_[8]{};
    unsigned mode_=UINT8_MAX;
};

} // namespace

ScreenView* createArpeggiatorScreen() { return new ArpeggiatorScreen(); }

} // namespace smk::p4::ui
