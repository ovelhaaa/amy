#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>
#include <cstring>

namespace smk::p4::ui {

namespace {

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

void liveMidiButton(lv_obj_t* parent, IUiBackend& backend, const char* text,
                    int x, int y, int width, IUiBackend::MidiAction action, unsigned knob = 0) {
    auto* button = createActionButton(parent, text, x, y, width, 34, nullptr, nullptr);
    if (button) {
        attachLvContext<LiveMidiAction>(button, {&backend, action, knob}, [](lv_event_t* event) {
            auto* context = static_cast<LiveMidiAction*>(lv_event_get_user_data(event));
            context->backend->midiPerformanceAction(context->action, context->knob);
        }, LV_EVENT_CLICKED);
    }
}

struct MidiLabelText { char text[114]{}; };

void refreshMidiLabel(lv_obj_t* label, const char* text) {
    if (!label || !text) return;
    auto* data = static_cast<MidiLabelText*>(lv_obj_get_user_data(label));
    if (!data || std::strcmp(data->text, text) == 0) return;
    std::snprintf(data->text, sizeof(data->text), "%s", text);
    lv_label_set_text_static(label, data->text);
}

void prepareMidiLabel(lv_obj_t* label) {
    if (label) lv_obj_set_user_data(label, attachLvContext<MidiLabelText>(label, {}, nullptr));
}

class MidiScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::MidiSettings; }

    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) override {
        const uint8_t tab = nav.subTab(ScreenId::MidiSettings);
        if (tab >= 2 && tab < 10 && backend.hasMidiPerformance()) {
            return buildDestination(parent, backend, nav, tab - 2);
        }
        bank_ = notice_ = profile_ = observation_ = binding_ = usb_label_ = nullptr;
        for (auto& label : pads_) label = nullptr;
        for (auto& label : knobs_) label = nullptr;

        // 1. Left Contextual Sidebar (x = 4..160, y = 4..384)
        lv_obj_t* sidebar = lv_obj_create(parent);
        applyPanelStyle(sidebar, false);
        lv_obj_set_pos(sidebar, 4, 4);
        lv_obj_set_size(sidebar, 156, 380);
        lv_obj_set_style_pad_all(sidebar, 4, 0);
        lv_obj_remove_flag(sidebar, LV_OBJ_FLAG_SCROLLABLE);

        createLabel(sidebar, "MIDI / SMK25", 4, 2, 140, &lv_font_montserrat_14, palette::kAccentPrimary);

        usb_label_ = createLabel(
            sidebar,
            backend.isDemoMode() ? "USB: SMK25 [DEMO]" : backend.model().diagnostics().usb_status,
            4, 22, 144, &lv_font_montserrat_14, palette::kActive);
        if (usb_label_) lv_label_set_long_mode(usb_label_, LV_LABEL_LONG_MODE_DOTS);

        struct TabCtx { UiNavigationManager* nav; uint8_t tab; };
        const auto add_tab = [&](const char* lbl, int32_t y, uint8_t t, bool active) {
            lv_obj_t* b = createActionButton(sidebar, lbl, 2, y, 144, 44, nullptr, nullptr, active, false);
            if (b) {
                attachLvContext<TabCtx>(b, {&nav, t}, [](lv_event_t* e) {
                    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                    auto* c = static_cast<TabCtx*>(lv_event_get_user_data(e));
                    if (c && c->nav) c->nav->requestNavigateTo(ScreenId::MidiSettings, c->tab);
                }, LV_EVENT_CLICKED);
            }
        };

        add_tab("CONEXAO",        52,  0, tab == 0);
        add_tab("CONTROLES / CC", 102, 1, tab == 1);
        if (backend.hasMidiPerformance()) {
            add_tab("SMK25 / PERFIL", 152, 10, tab == 10);
        }

        lv_obj_t* pf_btn = createActionButton(sidebar, "TOCAR >", 2, 336, 144, 34, nullptr, nullptr);
        if (pf_btn) {
            attachLvContext<UiNavigationManager*>(pf_btn, &nav, [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* n = *static_cast<UiNavigationManager**>(lv_event_get_user_data(e));
                if (n) n->requestReturnToPerformance();
            }, LV_EVENT_CLICKED);
        }

        // 2. Right Contextual Area (x = 166..794, y = 4..384)
        constexpr int32_t kX = 166;
        if (tab == 10 && backend.hasMidiPerformance()) {
            using Action = IUiBackend::MidiAction;
            liveMidiButton(parent, backend, "RESTORE SMK25 DEFAULTS", kX,       4, 224, Action::RestoreLegacy);
            liveMidiButton(parent, backend, "LOAD SAVED PROFILE",     kX + 230, 4, 200, Action::ReloadProfile);
            liveMidiButton(parent, backend, "USE P4 CUSTOM",          kX + 436, 4, 192, Action::UseGeneric);

            profile_     = createLabel(parent, "", kX, 46,  624, &lv_font_montserrat_14, palette::kAccentPrimary);
            notice_      = createLabel(parent, "", kX, 68,  624, &lv_font_montserrat_14, palette::kTextPrimary);
            observation_ = createLabel(parent, "", kX, 90,  624, &lv_font_montserrat_14, palette::kTextSecondary);
            binding_     = createLabel(parent, "", kX, 112, 624, &lv_font_montserrat_14, palette::kActive);

            for (unsigned i = 0; i < 16; ++i) {
                pads_[i] = createLabel(
                    parent, "",
                    kX + int(i / 8) * 314, 140 + int(i % 8) * 24, 308,
                    &lv_font_montserrat_14, palette::kTextPrimary);
            }
            createLabel(parent,
                        "TRANSPORTE: PLAY CC114 (RES) | STOP CC115 (USB PANIC) | REC CC117 (RES)",
                        kX, 352, 624, &lv_font_montserrat_14, palette::kTextSecondary);
        } else if (tab == 0) {
            // CONEXAO: USB MIDI Host, channels, bend, sustain, clock
            createParameterCard(parent, backend, "midi.input_channel",   kX,       4, 152, 108);
            createParameterCard(parent, backend, "midi.bend_range_st",   kX + 158, 4, 152, 108);
            createParameterCard(parent, backend, "midi.sustain_enable",  kX + 316, 4, 152, 108);
            createParameterCard(parent, backend, "midi.modwheel_enable", kX + 474, 4, 154, 108);

            createParameterCard(parent, backend, "midi.vel_curve",  kX,       120, 152, 108);
            createParameterCard(parent, backend, "midi.clock_sync", kX + 158, 120, 152, 108);
            createParameterCard(parent, backend, "layer_a.channel", kX + 316, 120, 152, 108,
                                PresentationLevel::BlueprintVerbose, "LAYER A CH");
            createParameterCard(parent, backend, "layer_b.channel", kX + 474, 120, 154, 108,
                                PresentationLevel::BlueprintVerbose, "LAYER B CH");

            lv_obj_t* info = lv_obj_create(parent);
            applyPanelStyle(info, false);
            lv_obj_set_pos(info, kX, 236);
            lv_obj_set_size(info, 628, 146);
            createLabel(info, "ESTADO DO HOST USB MIDI (ESP32-P4)", 8, 6, 400,
                        &lv_font_montserrat_14, palette::kAccentPrimary);
            createStatusBadge(info, FeatureStatus::Available, 540, 6);
            createLabel(info,
                        "ATIVO: Note On/Off, Vel 0->Off, Sustain CC64, All Notes Off, Pitch Bend, Mod Wheel CC1.",
                        8, 36, 608, &lv_font_montserrat_14, palette::kTextPrimary);
            createLabel(info,
                        "DESCONEXAO USB: Aciona Panic automatico no Audio Owner sem reboot.",
                        8, 64, 608, &lv_font_montserrat_14, palette::kActive);
            createLabel(info,
                        backend.hasMidiPerformance()
                            ? "Use CONTROLES / CC para MIDI Learn, Pickup e Save Profile."
                            : "PLANNED: MIDI Learn e perfis em modo Demo.",
                        8, 92, 608, &lv_font_montserrat_14, palette::kTextSecondary);
        } else if (backend.hasMidiPerformance()) {
            // CONTROLES / CC + MIDI Learn
            using Action = IUiBackend::MidiAction;
            bank_ = createLabel(parent, backend.midiBankName(), kX, 10, 168, &lv_font_montserrat_14, palette::kAccentPrimary);
            liveMidiButton(parent, backend, "BANK",         kX + 172, 4, 72,  Action::NextBank);
            liveMidiButton(parent, backend, "CONFIRM",      kX + 250, 4, 90,  Action::Confirm);
            liveMidiButton(parent, backend, "REPLACE",      kX + 346, 4, 90,  Action::Replace);
            liveMidiButton(parent, backend, "CANCEL",       kX + 442, 4, 82,  Action::Cancel);
            liveMidiButton(parent, backend, "SAVE PROFILE", kX + 530, 4, 98,  Action::SaveProfile);

            profile_     = createLabel(parent, "", kX, 44, 624, &lv_font_montserrat_14, palette::kAccentPrimary);
            notice_      = createLabel(parent, backend.midiLearnStatus(), kX, 64, 624, &lv_font_montserrat_14, palette::kWarning);
            observation_ = createLabel(parent, "", kX, 84, 624, &lv_font_montserrat_14, palette::kTextSecondary);
            binding_     = createLabel(parent, "", kX, 104, 624, &lv_font_montserrat_14, palette::kActive);

            for (unsigned i = 0; i < 8; ++i) {
                const int x = kX + int(i / 4) * 316;
                const int y = 128 + int(i % 4) * 64;
                knobs_[i] = createLabel(parent, "", x, y, 308, &lv_font_montserrat_14, palette::kTextPrimary);
                liveMidiButton(parent, backend, "LEARN",       x,       y + 22, 76,  Action::Learn, i);
                liveMidiButton(parent, backend, "LEARN + CC1", x + 80,  y + 22, 114, Action::LearnModWheel, i);
                auto* destination = createActionButton(parent, "DESTINATION", x + 198, y + 22, 110, 34, nullptr, nullptr);
                attachLvContext<MidiLearnBtnCtx>(destination, {&backend, &nav, uint8_t(i), 0}, [](lv_event_t* event) {
                    const auto* ctx = static_cast<const MidiLearnBtnCtx*>(lv_event_get_user_data(event));
                    ctx->nav->navigateTo(ScreenId::MidiSettings, uint8_t(2 + ctx->slot));
                }, LV_EVENT_CLICKED);
            }
        } else if (backend.isDemoMode()) {
            lv_obj_t* table = lv_obj_create(parent);
            applyPanelStyle(table, false);
            lv_obj_set_pos(table, kX, 4);
            lv_obj_set_size(table, 628, 380);

            createLabel(table, "SMK25 V2 MAPPING (DEMO RAM - PLANNED NO P4)", 8, 4, 520,
                        &lv_font_montserrat_14, palette::kWarning);
            createStatusBadge(table, FeatureStatus::Planned, 540, 4);

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

            for (uint8_t i = 0; i < 8; ++i) {
                const int32_t col = i / 4;
                const int32_t row = i % 4;
                const int32_t cx = 6 + col * 308;
                const int32_t cy = 34 + row * 82;

                lv_obj_t* cell = lv_obj_create(table);
                applyPanelStyle(cell, false);
                lv_obj_set_pos(cell, cx, cy);
                lv_obj_set_size(cell, 300, 74);

                const auto& map = backend.model().midiMapping(i);
                char hdr[64];
                std::snprintf(hdr, sizeof(hdr), "%s -> %s", map.control_label, map.target_param_id);
                createLabel(cell, hdr, 4, 2, 284, &lv_font_montserrat_14, palette::kTextPrimary);

                char cc_txt[32];
                std::snprintf(cc_txt, sizeof(cc_txt), "CH %u CC #%03u", map.channel, map.cc_number);
                createLabel(cell, cc_txt, 4, 26, 108, &lv_font_montserrat_14, palette::kAccentPrimary);

                lv_obj_t* dec_b = createActionButton(cell, "CC-", 114, 22, 46, 34, nullptr, nullptr);
                lv_obj_t* inc_b = createActionButton(cell, "CC+", 164, 22, 46, 34, nullptr, nullptr);
                lv_obj_t* lrn_b = createActionButton(cell, map.learning ? "WAIT*" : "LEARN",
                                                     214, 22, 76, 34, nullptr, nullptr, map.learning, false);

                if (dec_b) attachLvContext<MidiLearnBtnCtx>(dec_b, MidiLearnBtnCtx{&backend, &nav, i, 1}, learn_cb, LV_EVENT_CLICKED);
                if (inc_b) attachLvContext<MidiLearnBtnCtx>(inc_b, MidiLearnBtnCtx{&backend, &nav, i, 2}, learn_cb, LV_EVENT_CLICKED);
                if (lrn_b) attachLvContext<MidiLearnBtnCtx>(lrn_b, MidiLearnBtnCtx{&backend, &nav, i, 0}, learn_cb, LV_EVENT_CLICKED);
            }
        }

        for (auto* label : {usb_label_, profile_, bank_, notice_, observation_, binding_}) prepareMidiLabel(label);
        for (auto* label : knobs_) prepareMidiLabel(label);
        for (auto* label : pads_) prepareMidiLabel(label);
        refresh(backend);
        return true;
    }

    void refresh(IUiBackend& backend) override {
        if (usb_label_ && !backend.isDemoMode()) {
            char status[40]{};
            std::snprintf(status, sizeof(status), "USB: %s", backend.model().diagnostics().usb_status);
            refreshMidiLabel(usb_label_, status);
        }
        if (profile_) refreshMidiLabel(profile_, backend.midiProfileName());
        if (observation_) {
            char text[114]{};
            backend.midiObservationText(text, sizeof(text));
            refreshMidiLabel(observation_, text);
        }
        if (binding_) {
            char text[110]{};
            backend.midiLastBindingText(text, sizeof(text));
            refreshMidiLabel(binding_, text);
        }
        for (unsigned i = 0; i < 16; ++i) {
            if (pads_[i]) {
                char text[80]{};
                backend.midiPadText(i, text, sizeof(text));
                refreshMidiLabel(pads_[i], text);
            }
        }
        if (bank_) refreshMidiLabel(bank_, backend.midiBankName());
        if (notice_) refreshMidiLabel(notice_, backend.midiLearnStatus());
        for (unsigned i = 0; i < 8; ++i) {
            if (knobs_[i]) {
                char text[96]{};
                backend.midiKnobText(i, text, sizeof(text));
                refreshMidiLabel(knobs_[i], text);
            }
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
        std::snprintf(text, sizeof(text), "%s   |   KNOB %u", backend.midiBankName(), knob + 1);
        createLabel(parent, text, 12, 12, 750, &lv_font_montserrat_14, palette::kAccentPrimary);
        backend.midiKnobText(knob, text, sizeof(text));
        createLabel(parent, text, 12, 44, 750);
        createLabel(parent, "TAKEOVER: PICKUP", 12, 72, 750);
        auto* drop = lv_dropdown_create(parent);
        lv_obj_set_pos(drop, 12, 110);
        lv_obj_set_size(drop, 560, 44);
        lv_obj_set_style_max_height(lv_dropdown_get_list(drop), 180, 0);
        auto* notice = createLabel(parent, "SELECT A MUSICAL DESTINATION", 12, 282, 760);
        auto* apply = createActionButton(parent, "APPLY DESTINATION", 12, 322, 216, 44, nullptr, nullptr);
        auto* back = createActionButton(parent, "CANCEL / BACK", 240, 322, 172, 44, nullptr, nullptr);
        DestinationPanel panel{&backend, &nav, drop, notice, knob};
        char options[1900]{};
        size_t used = 0;
        unsigned count = 0, selected = 0;
        for (unsigned id = 0; id < 256 && count < 64; ++id) {
            const auto* name = backend.midiTargetName(id);
            if (!name || used + std::strlen(name) + 2 > sizeof(options)) continue;
            used += size_t(std::snprintf(options + used, sizeof(options) - used, "%s%s", count ? "\n" : "", name));
            if (id == backend.midiCustomTarget(knob)) selected = count;
            panel.choices[count++] = id;
        }
        lv_dropdown_set_options(drop, options);
        lv_dropdown_set_selected(drop, selected);
        attachLvContext<DestinationPanel>(apply, panel, [](lv_event_t* event) {
            const auto* ctx = static_cast<const DestinationPanel*>(lv_event_get_user_data(event));
            if (ctx->backend->setMidiCustomTarget(ctx->knob, ctx->choices[lv_dropdown_get_selected(ctx->select)])) {
                ctx->nav->navigateTo(ScreenId::MidiSettings, 1);
            } else {
                lv_label_set_text(ctx->notice, "DESTINATION UNAVAILABLE");
            }
        }, LV_EVENT_CLICKED);
        attachLvContext<UiNavigationManager*>(back, &nav, [](lv_event_t* event) {
            auto* navigation = *static_cast<UiNavigationManager**>(lv_event_get_user_data(event));
            navigation->navigateTo(ScreenId::MidiSettings, 1);
        }, LV_EVENT_CLICKED);
        return true;
    }

    lv_obj_t* usb_label_ = nullptr;
    lv_obj_t* bank_ = nullptr;
    lv_obj_t* notice_ = nullptr;
    lv_obj_t* knobs_[8]{};
    lv_obj_t* profile_ = nullptr;
    lv_obj_t* observation_ = nullptr;
    lv_obj_t* binding_ = nullptr;
    lv_obj_t* pads_[16]{};
};

} // namespace

ScreenView* createMidiScreen() {
    return new MidiScreen();
}

} // namespace smk::p4::ui
