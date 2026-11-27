#include "harmonic_pads_view.h"
#include <cstdio>
#include <cstring>
namespace smk::p4::ui {
namespace {
constexpr const char* scales[]={"CHROMATIC","MAJOR","MINOR","HARM MIN","MAJ PENTA","MIN PENTA","BLUES","DORIAN","PHRYGIAN","LYDIAN","MIXOLYDIAN"};
constexpr const char* degrees[]={"I","II","III","IV","V","VI","VII","I'"};
constexpr const char* minorDegrees[]={"i","ii","iii","iv","v","vi","vii","i'"};
void textIfChanged(lv_obj_t* label,const char* text) {
    if(std::strcmp(lv_label_get_text(label),text)==0)return;
    lv_label_set_text(label,text);
#ifndef ESP_PLATFORM
    ++HarmonicPadsView::updates.labels;
#endif
}
}
HarmonicPadsView::~HarmonicPadsView() {
    releaseAll();
    if(root_)lv_obj_delete(root_);
}
void HarmonicPadsView::release(Pad& p) {
    if(p.token && backend_)backend_->requestPadOff(p.id,p.token);
    p.down=false;
}
void HarmonicPadsView::releaseAll(){for(auto& p:pads_)release(p);}
bool HarmonicPadsView::build(lv_obj_t* parent,IUiBackend& backend) {
    backend_=&backend;root_=lv_obj_create(parent);
    if(!root_)return false;
    lv_obj_remove_style_all(root_);lv_obj_set_pos(root_,8,102);lv_obj_set_size(root_,784,282);
    lv_obj_remove_flag(root_,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(root_,[](lv_event_t* e){
        auto* self=static_cast<HarmonicPadsView*>(lv_event_get_user_data(e));
        self->releaseAll();self->root_=nullptr;
    },LV_EVENT_DELETE,this);
    const int x[]={0,158,430,566},w[]={150,264,128,218};
    struct Control {HarmonicPadsView* self;unsigned id;};
    for(unsigned i=0;i<4;++i) {
        controls_[i]=createActionButton(root_,"",x[i],0,w[i],44,nullptr,nullptr);
        attachLvContext<Control>(controls_[i],{this,i},[](lv_event_t* e){
            auto* c=static_cast<Control*>(lv_event_get_user_data(e));auto& v=*c->self;
            if(c->id<3)v.selector(c->id);
            else {v.releaseAll();v.hold_=!v.hold_;}
            v.labels();v.refresh();
        },LV_EVENT_CLICKED);
    }
    for(unsigned i=0;i<8;++i) {
        auto& p=pads_[i];p.owner=this;p.id=i;
        p.button=createActionButton(root_,"",int(i%4)*198,52+int(i/4)*84,190,80,nullptr,nullptr);
        lv_obj_add_event_cb(p.button,event,LV_EVENT_ALL,&p);
    }
    info_=createLabel(root_,"PADS: READY",0,220,784,&lv_font_montserrat_14,palette::kTextSecondary);
    auto* release=createActionButton(root_,"RELEASE ALL PADS",0,240,236,42,nullptr,nullptr);
    attachLvContext<HarmonicPadsView*>(release,this,[](lv_event_t* e){
        auto* v=*static_cast<HarmonicPadsView**>(lv_event_get_user_data(e));v->releaseAll();v->refresh();
    },LV_EVENT_CLICKED);
    context_=createLabel(root_,"",250,254,530,&lv_font_montserrat_14,palette::kTextSecondary);
    labels();refresh();return true;
}
void HarmonicPadsView::labels() {
    char text[48];
    std::snprintf(text,sizeof(text),"ROOT %s",pads::kPitchNames[config_.root]);textIfChanged(lv_obj_get_child(controls_[0],0),text);
    std::snprintf(text,sizeof(text),"SCALE %s",scales[config_.scale]);textIfChanged(lv_obj_get_child(controls_[1],0),text);
    std::snprintf(text,sizeof(text),"OCT %u",config_.octave);textIfChanged(lv_obj_get_child(controls_[2],0),text);
    textIfChanged(lv_obj_get_child(controls_[3],0),hold_?"HOLD":"MOMENTARY");
}
void HarmonicPadsView::event(lv_event_t* e) {
    auto* p=static_cast<Pad*>(lv_event_get_user_data(e));if(!p || !p->owner)return;
    auto& v=*p->owner;const auto code=lv_event_get_code(e);
    if(code==LV_EVENT_DELETE){v.release(*p);p->button=nullptr;return;}
    if(code==LV_EVENT_PRESSED) {
        if(p->down)return;
        p->down=true;v.last_pad_=p->id;
        if(v.hold_ && p->token){v.release(*p);p->down=true;}
        else {
            const auto intent=pads::resolve(p->id,config_);
            const auto notes=intent.valid?harmony::transform(intent.input,intent.configuration):harmony::Notes{};
            const auto* q=pads::quality(notes);p->minor=*q=='m' || *q=='d';
            p->input=intent.input;pads::name(intent,p->chord,sizeof(p->chord));p->token=v.backend_->requestPadOn(p->id,config_);p->operation=p->token;p->requested_ms=lv_tick_get();p->result=pads::Result::None;}
    } else if(code==LV_EVENT_RELEASED || code==LV_EVENT_PRESS_LOST) {
        p->down=false;if(!v.hold_)v.release(*p);
    } else return;
    v.refresh();
}
bool HarmonicPadsView::octaveValid(unsigned octave) const {
    auto c=config_;c.octave=uint8_t(octave);
    for(unsigned i=0;i<8;++i) {
        const auto intent=pads::resolve(i,c);
        if(!intent.valid || harmony::transform(intent.input,intent.configuration).error!=harmony::Error::None)return false;
    }
    return true;
}
void HarmonicPadsView::selector(unsigned kind) {
    // Reuse a bounded local overlay; callbacks only hide it, never destroy their context.
    if(selector_)lv_obj_delete(selector_);
    selector_=lv_obj_create(root_);lv_obj_remove_style_all(selector_);
    lv_obj_set_pos(selector_,0,0);lv_obj_set_size(selector_,784,282);
    lv_obj_set_style_bg_color(selector_,lv_color_hex(palette::kBackground),0);
    lv_obj_set_style_bg_opa(selector_,LV_OPA_COVER,0);
    lv_obj_remove_flag(selector_,LV_OBJ_FLAG_SCROLLABLE);
    createLabel(selector_,kind==0?"PADS LOCAL: ROOT":kind==1?"PADS LOCAL: SCALE (* requer 7 notas)":"PADS LOCAL: OCTAVE",8,12,620,&lv_font_montserrat_14,palette::kTextSecondary);
    auto* close=createActionButton(selector_,"FECHAR",652,0,132,44,nullptr,nullptr);
    attachLvContext<HarmonicPadsView*>(close,this,[](lv_event_t* e){auto* v=*static_cast<HarmonicPadsView**>(lv_event_get_user_data(e));lv_obj_add_flag(v->selector_,LV_OBJ_FLAG_HIDDEN);},LV_EVENT_CLICKED);
    struct Choice {HarmonicPadsView* self;unsigned kind,value;};
    const unsigned count=kind==0?12:kind==1?11:10;
    for(unsigned i=0;i<count;++i) {
        char text[48];
        if(kind==0)std::snprintf(text,sizeof(text),"%s",pads::kPitchNames[i]);
        else if(kind==1)std::snprintf(text,sizeof(text),"%s%s",scales[i],harmony::degrees(i)==7?"":" *");
        else std::snprintf(text,sizeof(text),"OCT %u%s",i,octaveValid(i)?"":" - MIDI");
        const unsigned cols=kind==1?3:4;
        auto* btn=createActionButton(selector_,text,int(i%cols)*int(784/cols),54+int(i/cols)*54,int(784/cols)-8,48,nullptr,nullptr);
        if(kind==2 && !octaveValid(i))lv_obj_add_state(btn,LV_STATE_DISABLED);
        attachLvContext<Choice>(btn,{this,kind,i},[](lv_event_t* e){
            auto* c=static_cast<Choice*>(lv_event_get_user_data(e));auto& v=*c->self;
            if(c->kind==0)v.config_.root=uint8_t(c->value);
            else if(c->kind==1)v.config_.scale=uint8_t(c->value);
            else {if(!v.octaveValid(c->value))return;v.config_.octave=uint8_t(c->value);}
            lv_obj_add_flag(v.selector_,LV_OBJ_FLAG_HIDDEN);v.labels();v.refresh();
        },LV_EVENT_CLICKED);
    }
}
void HarmonicPadsView::refresh() {
    if(!backend_ || !root_)return;
    backend_->refreshPadLease();
#ifndef ESP_PLATFORM
    ++updates.heartbeats;
#endif
    const auto* state=backend_->padState();
    if(state && generation_!=state->generation){
        releaseAll();generation_=state->generation;
        for(auto& p:pads_){p.token=p.operation=0;p.result=pads::Result::None;}
        last_pad_=8;
    }
    const bool unsupported=harmony::degrees(config_.scale)!=7;
    char summary[160];std::snprintf(summary,sizeof(summary),"%s | VOZES %u/8",unsupported?"ESCALA REQUER 7 NOTAS":"PADS READY",state?state->voices:0);
    for(auto& p:pads_) {
        if(!p.button)continue;
        const auto* status=state?&state->pads[p.id]:nullptr;
        const bool matching=status && p.operation && status->token==p.operation && status->generation==generation_;
        const bool active=matching && p.token && status->active_token==p.operation;
        if(matching && p.token){
            if(status->result==pads::Result::Expired && p.result!=pads::Result::Expired)last_pad_=p.id;
            p.result=status->result;
            if(!active && status->result!=pads::Result::None){p.token=0;}
        }
        // No owner acknowledgement: cancel intent via the release latch, without inventing Released.
        if(p.token && !active && uint32_t(lv_tick_get()-p.requested_ms)>=pads::kLeaseMs){release(p);p.token=0;}
        char chord[24],text[64];const auto intent=pads::resolve(p.id,config_);pads::name(intent,chord,sizeof(chord));
        const auto preview=intent.valid?harmony::transform(intent.input,intent.configuration):harmony::Notes{};
        const auto* quality=pads::quality(preview);
        const auto* degree=(*quality=='m' || *quality=='d')?minorDegrees[p.id]:degrees[p.id];
        // Active and pending intents retain their captured identity, even if the next configuration is invalid.
        const bool frozen=active || p.token;
        const bool disabled=!frozen && (!intent.valid || preview.error!=harmony::Error::None);
        if(lv_obj_has_state(p.button,LV_STATE_DISABLED)!=disabled){
            if(disabled)lv_obj_add_state(p.button,LV_STATE_DISABLED);else lv_obj_remove_state(p.button,LV_STATE_DISABLED);
#ifndef ESP_PLATFORM
            ++updates.styles;
#endif
        }
        if(frozen){std::snprintf(chord,sizeof(chord),"%s",p.chord);degree=p.minor?minorDegrees[p.id]:degrees[p.id];}
        if(disabled)std::snprintf(text,sizeof(text),"%s",unsupported?"SEM GRAUS\n7 NOTAS":"FORA DA FAIXA\nMIDI");
        else std::snprintf(text,sizeof(text),"%s%s\n%s | %s",chord,p.id==7?"'":"",degree,active?"ACTIVE":p.token?"PENDING":p.result==pads::Result::Released && intent.input==p.input && std::strcmp(chord,p.chord)==0?"RELEASED":"READY");
        textIfChanged(lv_obj_get_child(p.button,0),text);
        const uint32_t color=disabled?palette::kUnavailableBadge:active?palette::kActive:palette::kPanelHighlight;
        if(color!=p.color){p.color=color;lv_obj_set_style_bg_color(p.button,lv_color_hex(color),0);
#ifndef ESP_PLATFORM
            ++updates.styles;
#endif
        }
        if(p.id==last_pad_) {
            const char* result=p.result==pads::Result::Capacity?"VOZES INSUFICIENTES":p.result==pads::Result::Invalid?"CONFIG INVALIDA":p.result==pads::Result::Expired?"HOLD EXPIRED - LIBERADO POR SEGURANCA":p.result==pads::Result::Released?"RELEASED (OWNER)":p.result==pads::Result::Admitted?"VOZES ADMITIDAS":p.token?"PENDING":"SEM CONFIRMACAO";
            std::snprintf(summary,sizeof(summary),"LAST: %s | %s | VOZES %u/8",p.chord,result,state?state->voices:0);
        }
    }
    labels();textIfChanged(info_,summary);
    char local[96];std::snprintf(local,sizeof(local),"PADS LOCAL: %s %s | OCT %u%s",pads::kPitchNames[config_.root],scales[config_.scale],config_.octave,octaveValid(config_.octave)?"":" (INDISP.)");
    textIfChanged(context_,local);
}
}
