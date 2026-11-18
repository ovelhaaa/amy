#include "ui_screens.h"
#include "ui_widgets.h"
#include "../../amy_runtime/patch_library.h"
#include <cstdio>
#include <cstring>

namespace smk::p4::ui {
namespace {

constexpr const char* kDomains[]{"PATCH","PATTERN","FX","HARMONY","ARP","MACROS","TEMPO"};
constexpr uint8_t kLocks[]{0,0,perf_lock::Fx,perf_lock::Harmony,perf_lock::Arp,perf_lock::Macros,0};

void setText(lv_obj_t* label,const char* text) {
    if(std::strcmp(lv_label_get_text(label),text)) lv_label_set_text(label,text);
}

const char* slotState(seq::SlotState state) {
    switch(state) {
    case seq::SlotState::Empty: return "EMPTY";
    case seq::SlotState::Valid: return "VALID";
    case seq::SlotState::Corrupted: return "CORRUPTED";
    case seq::SlotState::Incompatible: return "INCOMPATIBLE";
    }
    return "";
}

const char* recallPhase(scene::RecallPhase phase) {
    switch(phase) {
    case scene::RecallPhase::Idle: return "";
    case scene::RecallPhase::WaitingBoundary: return "QUEUED / NEXT BAR";
    case scene::RecallPhase::WaitingPatch: return "WAITING PATCH / AUDIO MUTED";
    case scene::RecallPhase::Confirmed: return "RECALL CONFIRMED";
    case scene::RecallPhase::Failed: return "RECALL FAILED";
    case scene::RecallPhase::PartialApply: return "PARTIAL APPLY";
    case scene::RecallPhase::Cancelled: return "RECALL CANCELLED";
    }
    return "";
}

const char* rejectionText(scene::Reject rejection) {
    switch(rejection) {
    case scene::Reject::None: return "";
    case scene::Reject::InvalidScene: return "INVALID SCENE";
    case scene::Reject::EmptySelection: return "NO EFFECTIVE DOMAINS";
    case scene::Reject::PatchUnavailable: return "PATCH UNAVAILABLE";
    case scene::Reject::PatternUnavailable: return "PATTERN REFERENCE UNAVAILABLE";
    case scene::Reject::InvalidFx: return "INVALID FX";
    case scene::Reject::InvalidHarmony: return "INVALID HARMONY";
    case scene::Reject::InvalidArp: return "INVALID ARP";
    case scene::Reject::IncompatibleMacros: return "MACRO ROUTES INCOMPATIBLE WITH PATCH";
    case scene::Reject::InvalidTempo: return "INVALID TEMPO";
    case scene::Reject::CompareActive: return "EXIT SOUND COMPARE TO RECALL MACROS";
    }
    return "";
}

const char* dirtyText(scene::Dirty dirty) {
    switch(dirty) {
    case scene::Dirty::Clean: return "CLEAN";
    case scene::Dirty::Modified: return "MODIFIED";
    case scene::Dirty::Unsaved: return "UNSAVED";
    case scene::Dirty::Invalid: return "INVALID";
    case scene::Dirty::Pending: return "PENDING";
    }
    return "";
}

void domainText(char (&text)[64],uint8_t domains) {
    unsigned used=0;
    for(unsigned i=0;i<7;++i) {
        if(domains&(1u<<i)) used+=unsigned(std::snprintf(text+used,sizeof(text)-used,"%s%s",
            used?" / ":"",kDomains[i]));
    }
    if(!used) std::strcpy(text,"NONE");
}

class ScenesScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::Scenes; }
    bool build(lv_obj_t* parent,IUiBackend& backend,UiNavigationManager&) override {
        parent_=parent;
        backend_=&backend;
        if(!backend.sceneSlot(0)) {
            createLabel(parent,"SCENE LIBRARY UNAVAILABLE",12,16,760,&lv_font_montserrat_14,palette::kWarning);
            return true;
        }

        for(unsigned i=0;i<scene::kSlots;++i) {
            auto& card=cards_[i];
            card.button=createActionButton(parent,"",12+194*(i%4),12+114*(i/4),184,102,nullptr,nullptr);
            lv_obj_add_flag(lv_obj_get_child(card.button,0),LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_pad_all(card.button,0,0);
            card.title=createLabel(card.button,"",10,9,168,&lv_font_montserrat_14,palette::kTextPrimary);
            card.patch=createLabel(card.button,"",10,33,168,&lv_font_montserrat_14,palette::kTextPrimary);
            lv_label_set_long_mode(card.title,LV_LABEL_LONG_MODE_DOTS);
            lv_label_set_long_mode(card.patch,LV_LABEL_LONG_MODE_DOTS);
            card.pattern=createLabel(card.button,"",10,54,168,&lv_font_montserrat_14,palette::kTextPrimary);
            card.state=createLabel(card.button,"",10,76,168,&lv_font_montserrat_14,palette::kTextPrimary);
            attachLvContext<Choice>(card.button,{this,uint8_t(i)},[](lv_event_t* e) {
                const auto& choice=*static_cast<const Choice*>(lv_event_get_user_data(e));
                choice.screen->slot_=choice.slot;
                choice.screen->refresh(*choice.screen->backend_);
            },LV_EVENT_CLICKED);
        }
        button(parent,"PREVIOUS",12,242,100,Action::Previous);
        button(parent,"NEXT",120,242,76,Action::Next);
        button(parent,"LOAD",204,242,76,Action::Load);
        button(parent,"SAVE",288,242,76,Action::Save);
        button(parent,"RENAME",372,242,92,Action::Rename);
        button(parent,"DELETE",472,242,86,Action::Delete);
        button(parent,"CANCEL QUEUED",566,242,132,Action::Cancel);
        button(parent,"PANIC",706,242,80,Action::Panic);
        status_=createLabel(parent,"",12,303,774,&lv_font_montserrat_14,palette::kAccentPrimary);
        detail_=createLabel(parent,"",12,328,774,&lv_font_montserrat_14,palette::kTextPrimary);
        refresh(backend);
        return true;
    }

    void refresh(IUiBackend& backend) override {
        if(!status_) return;
        const auto view=backend.sceneView();
        for(unsigned i=0;i<scene::kSlots;++i) {
            const auto* slot=backend.sceneSlot(i);
            if(!slot) continue;
            auto& card=cards_[i];
            const auto& config=slot->config;
            const bool valid=slot->state==seq::SlotState::Valid;
            char buffer[64];
            std::snprintf(buffer,sizeof(buffer),"%02u %s",i+1,valid?config.name:slotState(slot->state));
            setText(card.title,buffer);
            const auto* patch=valid && (config.domains&scene::Patch)?library::find(config.patch_id):nullptr;
            setText(card.patch,patch?patch->name:"");
            if(valid && (config.domains&scene::Pattern)) {
                std::snprintf(buffer,sizeof(buffer),"PATTERN %02u",unsigned(config.pattern_slot)+1);
                setText(card.pattern,buffer);
            } else setText(card.pattern,"");
            const char* playing=view.recall.pending==i?"PENDING":view.recall.active==i?"ACTIVE":"";
            const char* dirty=view.reference==i && view.recall.pending!=i?dirtyText(view.dirty):"";
            std::snprintf(buffer,sizeof(buffer),"%s%s%s",playing,*playing && *dirty?" / ":"",dirty);
            setText(card.state,*buffer?buffer:valid?"VALID":"");
            const bool highlighted=i==slot_ || view.recall.active==i || view.recall.pending==i;
            const uint32_t highlight=view.recall.pending==i?palette::kWarning:
                view.recall.active==i?palette::kActive:palette::kAccentPrimary;
            if(card.highlighted!=highlighted || card.highlight!=highlight) {
                card.highlighted=highlighted;
                card.highlight=highlight;
                lv_obj_set_style_outline_width(card.button,highlighted?2:0,0);
                lv_obj_set_style_outline_color(card.button,lv_color_hex(highlight),0);
            }
        }
        char status[120],applied[64];
        std::snprintf(status,sizeof(status),"%s%s%s",*view.notice?view.notice:recallPhase(view.recall.phase),
            view.selective?"  /  ":"",view.selective?"SELECTIVE RECALL":"");
        setText(status_,status);
        domainText(applied,view.recall.applied_domains);
        if(view.recall.error!=scene::RecallError::None) {
            char detail[150];
            std::snprintf(detail,sizeof(detail),"APPLIED %s   %s   CODE %u / PATCH %d",
                applied,rejectionText(view.recall.rejection),unsigned(view.recall.error),view.recall.patch_error);
            setText(detail_,detail);
        } else setText(detail_,"");
        if(modal_ && mask_) refreshSelection();
    }

private:
    enum class Action { Previous,Next,Load,Save,Rename,Delete,Cancel,Panic,Close,Apply,Full,Mode,Tempo,Discard };
    struct Binding { ScenesScreen* screen; Action action; };
    struct Choice { ScenesScreen* screen; uint8_t slot; };
    struct Card {
        lv_obj_t *button=nullptr,*title=nullptr,*patch=nullptr,*pattern=nullptr,*state=nullptr;
        uint32_t highlight=0;
        bool highlighted=false;
    };

    lv_obj_t* button(lv_obj_t* parent,const char* label,int x,int y,int width,Action action) {
        auto* object=createActionButton(parent,label,x,y,width,44,nullptr,nullptr);
        attachLvContext<Binding>(object,{this,action},[](lv_event_t* e) {
            const auto& binding=*static_cast<const Binding*>(lv_event_get_user_data(e));
            binding.screen->action(binding.action);
        },LV_EVENT_CLICKED);
        return object;
    }

    void action(Action selected) {
        switch(selected) {
        case Action::Previous: slot_=uint8_t((slot_+scene::kSlots-1)%scene::kSlots); break;
        case Action::Next: slot_=uint8_t((slot_+1)%scene::kSlots); break;
        case Action::Load: open(SceneAction::Load); break;
        case Action::Save: open(SceneAction::Save); break;
        case Action::Rename: open(SceneAction::Rename); break;
        case Action::Delete: open(SceneAction::Delete); break;
        case Action::Cancel: backend_->requestScene({SceneAction::Cancel}); break;
        case Action::Panic: backend_->requestPanic(); close(); break;
        case Action::Close: close(); break;
        case Action::Mode:
            mode_=mode_==scene::RecallMode::Immediate?scene::RecallMode::NextBar:scene::RecallMode::Immediate;
            break;
        case Action::Tempo: tempo_=!tempo_; break;
        case Action::Discard: discard_=!discard_; break;
        case Action::Full:
        case Action::Apply: {
            SceneOperation operation;
            operation.action=operation_;
            operation.slot=slot_;
            operation.domains=selected==Action::Full?stored_:selectedDomains();
            operation.mode=mode_;
            operation.locks=sessionLockMask();
            operation.apply_tempo=tempo_;
            operation.discard_pattern=discard_;
            if(name_) std::snprintf(operation.name,sizeof(operation.name),"%s",lv_textarea_get_text(name_));
            backend_->requestScene(operation);
            close();
            break;
        }
        }
        refresh(*backend_);
    }

    void close() {
        if(modal_) lv_obj_delete(modal_);
        modal_=mask_=compatibility_=name_=keyboard_=mode_label_=tempo_label_=discard_label_=nullptr;
        for(auto& control:domains_) control=nullptr;
    }

    uint8_t selectedDomains() const {
        uint8_t result=0;
        for(unsigned i=0;i<7;++i)
            if(domains_[i] && lv_obj_has_state(domains_[i],LV_STATE_CHECKED)) result|=uint8_t(1u<<i);
        return result&stored_;
    }

    void refreshSelection() {
        const bool load=operation_==SceneAction::Load;
        char effective[64],row[150];
        const auto domains=selectedDomains();
        domainText(effective,load?scene::effectiveDomains(stored_,domains,sessionLockMask(),tempo_):domains);
        std::snprintf(row,sizeof(row),"%s %s",load?"EFFECTIVE":"SAVE",effective);
        setText(mask_,row);
        if(load) {
            SceneOperation operation;
            operation.slot=slot_;
            operation.domains=domains;
            operation.locks=sessionLockMask();
            operation.apply_tempo=tempo_;
            setText(compatibility_,rejectionText(backend_->sceneCompatibility(operation)));
            setText(mode_label_,mode_==scene::RecallMode::NextBar?"NEXT BAR":"IMMEDIATE");
            setText(tempo_label_,tempo_?"SCENE TEMPO ON":"SCENE TEMPO OFF");
            setText(discard_label_,discard_?"DISCARD PATTERN ON":"DISCARD PATTERN OFF");
        }
    }

    void open(SceneAction operation) {
        const auto* record=backend_->sceneSlot(slot_);
        if(!record) return;
        close();
        operation_=operation;
        stored_=operation==SceneAction::Save?backend_->sceneCaptureDomains():record->config.domains;
        modal_=lv_obj_create(parent_);
        applyPanelStyle(modal_,false);
        lv_obj_set_style_pad_all(modal_,0,0);
        lv_obj_set_pos(modal_,12,4);
        lv_obj_set_size(modal_,776,376);
        lv_obj_remove_flag(modal_,LV_OBJ_FLAG_SCROLLABLE);
        const char* title=operation==SceneAction::Load?"SELECTIVE RECALL":
            operation==SceneAction::Save?"SAVE SCENE":operation==SceneAction::Rename?"RENAME SCENE":"DELETE SCENE";
        char heading[100];
        std::snprintf(heading,sizeof(heading),"%s %02u  %s",title,unsigned(slot_)+1,record->config.name);
        createLabel(modal_,heading,14,14,630,&lv_font_montserrat_14,palette::kAccentPrimary);
        button(modal_,"PANIC",680,6,80,Action::Panic);
        button(modal_,"CLOSE",14,320,100,Action::Close);
        button(modal_,operation==SceneAction::Load?"SELECTIVE RECALL":
            operation==SceneAction::Save?"SAVE SCENE":operation==SceneAction::Rename?"RENAME SCENE":"DELETE SCENE",
            544,320,216,Action::Apply);

        if(operation==SceneAction::Rename || operation==SceneAction::Save) {
            name_=lv_textarea_create(modal_);
            lv_textarea_set_one_line(name_,true);
            lv_textarea_set_max_length(name_,seq::kPatternNameBytes-1);
            lv_textarea_set_text(name_,record->config.name);
            lv_textarea_set_placeholder_text(name_,"Scene name");
            lv_obj_set_pos(name_,14,48);
            lv_obj_set_size(name_,400,38);
            lv_obj_add_event_cb(name_,[](lv_event_t* e) {
                auto* self=static_cast<ScenesScreen*>(lv_event_get_user_data(e));
                if(self->keyboard_) return;
                auto* keyboard=self->keyboard_=lv_keyboard_create(self->modal_);
                lv_obj_set_pos(keyboard,14,126);
                lv_obj_set_size(keyboard,746,184);
                lv_keyboard_set_textarea(keyboard,self->name_);
                for(auto event:{LV_EVENT_READY,LV_EVENT_CANCEL})
                    lv_obj_add_event_cb(keyboard,[](lv_event_t* key) {
                        auto* screen=static_cast<ScenesScreen*>(lv_event_get_user_data(key));
                        lv_obj_delete(screen->keyboard_);
                        screen->keyboard_=nullptr;
                    },event,self);
            },LV_EVENT_CLICKED,this);
        }
        if(operation!=SceneAction::Load && operation!=SceneAction::Save) return;
        const bool load=operation==SceneAction::Load;
        for(unsigned i=0;i<7;++i) {
            auto* box=lv_checkbox_create(modal_);
            domains_[i]=box;
            char label[32];
            const bool available=stored_&(1u<<i);
            const bool locked=load && (sessionLockMask()&kLocks[i]);
            std::snprintf(label,sizeof(label),"%s%s",kDomains[i],
                !available?" / ABSENT":locked?" / LOCKED":"");
            lv_checkbox_set_text(box,label);
            lv_obj_set_pos(box,14+188*(i%4),98+45*(i/4));
            lv_obj_set_size(box,184,42);
            if(available && !locked) lv_obj_add_state(box,LV_STATE_CHECKED);
            else lv_obj_add_state(box,LV_STATE_DISABLED);
        }
        mask_=createLabel(modal_,"",14,191,746,&lv_font_montserrat_14,palette::kTextPrimary);
        if(load) {
            compatibility_=createLabel(modal_,"",14,214,746,&lv_font_montserrat_14,palette::kWarning);
            mode_label_=lv_obj_get_child(button(modal_,"IMMEDIATE",14,240,128,Action::Mode),0);
            tempo_label_=lv_obj_get_child(button(modal_,"SCENE TEMPO OFF",154,240,190,Action::Tempo),0);
            discard_label_=lv_obj_get_child(button(modal_,"DISCARD PATTERN OFF",356,240,220,Action::Discard),0);
            button(modal_,"FULL RECALL",344,320,185,Action::Full);
        }
        refreshSelection();
    }

    IUiBackend* backend_=nullptr;
    lv_obj_t *parent_=nullptr,*status_=nullptr,*detail_=nullptr,*modal_=nullptr,*mask_=nullptr,*compatibility_=nullptr,*name_=nullptr,
             *mode_label_=nullptr,*tempo_label_=nullptr,*discard_label_=nullptr,*keyboard_=nullptr;
    lv_obj_t* domains_[7]{};
    Card cards_[scene::kSlots]{};
    uint8_t slot_=0,stored_=0;
    SceneAction operation_=SceneAction::Load;
    scene::RecallMode mode_=scene::RecallMode::Immediate;
    bool tempo_=false,discard_=false;
};

}

ScreenView* createScenesScreen() { return new ScenesScreen; }

}
