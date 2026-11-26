#include "harmonic_pads_view.h"
#include <cstdio>
namespace smk::p4::ui {
namespace {
constexpr const char* scales[]={"CHROMATIC","MAJOR","MINOR","HARM MIN","MAJ PENTA","MIN PENTA","BLUES","DORIAN","PHRYGIAN","LYDIAN","MIXOLYDIAN"};
constexpr const char* degrees[]={"I","II","III","IV","V","VI","VII","I'"};
constexpr const char* minorDegrees[]={"i","ii","iii","iv","v","vi","vii","i'"};
}
HarmonicPadsView::~HarmonicPadsView() {
    releaseAll();
    if(root_)lv_obj_delete(root_);
}
void HarmonicPadsView::release(Pad& p) {
    if(p.token && backend_)backend_->requestPadOff(p.id,p.token);
    p.token=0;p.down=false;
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
        controls_[i]=createActionButton(root_,"",x[i],0,w[i],32,nullptr,nullptr);
        attachLvContext<Control>(controls_[i],{this,i},[](lv_event_t* e){
            auto* c=static_cast<Control*>(lv_event_get_user_data(e));auto& v=*c->self;
            switch(c->id){case 0:v.config_.root=uint8_t((v.config_.root+1)%12);break;
            case 1:v.config_.scale=uint8_t((v.config_.scale+1)%11);break;
            case 2:v.config_.octave=uint8_t((v.config_.octave+1)%9);break;
            case 3:v.releaseAll();v.hold_=!v.hold_;break;}
            v.labels();v.refresh();
        },LV_EVENT_CLICKED);
    }
    for(unsigned i=0;i<8;++i) {
        auto& p=pads_[i];p.owner=this;p.id=i;
        p.button=createActionButton(root_,"",int(i%4)*198,40+int(i/4)*88,190,80,nullptr,nullptr);
        lv_obj_add_event_cb(p.button,event,LV_EVENT_ALL,&p);
    }
    info_=createLabel(root_,"PADS: READY",0,218,784,&lv_font_montserrat_14,palette::kTextSecondary);
    auto* release=createActionButton(root_,"RELEASE ALL PADS",0,246,236,32,nullptr,nullptr);
    attachLvContext<HarmonicPadsView*>(release,this,[](lv_event_t* e){
        auto* v=*static_cast<HarmonicPadsView**>(lv_event_get_user_data(e));v->releaseAll();v->refresh();
    },LV_EVENT_CLICKED);
    createLabel(root_,"Sair libera PADS | ENCADEAR [PLAN]",250,254,530,&lv_font_montserrat_14,palette::kTextSecondary);
    labels();refresh();return true;
}
void HarmonicPadsView::labels() {
    char text[48];
    std::snprintf(text,sizeof(text),"ROOT %s",pads::kPitchNames[config_.root]);lv_label_set_text(lv_obj_get_child(controls_[0],0),text);
    std::snprintf(text,sizeof(text),"SCALE %s",scales[config_.scale]);lv_label_set_text(lv_obj_get_child(controls_[1],0),text);
    std::snprintf(text,sizeof(text),"OCT %u",config_.octave);lv_label_set_text(lv_obj_get_child(controls_[2],0),text);
    lv_label_set_text(lv_obj_get_child(controls_[3],0),hold_?"HOLD":"MOMENTARY");
}
void HarmonicPadsView::event(lv_event_t* e) {
    auto* p=static_cast<Pad*>(lv_event_get_user_data(e));if(!p || !p->owner)return;
    auto& v=*p->owner;const auto code=lv_event_get_code(e);
    if(code==LV_EVENT_DELETE){v.release(*p);p->button=nullptr;return;}
    if(code==LV_EVENT_PRESSED) {
        if(p->down)return;
        p->down=true;p->rejected=false;v.last_pad_=p->id;
        if(v.hold_ && p->token){v.release(*p);p->down=true;}
        else {pads::name(pads::resolve(p->id,config_),p->chord,sizeof(p->chord));p->token=v.backend_->requestPadOn(p->id,config_);p->requested_ms=lv_tick_get();p->rejected=!p->token;}
    } else if(code==LV_EVENT_RELEASED || code==LV_EVENT_PRESS_LOST) {
        p->down=false;if(!v.hold_)v.release(*p);
    } else return;
    v.refresh();
}
void HarmonicPadsView::refresh() {
    if(!backend_ || !root_)return;
    backend_->refreshPadLease();
    const auto* state=backend_->padState();
    if(state && generation_!=state->generation){releaseAll();generation_=state->generation;}
    bool unsupported=harmony::degrees(config_.scale)!=7;
    char summary[128];std::snprintf(summary,sizeof(summary),"%s | VOZES %u/8",unsupported?"ESCALA REQUER 7 NOTAS":"PADS",state?state->voices:0);
    for(auto& p:pads_) {
        if(!p.button)continue;
        const auto* status=state?&state->pads[p.id]:nullptr;
        const bool matching=status && p.token && status->token==p.token;
        const bool active=matching && status->active_token==p.token;
        if(matching && (status->result==pads::Result::Capacity || status->result==pads::Result::Invalid)){
            p.rejected=true;p.token=0;
        }
        if(p.token && ((matching && !active) || (uint32_t(lv_tick_get()-p.requested_ms)>=pads::kLeaseMs && !active))) release(p);
        char chord[24],text[64];const auto intent=pads::resolve(p.id,config_);pads::name(intent,chord,sizeof(chord));
        const auto preview=intent.valid?harmony::transform(intent.input,intent.configuration):harmony::Notes{};
        const auto* quality=pads::quality(preview);
        const auto* degree=(*quality=='m' || *quality=='d')?minorDegrees[p.id]:degrees[p.id];
        if(intent.valid)lv_obj_remove_state(p.button,LV_STATE_DISABLED);else lv_obj_add_state(p.button,LV_STATE_DISABLED);
        if(active)std::snprintf(chord,sizeof(chord),"%s",p.chord);
        std::snprintf(text,sizeof(text),"%s%s\n%s | %s",chord,p.id==7?"'":"",degree,active?"ACTIVE":p.rejected?"REJECTED":p.token?"PENDING":"READY");
        lv_label_set_text(lv_obj_get_child(p.button,0),text);
        lv_obj_set_style_bg_color(p.button,lv_color_hex(active?palette::kActive:p.rejected?palette::kErrorPanic:palette::kPanelHighlight),0);

    }
    if(!unsupported && last_pad_<8) {
        const auto& pad=pads_[last_pad_];
        const auto* status=state?&state->pads[last_pad_]:nullptr;
        if(pad.rejected)std::snprintf(summary,sizeof(summary),"VOZES INSUFICIENTES / INDISPONIVEL | VOZES %u/8",state?state->voices:0);
        else if(status && status->count)std::snprintf(summary,sizeof(summary),"LAST: %u %u %u | VOZES %u/8 | %s",status->notes[0],status->notes[1],status->notes[2],state->voices,status->active_token?"OWNER OK":"RELEASED");
    }
    lv_label_set_text(info_,summary);
}
}
