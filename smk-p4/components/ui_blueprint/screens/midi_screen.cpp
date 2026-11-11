#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>
#include <cstring>

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

class MidiScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::MidiSettings; }

    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) override {
        const uint8_t tab = nav.subTab(ScreenId::MidiSettings) % 2;
        bank_=notice_=nullptr;
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

        if (tab == 0) {
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
                        "PLANNED FOR P4: Data-driven MIDI Learn & Controller Profile persistence (implemented in smk-s3).",
                        8, 58, 760, &lv_font_montserrat_14, palette::kTextSecondary);
        } else if(backend.hasMidiPerformance()) {
            using Action=IUiBackend::MidiAction;
            bank_=createLabel(parent,backend.midiBankName(),8,54,204,&lv_font_montserrat_14,palette::kAccentPrimary);
            liveMidiButton(parent,backend,"BANK",212,44,90,Action::NextBank);
            liveMidiButton(parent,backend,"CONFIRM",310,44,108,Action::Confirm);
            liveMidiButton(parent,backend,"REPLACE",426,44,108,Action::Replace);
            liveMidiButton(parent,backend,"CANCEL",542,44,108,Action::Cancel);
            liveMidiButton(parent,backend,"SAVE PROFILE",658,44,134,Action::SaveProfile);
            notice_=createLabel(parent,backend.midiLearnStatus(),8,84,780,&lv_font_montserrat_14,palette::kAccentPrimary);
            for(unsigned i=0;i<8;++i){
                const int x=8+int(i/4)*396,y=112+int(i%4)*68;
                knobs_[i]=createLabel(parent,"",x,y,388,&lv_font_montserrat_14,palette::kTextPrimary);
                liveMidiButton(parent,backend,"LEARN",x,y+25,98,Action::Learn,i);
                liveMidiButton(parent,backend,"LEARN + CC1",x+106,y+25,144,Action::LearnModWheel,i);
            }
            refresh(backend);
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

        return true;
    }

    void refresh(IUiBackend& backend) override {
        if(usb_label_ && !backend.isDemoMode()) lv_label_set_text_fmt(usb_label_,"USB: %s",backend.model().diagnostics().usb_status);
        if(bank_) lv_label_set_text(bank_,backend.midiBankName());
        if(notice_) lv_label_set_text(notice_,backend.midiLearnStatus());
        for(unsigned i=0;i<8;++i) if(knobs_[i]){
            char text[96]{};
            backend.midiKnobText(i,text,sizeof(text));
            if(std::strcmp(lv_label_get_text(knobs_[i]),text)!=0) lv_label_set_text(knobs_[i],text);
        }
    }
private:
    lv_obj_t* usb_label_=nullptr;
    lv_obj_t* bank_=nullptr;
    lv_obj_t* notice_=nullptr;
    lv_obj_t* knobs_[8]{};
};

} // namespace

ScreenView* createMidiScreen() {
    return new MidiScreen();
}

} // namespace smk::p4::ui
