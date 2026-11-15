#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace smk::p4::ui {

namespace {

constexpr const char* kMidiTabs[2] = {
    "MIDI CHANNELS & FILTERS",
    "SMK25 CC MAPPING & LEARN"
};

struct MidiLearnBtnCtx {
    IUiBackend*          backend = nullptr;
    UiNavigationManager* nav     = nullptr;
    uint8_t              slot    = 0;
    int                  action  = 0; // 0=ToggleLearn, 1=DecCC, 2=IncCC
};

struct LiveMidiAction {
    IUiBackend* backend;
    IUiBackend::MidiAction action;
    unsigned knob;
};

void liveMidiButton(lv_obj_t* parent,IUiBackend& backend,const char* text,
                    int x,int y,int width,IUiBackend::MidiAction action,unsigned knob=0) {
    auto* button=createActionButton(parent,text,x,y,width,34,nullptr,nullptr);
    if(button) attachLvContext<LiveMidiAction>(button,{&backend,action,knob},[](lv_event_t* event){
        auto* context=static_cast<LiveMidiAction*>(lv_event_get_user_data(event));
        context->backend->midiPerformanceAction(context->action,context->knob);
    },LV_EVENT_CLICKED);
}

struct MidiLabelText { char text[114]{}; };

void refreshMidiLabel(lv_obj_t* label,const char* text) {
    auto* data=static_cast<MidiLabelText*>(lv_obj_get_user_data(label));
    if(!data || std::strcmp(data->text,text)==0) return;
    std::snprintf(data->text,sizeof(data->text),"%s",text);
    lv_label_set_text_static(label,data->text);
}

void prepareMidiLabel(lv_obj_t* label) {
    if(label) lv_obj_set_user_data(label,attachLvContext<MidiLabelText>(label,{},nullptr));
}

class MidiScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::MidiSettings; }

    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) override {
        const uint8_t tab = nav.subTab(ScreenId::MidiSettings);
        if (tab >= 2 && tab < 10 && backend.hasMidiPerformance())
            return buildDestination(parent, backend, nav, tab - 2);
        bank_=notice_=profile_=observation_=binding_=nullptr;
        for(auto& label:pads_) label=nullptr;
        for(auto& label:knobs_) label=nullptr;

        // 1. Sub-tab bar (y = 4..40)
        createSubTabBar(parent, kMidiTabs, 2, tab, ScreenId::MidiSettings, nav, 8, 4, 540, 36);

        lv_obj_t* dev_card = lv_obj_create(parent);
        applyPanelStyle(dev_card, true);
        lv_obj_set_pos(dev_card, 556, 4);
        lv_obj_set_size(dev_card, 236, 36);
        lv_obj_set_style_pad_all(dev_card, 4, 0);
        usb_label_=createLabel(dev_card, backend.isDemoMode() ? "USB: SMK25 V2 [DEMO]" : backend.model().diagnostics().usb_status, 6, 4, 220,
                    &lv_font_montserrat_14, palette::kAccentPrimary);

        if(tab==10 && backend.hasMidiPerformance()){
            using Action=IUiBackend::MidiAction;
            liveMidiButton(parent,backend,"RESTORE SMK25 DEFAULTS",8,44,240,Action::RestoreLegacy);
            liveMidiButton(parent,backend,"LOAD SAVED PROFILE",256,44,196,Action::ReloadProfile);
            liveMidiButton(parent,backend,"USE P4 CUSTOM",460,44,160,Action::UseGeneric);
            profile_=createLabel(parent,"",8,82,780);
            notice_=createLabel(parent,"",8,106,780,&lv_font_montserrat_14);
            observation_=createLabel(parent,"",8,126,780,&lv_font_montserrat_14);
            binding_=createLabel(parent,"",8,147,780,&lv_font_montserrat_14);
            for(unsigned i=0;i<16;++i)
                pads_[i]=createLabel(parent,"",8+int(i/8)*396,170+int(i%8)*22,390,&lv_font_montserrat_14);
            createLabel(parent,"CH1 PLAY CC114 RESERVED  |  STOP CC115 USB PANIC  |  REC CC117 RESERVED",
                        8,352,780,&lv_font_montserrat_14);
        } else if (tab == 0) {
            // Row 1: Available P4 MIDI features (y = 46..152)
            createParameterCard(parent, backend, "midi.input_channel",   8,   46, 190, 104);
            createParameterCard(parent, backend, "midi.bend_range_st",   206, 46, 190, 104);
            createParameterCard(parent, backend, "midi.sustain_enable",  404, 46, 190, 104);
            createParameterCard(parent, backend, "midi.modwheel_enable", 602, 46, 190, 104);

            // Row 2: Planned MIDI routing & velocity/clock features (y = 158..264)
            createParameterCard(parent, backend, "midi.vel_curve",  8,   158, 190, 104);
            createParameterCard(parent, backend, "midi.clock_sync", 206, 158, 190, 104);
            createParameterCard(parent, backend, "layer_a.channel", 404, 158, 190, 104,
                                PresentationLevel::BlueprintVerbose, "LAYER A CH");
            createParameterCard(parent, backend, "layer_b.channel", 602, 158, 190, 104,
                                PresentationLevel::BlueprintVerbose, "LAYER B CH");

            // Row 3: USB-MIDI Implementation Status Summary (y = 272..386)
            lv_obj_t* info = lv_obj_create(parent);
            applyPanelStyle(info, false);
            lv_obj_set_pos(info, 8, 272);
            lv_obj_set_size(info, 784, 114);
            createLabel(info, "P4 USB-MIDI HOST IMPLEMENTATION STATUS", 8, 6, 460,
                        &lv_font_montserrat_14, palette::kAccentPrimary);
            createStatusBadge(info, FeatureStatus::Available, 480, 6);
            createLabel(info,
                        "AVAILABLE IN P4: Note On/Off, Vel 0->Off, Sustain CC64, All Notes Off, Pitch Bend, Mod Wheel CC1.",
                        8, 32, 760, &lv_font_montserrat_14, palette::kTextPrimary);
            createLabel(info,
                        backend.hasMidiPerformance() ?
                            "Use the mapping tab for MIDI Learn, controller banks and Save Profile." :
                            "PLANNED: MIDI Learn and controller profiles.",
                        8, 58, 760, &lv_font_montserrat_14, palette::kTextSecondary);
        } else if(backend.hasMidiPerformance()) {
            using Action=IUiBackend::MidiAction;
            bank_=createLabel(parent,backend.midiBankName(),8,54,204,&lv_font_montserrat_14,palette::kAccentPrimary);
            liveMidiButton(parent,backend,"BANK",212,44,90,Action::NextBank);
            liveMidiButton(parent,backend,"CONFIRM",310,44,108,Action::Confirm);
            liveMidiButton(parent,backend,"REPLACE",426,44,108,Action::Replace);
            liveMidiButton(parent,backend,"CANCEL",542,44,108,Action::Cancel);
            liveMidiButton(parent,backend,"SAVE PROFILE",658,44,134,Action::SaveProfile);
            profile_=createLabel(parent,"",8,80,420,&lv_font_montserrat_14,palette::kAccentPrimary);
            auto* details=createActionButton(parent,"PROFILE / PADS / TRANSPORT",490,81,302,28,nullptr,nullptr);
            attachLvContext<UiNavigationManager*>(details,&nav,[](lv_event_t* event){
                auto* navigation=*static_cast<UiNavigationManager**>(lv_event_get_user_data(event));
                navigation->navigateTo(ScreenId::MidiSettings,10);
            },LV_EVENT_CLICKED);
            notice_=createLabel(parent,backend.midiLearnStatus(),8,103,780,&lv_font_montserrat_14,palette::kAccentPrimary);
            observation_=createLabel(parent,"",8,122,780,&lv_font_montserrat_14);
            binding_=createLabel(parent,"",8,140,780,&lv_font_montserrat_14);
            for(unsigned i=0;i<8;++i){
                const int x=8+int(i/4)*396,y=159+int(i%4)*57;
                knobs_[i]=createLabel(parent,"",x,y,388,&lv_font_montserrat_14,palette::kTextPrimary);
                liveMidiButton(parent,backend,"LEARN",x,y+22,98,Action::Learn,i);
                liveMidiButton(parent,backend,"LEARN + CC1",x+106,y+22,144,Action::LearnModWheel,i);
                auto* destination=createActionButton(parent,"DESTINATION",x+258,y+22,128,34,nullptr,nullptr);
                attachLvContext<MidiLearnBtnCtx>(destination,{&backend,&nav,uint8_t(i),0},[](lv_event_t* event){
                    const auto* ctx=static_cast<const MidiLearnBtnCtx*>(lv_event_get_user_data(event));
                    ctx->nav->navigateTo(ScreenId::MidiSettings,uint8_t(2+ctx->slot));
                },LV_EVENT_CLICKED);
            }
        } else if(backend.isDemoMode()) {
            // Tab 1: Data-Driven SMK25 V2 Knob CC Mapping & Demo Learn
            lv_obj_t* table = lv_obj_create(parent);
            applyPanelStyle(table, false);
            lv_obj_set_pos(table, 8, 46);
            lv_obj_set_size(table, 784, 340);

            createLabel(table,
                        "SMK25 V2 CONTROLLER MAPPING PROFILE (PLANNED ON P4 - INTERACTIVE IN UI DEMO)",
                        8, 4, 680, &lv_font_montserrat_14, palette::kWarning);
            createStatusBadge(table, FeatureStatus::Planned, 700, 4);

            auto learn_cb = [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* ctx = static_cast<MidiLearnBtnCtx*>(lv_event_get_user_data(e));
                if (!ctx || !ctx->backend || !ctx->nav) return;
                const auto& m = ctx->backend->model().midiMapping(ctx->slot);
                if (ctx->action == 0) {
                    const int cur = ctx->backend->model().midiLearnSlot();
                    ctx->backend->model().setMidiLearnSlot(cur == ctx->slot ? -1 : ctx->slot);
                    ctx->backend->model().setNotice("DEMO MIDI LEARN: WAITING FOR SIMULATED CC...");
                } else if (ctx->action == 1) {
                    ctx->backend->model().setMidiMappingCc(ctx->slot, (m.cc_number + 127) % 128, m.channel);
                } else if (ctx->action == 2) {
                    ctx->backend->model().setMidiMappingCc(ctx->slot, (m.cc_number + 1) % 128, m.channel);
                }
                ctx->nav->navigateTo(ScreenId::MidiSettings, 1);
            };

            // 2 columns x 4 rows of SMK25 Knob mappings
            for (uint8_t i = 0; i < 8; ++i) {
                const int32_t col = i / 4;
                const int32_t row = i % 4;
                const int32_t cx = 6 + col * 386;
                const int32_t cy = 32 + row * 74;

                lv_obj_t* cell = lv_obj_create(table);
                applyPanelStyle(cell, false);
                lv_obj_set_pos(cell, cx, cy);
                lv_obj_set_size(cell, 378, 68);

                const auto& map = backend.model().midiMapping(i);
                char hdr[64];
                std::snprintf(hdr, sizeof(hdr), "%s -> %s", map.control_label, map.target_param_id);
                createLabel(cell, hdr, 4, 2, 360, &lv_font_montserrat_14, palette::kTextPrimary);

                char cc_txt[32];
                std::snprintf(cc_txt, sizeof(cc_txt), "CH %u  CC #%03u", map.channel, map.cc_number);
                createLabel(cell, cc_txt, 6, 28, 128, &lv_font_montserrat_14, palette::kAccentPrimary);

                lv_obj_t* dec_b = createActionButton(cell, "CC-", 140, 22, 54, 34, nullptr, nullptr);
                lv_obj_t* inc_b = createActionButton(cell, "CC+", 200, 22, 54, 34, nullptr, nullptr);
                lv_obj_t* lrn_b = createActionButton(cell, map.learning ? "WAITING*" : "LEARN",
                                                     260, 22, 104, 34, nullptr, nullptr, map.learning, false);

                if (dec_b) attachLvContext<MidiLearnBtnCtx>(dec_b, MidiLearnBtnCtx{&backend, &nav, i, 1}, learn_cb, LV_EVENT_CLICKED);
                if (inc_b) attachLvContext<MidiLearnBtnCtx>(inc_b, MidiLearnBtnCtx{&backend, &nav, i, 2}, learn_cb, LV_EVENT_CLICKED);
                if (lrn_b) attachLvContext<MidiLearnBtnCtx>(lrn_b, MidiLearnBtnCtx{&backend, &nav, i, 0}, learn_cb, LV_EVENT_CLICKED);
            }
        }

        for(auto* label:{usb_label_,profile_,bank_,notice_,observation_,binding_}) prepareMidiLabel(label);
        for(auto* label:knobs_) prepareMidiLabel(label);
        for(auto* label:pads_) prepareMidiLabel(label);
        return true;
    }

    void refresh(IUiBackend& backend) override {
        if(usb_label_ && !backend.isDemoMode()){
            char status[40]{};
            std::snprintf(status,sizeof(status),"USB MIDI %s",backend.model().diagnostics().usb_status);
            refreshMidiLabel(usb_label_,status);
        }
        if(profile_) refreshMidiLabel(profile_,backend.midiProfileName());
        if(observation_){
            char text[114]{};
            backend.midiObservationText(text,sizeof(text));
            refreshMidiLabel(observation_,text);
        }
        if(binding_){
            char text[110]{};
            backend.midiLastBindingText(text,sizeof(text));
            refreshMidiLabel(binding_,text);
        }
        for(unsigned i=0;i<16;++i) if(pads_[i]){
            char text[80]{};
            backend.midiPadText(i,text,sizeof(text));
            refreshMidiLabel(pads_[i],text);
        }
        if(bank_) refreshMidiLabel(bank_,backend.midiBankName());
        if(notice_) refreshMidiLabel(notice_,backend.midiLearnStatus());
        for(unsigned i=0;i<8;++i) if(knobs_[i]){
            char text[96]{};
            backend.midiKnobText(i,text,sizeof(text));
            refreshMidiLabel(knobs_[i],text);
        }
    }
private:
    struct DestinationPanel {
        IUiBackend* backend;
        UiNavigationManager* nav;
        lv_obj_t* select;
        lv_obj_t* notice;
        unsigned knob;
        unsigned choices[64]{};
    };

    static bool buildDestination(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav, unsigned knob) {
        char text[120];
        std::snprintf(text,sizeof(text),"%s   |   KNOB %u",backend.midiBankName(),knob+1);
        createLabel(parent,text,12,12,750,&lv_font_montserrat_14,palette::kAccentPrimary);
        backend.midiKnobText(knob,text,sizeof(text));
        createLabel(parent,text,12,44,750);
        createLabel(parent,"TAKEOVER: PICKUP",12,72,750);
        auto* drop=lv_dropdown_create(parent);
        lv_obj_set_pos(drop,12,110);
        lv_obj_set_size(drop,560,44);
        lv_obj_set_style_max_height(lv_dropdown_get_list(drop),180,0);
        auto* notice=createLabel(parent,"SELECT A MUSICAL DESTINATION",12,282,760);
        auto* apply=createActionButton(parent,"APPLY DESTINATION",12,322,216,44,nullptr,nullptr);
        auto* back=createActionButton(parent,"CANCEL / BACK",240,322,172,44,nullptr,nullptr);
        DestinationPanel panel{&backend,&nav,drop,notice,knob};
        char options[1900]{};
        size_t used=0;
        unsigned count=0,selected=0;
        for(unsigned id=0;id<256 && count<64;++id){
            const auto* name=backend.midiTargetName(id);
            if(!name || used+std::strlen(name)+2>sizeof(options))continue;
            used+=size_t(std::snprintf(options+used,sizeof(options)-used,"%s%s",count?"\n":"",name));
            if(id==backend.midiCustomTarget(knob))selected=count;
            panel.choices[count++]=id;
        }
        lv_dropdown_set_options(drop,options);
        lv_dropdown_set_selected(drop,selected);
        attachLvContext<DestinationPanel>(apply,panel,[](lv_event_t* event){
            const auto* ctx=static_cast<const DestinationPanel*>(lv_event_get_user_data(event));
            if(ctx->backend->setMidiCustomTarget(ctx->knob,ctx->choices[lv_dropdown_get_selected(ctx->select)]))
                ctx->nav->navigateTo(ScreenId::MidiSettings,1);
            else lv_label_set_text(ctx->notice,"DESTINATION UNAVAILABLE");
        },LV_EVENT_CLICKED);
        attachLvContext<UiNavigationManager*>(back,&nav,[](lv_event_t* event){
            auto* navigation=*static_cast<UiNavigationManager**>(lv_event_get_user_data(event));
            navigation->navigateTo(ScreenId::MidiSettings,1);
        },LV_EVENT_CLICKED);
        return true;
    }

    lv_obj_t* usb_label_=nullptr;
    lv_obj_t* bank_=nullptr;
    lv_obj_t* notice_=nullptr;
    lv_obj_t* knobs_[8]{};
    lv_obj_t* profile_=nullptr;
    lv_obj_t* observation_=nullptr;
    lv_obj_t* binding_=nullptr;
    lv_obj_t* pads_[16]{};
};

} // namespace

ScreenView* createMidiScreen() {
    return new MidiScreen();
}

} // namespace smk::p4::ui
