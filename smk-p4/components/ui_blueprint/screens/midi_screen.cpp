#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>
#include <cstring>

namespace smk::p4::ui {

namespace {

uint8_t s_selected_knob = 0;

struct MidiLearnBtnCtx {
    IUiBackend*          backend = nullptr;
    UiNavigationManager* nav     = nullptr;
    uint8_t              slot    = 0;
    int                  action  = 0; // 0=ToggleLearn, 1=DecCC, 2=IncCC
};

struct LiveMidiAction {
    IUiBackend* backend = nullptr;
    IUiBackend::MidiAction action = IUiBackend::MidiAction::NextBank;
    unsigned knob = 0;
    bool use_selected_knob = false;
};

void sanitizeAscii(const char* src, char* dst, size_t cap) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 1 < cap; ++i) {
        const unsigned char c = static_cast<unsigned char>(src[i]);
        if (c < 0x80) {
            dst[j++] = static_cast<char>(c);
        } else if (c == 0xE2 && static_cast<unsigned char>(src[i + 1]) == 0x80 &&
                   (static_cast<unsigned char>(src[i + 2]) == 0x94 || static_cast<unsigned char>(src[i + 2]) == 0x93)) {
            dst[j++] = '-';
            i += 2;
        } else {
            // Skip other multi-byte UTF-8 continuation bytes cleanly
            while (src[i + 1] != '\0' && (static_cast<unsigned char>(src[i + 1]) & 0xC0) == 0x80) {
                ++i;
            }
        }
    }
    dst[j] = '\0';
}

lv_obj_t* liveMidiButton(lv_obj_t* parent, IUiBackend& backend, const char* text,
                         int x, int y, int width, int height,
                         IUiBackend::MidiAction action, unsigned knob = 0, bool use_selected = false) {
    auto* button = createActionButton(parent, text, x, y, width, height, nullptr, nullptr);
    if (button) {
        attachLvContext<LiveMidiAction>(button, {&backend, action, knob, use_selected}, [](lv_event_t* event) {
            if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
            auto* context = static_cast<LiveMidiAction*>(lv_event_get_user_data(event));
            if (!context || !context->backend) return;
            const unsigned target_knob = context->use_selected_knob ? unsigned(s_selected_knob) : context->knob;
            context->backend->midiPerformanceAction(context->action, target_knob);
        }, LV_EVENT_CLICKED);
    }
    return button;
}

struct MidiLabelText { char text[128]{}; };

void refreshMidiLabel(lv_obj_t* label, const char* text) {
    if (!label || !text) return;
    char clean[128]{};
    sanitizeAscii(text, clean, sizeof(clean));
    auto* data = static_cast<MidiLabelText*>(lv_obj_get_user_data(label));
    if (!data || std::strcmp(data->text, clean) == 0) return;
    std::snprintf(data->text, sizeof(data->text), "%s", clean);
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
            s_selected_knob = uint8_t(tab - 2);
            return buildDestination(parent, backend, nav, tab - 2);
        }
        bank_ = notice_ = profile_ = observation_ = binding_ = usb_label_ = nullptr;
        usb_host_pill_ = sel_hdr_lbl_ = sel_detail_lbl_ = confirm_lbl_ = replace_lbl_ = nullptr;
        for (auto& label : pads_) label = nullptr;
        for (auto& label : knobs_) label = nullptr;
        for (auto& btn : knob_btns_) btn = nullptr;

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
            4, 20, 140, &lv_font_montserrat_14, palette::kActive);
        if (usb_label_) {
            lv_obj_set_height(usb_label_, 34);
            lv_label_set_long_mode(usb_label_, LV_LABEL_LONG_MODE_WRAP);
        }

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

        add_tab("CONEXAO",        60,  0, tab == 0);
        add_tab("CONTROLES / CC", 110, 1, tab == 1);
        if (backend.hasMidiPerformance()) {
            add_tab("SMK25 / PERFIL", 160, 10, tab == 10);
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
            liveMidiButton(parent, backend, "RESTORE SMK25 DEFAULTS", kX,       4, 224, 34, Action::RestoreLegacy);
            liveMidiButton(parent, backend, "LOAD SAVED PROFILE",     kX + 230, 4, 200, 34, Action::ReloadProfile);
            liveMidiButton(parent, backend, "USE P4 CUSTOM",          kX + 436, 4, 192, 34, Action::UseGeneric);

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
            createLabel(info, "ESTADO DO HOST USB MIDI (ESP32-P4)", 2, 0, 540,
                        &lv_font_montserrat_14, palette::kAccentPrimary);
            createStatusBadge(info, FeatureStatus::Available, 562, 0);
            lv_obj_t* active_lbl = createLabel(
                info,
                "ATIVO: Note On/Off, Vel 0->Off, Sustain CC64, All Notes Off, Pitch Bend, Mod Wheel CC1.",
                2, 24, 610, &lv_font_montserrat_14, palette::kTextPrimary);
            if (active_lbl) {
                lv_obj_set_height(active_lbl, 38);
                lv_label_set_long_mode(active_lbl, LV_LABEL_LONG_MODE_WRAP);
            }
            createLabel(info,
                        "DESCONEXAO USB: Aciona Panic automatico no Audio Owner sem reboot.",
                        2, 68, 610, &lv_font_montserrat_14, palette::kActive);
            createLabel(info,
                        backend.hasMidiPerformance()
                            ? "Use CONTROLES / CC para MIDI Learn, Pickup e Save Profile."
                            : "PLANNED: MIDI Learn e perfis em modo Demo.",
                        2, 94, 610, &lv_font_montserrat_14, palette::kTextSecondary);
        } else if (backend.hasMidiPerformance()) {
            // CONTROLES / CC + MIDI Learn (Two-pane ergonomic layout)
            using Action = IUiBackend::MidiAction;

            // Top bar (y = 4..48, h = 44)
            lv_obj_t* top_bar = lv_obj_create(parent);
            applyPanelStyle(top_bar, false);
            lv_obj_set_pos(top_bar, kX, 4);
            lv_obj_set_size(top_bar, 628, 44);
            lv_obj_set_style_pad_all(top_bar, 4, 0);

            profile_ = createLabel(top_bar, "", 2, 0, 268, &lv_font_montserrat_14, palette::kAccentPrimary);
            if (profile_) lv_label_set_long_mode(profile_, LV_LABEL_LONG_MODE_DOTS);
            bank_ = createLabel(top_bar, backend.midiBankName(), 2, 18, 268, &lv_font_montserrat_14, palette::kTextPrimary);
            if (bank_) lv_label_set_long_mode(bank_, LV_LABEL_LONG_MODE_DOTS);

            liveMidiButton(top_bar, backend, "BANK",         276, 1, 84,  32, Action::NextBank);
            liveMidiButton(top_bar, backend, "SAVE PROFILE", 366, 1, 134, 32, Action::SaveProfile);
            usb_host_pill_ = createLabel(top_bar, "USB: WAIT", 506, 8, 110, &lv_font_montserrat_14, palette::kWarning);
            if (usb_host_pill_) {
                lv_obj_set_style_text_align(usb_host_pill_, LV_TEXT_ALIGN_RIGHT, 0);
                lv_label_set_long_mode(usb_host_pill_, LV_LABEL_LONG_MODE_DOTS);
            }

            // Left column: K1..K8 list (x = 166, y = 52, w = 264, h = 332)
            lv_obj_t* list_panel = lv_obj_create(parent);
            applyPanelStyle(list_panel, false);
            lv_obj_set_pos(list_panel, kX, 52);
            lv_obj_set_size(list_panel, 264, 332);
            lv_obj_set_style_pad_all(list_panel, 4, 0);

            struct SelectKnobCtx { MidiScreen* screen; IUiBackend* backend; uint8_t slot; };
            for (unsigned i = 0; i < 8; ++i) {
                lv_obj_t* kbtn = createActionButton(list_panel, "", 2, 2 + int(i) * 40, 250, 36, nullptr, nullptr, i == s_selected_knob, false);
                knob_btns_[i] = kbtn;
                if (kbtn) {
                    lv_obj_t* klbl = lv_obj_get_child(kbtn, 0);
                    knobs_[i] = klbl;
                    if (klbl) {
                        lv_obj_set_size(klbl, 240, 18);
                        lv_label_set_long_mode(klbl, LV_LABEL_LONG_MODE_DOTS);
                        lv_obj_set_style_text_align(klbl, LV_TEXT_ALIGN_LEFT, 0);
                    }
                    attachLvContext<SelectKnobCtx>(kbtn, {this, &backend, uint8_t(i)}, [](lv_event_t* e) {
                        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                        auto* c = static_cast<SelectKnobCtx*>(lv_event_get_user_data(e));
                        if (!c || !c->screen || !c->backend) return;
                        s_selected_knob = c->slot;
                        c->screen->refresh(*c->backend);
                    }, LV_EVENT_CLICKED);
                }
            }

            // Right column: Contextual panel for selected knob (x = 436, y = 52, w = 358, h = 332)
            lv_obj_t* ctx_panel = lv_obj_create(parent);
            applyPanelStyle(ctx_panel, true);
            lv_obj_set_pos(ctx_panel, kX + 270, 52);
            lv_obj_set_size(ctx_panel, 358, 332);

            sel_hdr_lbl_ = createLabel(ctx_panel, "KNOB K1", 2, 0, 340, &lv_font_montserrat_14, palette::kAccentPrimary);
            sel_detail_lbl_ = createLabel(ctx_panel, "", 2, 22, 340, &lv_font_montserrat_14, palette::kTextPrimary);
            if (sel_detail_lbl_) {
                lv_obj_set_height(sel_detail_lbl_, 40);
                lv_label_set_long_mode(sel_detail_lbl_, LV_LABEL_LONG_MODE_WRAP);
            }

            notice_ = createLabel(ctx_panel, backend.midiLearnStatus(), 2, 66, 340, &lv_font_montserrat_14, palette::kWarning);
            if (notice_) {
                lv_obj_set_height(notice_, 38);
                lv_label_set_long_mode(notice_, LV_LABEL_LONG_MODE_WRAP);
            }

            observation_ = createLabel(ctx_panel, "", 2, 108, 340, &lv_font_montserrat_14, palette::kTextSecondary);
            if (observation_) {
                lv_obj_set_height(observation_, 36);
                lv_label_set_long_mode(observation_, LV_LABEL_LONG_MODE_WRAP);
            }

            binding_ = createLabel(ctx_panel, "", 2, 146, 340, &lv_font_montserrat_14, palette::kActive);
            if (binding_) lv_label_set_long_mode(binding_, LV_LABEL_LONG_MODE_DOTS);

            // Row 1: LEARN, LEARN + CC1, CANCEL
            liveMidiButton(ctx_panel, backend, "LEARN",       2,   178, 106, 38, Action::Learn, 0, true);
            liveMidiButton(ctx_panel, backend, "LEARN + CC1", 112, 178, 122, 38, Action::LearnModWheel, 0, true);
            liveMidiButton(ctx_panel, backend, "CANCEL",      238, 178, 104, 38, Action::Cancel, 0, true);

            // Row 2: CONFIRM Kx, REPLACE Kx
            lv_obj_t* cbtn = liveMidiButton(ctx_panel, backend, "CONFIRM", 2,   224, 166, 38, Action::Confirm, 0, true);
            lv_obj_t* rbtn = liveMidiButton(ctx_panel, backend, "REPLACE", 176, 224, 166, 38, Action::Replace, 0, true);
            confirm_lbl_ = cbtn ? lv_obj_get_child(cbtn, 0) : nullptr;
            replace_lbl_ = rbtn ? lv_obj_get_child(rbtn, 0) : nullptr;

            // Row 3: DESTINATION
            auto* destination = createActionButton(ctx_panel, "DESTINATION", 2, 270, 340, 40, nullptr, nullptr);
            if (destination) {
                attachLvContext<MidiLearnBtnCtx>(destination, {&backend, &nav, 0, 0}, [](lv_event_t* event) {
                    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
                    const auto* ctx = static_cast<const MidiLearnBtnCtx*>(lv_event_get_user_data(event));
                    if (ctx && ctx->nav) {
                        ctx->nav->requestNavigateTo(ScreenId::MidiSettings, uint8_t(2 + s_selected_knob));
                    }
                }, LV_EVENT_CLICKED);
            }
        } else if (backend.isDemoMode()) {
            lv_obj_t* table = lv_obj_create(parent);
            applyPanelStyle(table, false);
            lv_obj_set_pos(table, kX, 4);
            lv_obj_set_size(table, 628, 380);

            createLabel(table, "SMK25 V2 MAPPING (DEMO RAM - PLANNED NO P4)", 2, 0, 540,
                        &lv_font_montserrat_14, palette::kWarning);
            createStatusBadge(table, FeatureStatus::Planned, 562, 0);

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
                ctx->nav->requestNavigateTo(ScreenId::MidiSettings, 1);
            };

            for (uint8_t i = 0; i < 8; ++i) {
                const int32_t col = i / 4;
                const int32_t row = i % 4;
                const int32_t cx = 2 + col * 308;
                const int32_t cy = 30 + row * 82;

                lv_obj_t* cell = lv_obj_create(table);
                applyPanelStyle(cell, false);
                lv_obj_set_pos(cell, cx, cy);
                lv_obj_set_size(cell, 300, 74);

                const auto& map = backend.model().midiMapping(i);
                char hdr[64];
                std::snprintf(hdr, sizeof(hdr), "%s -> %s", map.control_label, map.target_param_id);
                createLabel(cell, hdr, 0, 0, 284, &lv_font_montserrat_14, palette::kTextPrimary);

                char cc_txt[32];
                std::snprintf(cc_txt, sizeof(cc_txt), "CH %u CC #%03u", map.channel, map.cc_number);
                createLabel(cell, cc_txt, 0, 24, 108, &lv_font_montserrat_14, palette::kAccentPrimary);

                lv_obj_t* dec_b = createActionButton(cell, "CC-", 110, 20, 46, 34, nullptr, nullptr);
                lv_obj_t* inc_b = createActionButton(cell, "CC+", 160, 20, 46, 34, nullptr, nullptr);
                lv_obj_t* lrn_b = createActionButton(cell, map.learning ? "WAIT*" : "LEARN",
                                                     210, 20, 76, 34, nullptr, nullptr, map.learning, false);

                if (dec_b) attachLvContext<MidiLearnBtnCtx>(dec_b, MidiLearnBtnCtx{&backend, &nav, i, 1}, learn_cb, LV_EVENT_CLICKED);
                if (inc_b) attachLvContext<MidiLearnBtnCtx>(inc_b, MidiLearnBtnCtx{&backend, &nav, i, 2}, learn_cb, LV_EVENT_CLICKED);
                if (lrn_b) attachLvContext<MidiLearnBtnCtx>(lrn_b, MidiLearnBtnCtx{&backend, &nav, i, 0}, learn_cb, LV_EVENT_CLICKED);
            }
        }

        for (auto* label : {usb_label_, usb_host_pill_, profile_, bank_, notice_, observation_, binding_, sel_hdr_lbl_, sel_detail_lbl_, confirm_lbl_, replace_lbl_}) {
            prepareMidiLabel(label);
        }
        for (auto* label : knobs_) prepareMidiLabel(label);
        for (auto* label : pads_) prepareMidiLabel(label);
        refresh(backend);
        return true;
    }

    void refresh(IUiBackend& backend) override {
        if (usb_label_ && !backend.isDemoMode()) {
            char status[48]{};
            std::snprintf(status, sizeof(status), "USB: %s", backend.model().diagnostics().usb_status);
            refreshMidiLabel(usb_label_, status);
        }
        if (usb_host_pill_) {
            const bool conn = backend.model().diagnostics().midi_connected;
            refreshMidiLabel(usb_host_pill_, conn ? "USB: ONLINE" : "USB: OFFLINE");
            lv_obj_set_style_text_color(usb_host_pill_, lv_color_hex(conn ? palette::kActive : palette::kWarning), 0);
        }
        if (profile_) refreshMidiLabel(profile_, backend.midiProfileName());
        if (observation_) {
            char text[120]{};
            backend.midiObservationText(text, sizeof(text));
            refreshMidiLabel(observation_, text);
        }
        if (binding_) {
            char text[120]{};
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
            if (knob_btns_[i]) {
                if (i == s_selected_knob) lv_obj_add_state(knob_btns_[i], LV_STATE_CHECKED);
                else lv_obj_remove_state(knob_btns_[i], LV_STATE_CHECKED);
            }
            if (knobs_[i]) {
                char text[96]{};
                backend.midiKnobText(i, text, sizeof(text));
                refreshMidiLabel(knobs_[i], text);
                lv_obj_set_style_text_color(knobs_[i],
                                            lv_color_hex(i == s_selected_knob ? palette::kBackground : palette::kTextPrimary), 0);
            }
        }

        if (sel_hdr_lbl_) {
            char hdr[48]{};
            std::snprintf(hdr, sizeof(hdr), "KNOB K%u SELECIONADO", unsigned(s_selected_knob) + 1);
            refreshMidiLabel(sel_hdr_lbl_, hdr);
        }
        if (sel_detail_lbl_) {
            char detail[96]{};
            backend.midiKnobText(s_selected_knob, detail, sizeof(detail));
            refreshMidiLabel(sel_detail_lbl_, detail);
        }
        if (confirm_lbl_) {
            char cbuf[24]{};
            std::snprintf(cbuf, sizeof(cbuf), "CONFIRM K%u", unsigned(s_selected_knob) + 1);
            refreshMidiLabel(confirm_lbl_, cbuf);
        }
        if (replace_lbl_) {
            char rbuf[24]{};
            std::snprintf(rbuf, sizeof(rbuf), "REPLACE K%u", unsigned(s_selected_knob) + 1);
            refreshMidiLabel(replace_lbl_, rbuf);
        }
    }

private:
    struct DestinationPanel {
        IUiBackend* backend = nullptr;
        UiNavigationManager* nav = nullptr;
        lv_obj_t* select = nullptr;
        lv_obj_t* notice = nullptr;
        unsigned knob = 0;
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
            if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
            const auto* ctx = static_cast<const DestinationPanel*>(lv_event_get_user_data(event));
            if (!ctx || !ctx->backend || !ctx->nav || !ctx->select) return;
            if (ctx->backend->setMidiCustomTarget(ctx->knob, ctx->choices[lv_dropdown_get_selected(ctx->select)])) {
                ctx->nav->requestNavigateTo(ScreenId::MidiSettings, 1);
            } else if (ctx->notice) {
                lv_label_set_text(ctx->notice, "DESTINATION UNAVAILABLE");
            }
        }, LV_EVENT_CLICKED);
        attachLvContext<UiNavigationManager*>(back, &nav, [](lv_event_t* event) {
            if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
            auto* navigation = *static_cast<UiNavigationManager**>(lv_event_get_user_data(event));
            if (navigation) navigation->requestNavigateTo(ScreenId::MidiSettings, 1);
        }, LV_EVENT_CLICKED);
        return true;
    }

    lv_obj_t* usb_label_ = nullptr;
    lv_obj_t* usb_host_pill_ = nullptr;
    lv_obj_t* bank_ = nullptr;
    lv_obj_t* notice_ = nullptr;
    lv_obj_t* knob_btns_[8]{};
    lv_obj_t* knobs_[8]{};
    lv_obj_t* profile_ = nullptr;
    lv_obj_t* observation_ = nullptr;
    lv_obj_t* binding_ = nullptr;
    lv_obj_t* sel_hdr_lbl_ = nullptr;
    lv_obj_t* sel_detail_lbl_ = nullptr;
    lv_obj_t* confirm_lbl_ = nullptr;
    lv_obj_t* replace_lbl_ = nullptr;
    lv_obj_t* pads_[16]{};
};

} // namespace

ScreenView* createMidiScreen() {
    return new MidiScreen();
}

} // namespace smk::p4::ui
