#include "ui_widgets.h"
#include "ui_parameters.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace smk::p4::ui {

namespace {

constexpr const char* kRootNames[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

constexpr const char* kScaleShort[11] = {
    "CHROM", "MAJ", "MIN", "HARM", "P.MAJ", "P.MIN", "BLUES", "DOR", "PHRY", "LYD", "MIXO"
};

struct StripCtx {
    StatusStrip*         strip   = nullptr;
    IUiBackend*          backend = nullptr;
    UiNavigationManager* nav     = nullptr;
};

struct ToggleButtonStyleHelper {
    static lv_obj_t* create(lv_obj_t* parent, const char* static_text, int32_t x, int32_t y,
                            int32_t w, int32_t h, bool active = false, bool accent = false) {
        lv_obj_t* btn = lv_button_create(parent);
        if (!btn) return nullptr;
        lv_obj_remove_style_all(btn);
        lv_obj_add_style(btn, &theme().button, 0);
        lv_obj_add_style(btn, &theme().button_active, LV_STATE_CHECKED);
        lv_obj_add_style(btn, &theme().button_accent, LV_STATE_USER_1);
        lv_obj_set_pos(btn, x, y);
        lv_obj_set_size(btn, w, h);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        setState(btn, active, accent);
        lv_obj_t* lbl = lv_label_create(btn);
        if (lbl) {
            lv_label_set_text_static(lbl, static_text ? static_text : "");
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
            lv_obj_center(lbl);
        }
        return btn;
    }

    static void setState(lv_obj_t* btn, bool active, bool accent) {
        if (!btn) return;
        const bool has_chk = lv_obj_has_state(btn, LV_STATE_CHECKED);
        const bool has_acc = lv_obj_has_state(btn, LV_STATE_USER_1);
        if (active && !has_chk) lv_obj_add_state(btn, LV_STATE_CHECKED);
        else if (!active && has_chk) lv_obj_remove_state(btn, LV_STATE_CHECKED);
        const bool want_acc = !active && accent;
        if (want_acc && !has_acc) lv_obj_add_state(btn, LV_STATE_USER_1);
        else if (!want_acc && has_acc) lv_obj_remove_state(btn, LV_STATE_USER_1);
    }
};

void refreshSlider(lv_obj_t* card) {
    if (!card) return;
    const auto count = lv_obj_get_child_count(card);
    for (uint32_t i = 0; i < count; ++i) {
        lv_obj_send_event(lv_obj_get_child(card, int32_t(i)), LV_EVENT_REFRESH, nullptr);
    }
}

void setStaticTextIfChanged(lv_obj_t* label, const char* static_text) {
    if (!label || !static_text) return;
    const char* cur = lv_label_get_text(label);
    if (!cur || std::strcmp(cur, static_text) != 0) {
        lv_label_set_text_static(label, static_text);
        lv_obj_invalidate(label);
    }
}

void setStaticBufIfChanged(lv_obj_t* label, char* buf, size_t cap, const char* next) {
    if (!label || !buf || cap == 0 || !next) return;
    if (std::strcmp(buf, next) == 0) return;
    std::snprintf(buf, cap, "%s", next);
    lv_label_set_text_static(label, buf);
    lv_obj_invalidate(label);
}

bool harmonyControlAvailable(const IUiBackend& backend) {
    return backend.isDemoMode() || backend.model().live_harmony.available;
}

} // namespace

bool StatusStrip::build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) {
    if (!parent) return false;
    parent_root_ = parent;
    backend_ = &backend;
    nav_ = &nav;
    active_overlay_ = OverlayKind::None;
    overlay_backdrop_ = overlay_card_ = nullptr;
    tempo_label_ = tempo_notice_ = tempo_slider_ = tempo_big_lbl_ = nullptr;
    harm_status_lbl_ = harm_q_btn_ = harm_ch_btn_ = nullptr;

    root_ = lv_obj_create(parent);
    if (!root_) return false;
    applyPanelStyle(root_, false);
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_set_size(root_, kScreenWidth, kStatusStripHeight);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);

    // 1. Mode badge (x=4..86)
    demo_badge_ = lv_obj_create(root_);
    lv_obj_remove_style_all(demo_badge_);
    lv_obj_add_style(demo_badge_, &theme().badge, 0);
    lv_obj_set_style_bg_color(
        demo_badge_,
        lv_color_hex(backend.isDemoMode() ? palette::kAccentPrimary : palette::kActive), 0);
    lv_obj_set_pos(demo_badge_, 4, 4);
    lv_obj_set_size(demo_badge_, 82, 36);
    lv_obj_remove_flag(demo_badge_, LV_OBJ_FLAG_SCROLLABLE);
    if (lv_obj_t* badge_lbl = lv_label_create(demo_badge_)) {
        lv_label_set_text_static(badge_lbl, backend.modeBadgeText());
        lv_obj_set_style_text_font(badge_lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(badge_lbl, lv_color_hex(palette::kBackground), 0);
        lv_obj_center(badge_lbl);
    }

    // 2. Patch + MIDI/Audio status box (x=90..298)
    lv_obj_t* info_box = lv_obj_create(root_);
    if (info_box) {
        applyPanelStyle(info_box, false);
        lv_obj_set_pos(info_box, 90, 4);
        lv_obj_set_size(info_box, 208, 36);
        lv_obj_set_style_pad_all(info_box, 0, 0);
        lv_obj_remove_flag(info_box, LV_OBJ_FLAG_SCROLLABLE);

        std::snprintf(patch_buf_, sizeof(patch_buf_), "001 INIT");
        patch_lbl_ = createLabel(info_box, patch_buf_, 4, 1, 200, &lv_font_montserrat_14, palette::kAccentPrimary);
        if (patch_lbl_) lv_label_set_text_static(patch_lbl_, patch_buf_);
        midi_lbl_ = createLabel(info_box, "MIDI: DEMO", 4, 17, 102, &lv_font_montserrat_14, palette::kActive);
        if (midi_lbl_) lv_label_set_text_static(midi_lbl_, "MIDI: DEMO");
        audio_lbl_ = createLabel(info_box, "AUD: SIM", 110, 17, 94, &lv_font_montserrat_14, palette::kTextSecondary);
        if (audio_lbl_) lv_label_set_text_static(audio_lbl_, "AUD: SIM");
    }

    // 3. Global BPM button (x=302..406) -> opens Tempo overlay
    std::snprintf(tempo_buf_, sizeof(tempo_buf_), "120 BPM INT");
    tempo_btn_ = ToggleButtonStyleHelper::create(root_, tempo_buf_, 302, 4, 104, 36, false, false);
    if (tempo_btn_) {
        tempo_label_ = lv_obj_get_child(tempo_btn_, 0);
        attachLvContext<StripCtx>(
            tempo_btn_, StripCtx{this, &backend, &nav},
            [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* ctx = static_cast<StripCtx*>(lv_event_get_user_data(e));
                if (ctx && ctx->strip && ctx->backend) ctx->strip->toggleTempo(*ctx->backend);
            },
            LV_EVENT_CLICKED);
    }

    // 4. Harmony Root/Scale button (x=410..528) -> opens Harmony overlay
    std::snprintf(harm_buf_, sizeof(harm_buf_), "C MAJ");
    harm_btn_ = ToggleButtonStyleHelper::create(root_, harm_buf_, 410, 4, 118, 36, false, false);
    if (harm_btn_) {
        harm_lbl_ = lv_obj_get_child(harm_btn_, 0);
        attachLvContext<StripCtx>(
            harm_btn_, StripCtx{this, &backend, &nav},
            [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* ctx = static_cast<StripCtx*>(lv_event_get_user_data(e));
                if (ctx && ctx->strip && ctx->backend) ctx->strip->toggleHarmony(*ctx->backend);
            },
            LV_EVENT_CLICKED);
    }

    // 5. Independent Q (Scale Quantize) button (x=532..590)
    std::snprintf(q_buf_, sizeof(q_buf_), "Q OFF");
    q_btn_ = ToggleButtonStyleHelper::create(root_, q_buf_, 532, 4, 58, 36, false, false);
    if (q_btn_) {
        q_lbl_ = lv_obj_get_child(q_btn_, 0);
        attachLvContext<StripCtx>(
            q_btn_, StripCtx{this, &backend, &nav},
            [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* ctx = static_cast<StripCtx*>(lv_event_get_user_data(e));
                if (!ctx || !ctx->backend) return;
                const bool cur = ctx->backend->model().getBool("harmony.scale_enable", false);
                ctx->backend->requestParameterBool("harmony.scale_enable", !cur);
                if (ctx->strip && ctx->nav) ctx->strip->refresh(*ctx->backend, *ctx->nav);
            },
            LV_EVENT_CLICKED);
    }

    // 6. Independent CH (Chord Assist) button (x=594..656)
    std::snprintf(ch_buf_, sizeof(ch_buf_), "CH OFF");
    ch_btn_ = ToggleButtonStyleHelper::create(root_, ch_buf_, 594, 4, 62, 36, false, false);
    if (ch_btn_) {
        ch_lbl_ = lv_obj_get_child(ch_btn_, 0);
        attachLvContext<StripCtx>(
            ch_btn_, StripCtx{this, &backend, &nav},
            [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* ctx = static_cast<StripCtx*>(lv_event_get_user_data(e));
                if (!ctx || !ctx->backend) return;
                const bool cur = ctx->backend->model().getBool("harmony.chord_enable", false);
                ctx->backend->requestParameterBool("harmony.chord_enable", !cur);
                if (ctx->strip && ctx->nav) ctx->strip->refresh(*ctx->backend, *ctx->nav);
            },
            LV_EVENT_CLICKED);
    }

    // 7. Contextual MODS / PERF button (x=660..718)
    mods_btn_ = ToggleButtonStyleHelper::create(root_, "MODS", 660, 4, 58, 36, false, true);
    if (mods_btn_) {
        mods_lbl_ = lv_obj_get_child(mods_btn_, 0);
        attachLvContext<StripCtx>(
            mods_btn_, StripCtx{this, &backend, &nav},
            [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* ctx = static_cast<StripCtx*>(lv_event_get_user_data(e));
                if (!ctx || !ctx->strip || !ctx->nav || !ctx->backend) return;
                const UiArea area = ctx->nav->currentArea();
                if (area == UiArea::Timbres || area == UiArea::Ritmo || area == UiArea::Mais) {
                    ctx->strip->toggleAreaModules(area, *ctx->backend, *ctx->nav);
                } else {
                    ctx->strip->closeOverlays();
                    ctx->nav->requestReturnToPerformance();
                }
            },
            LV_EVENT_CLICKED);
    }

    // 8. Global PANIC button (x=722..796) - ALWAYS accessible above all overlays
    panic_btn_ = lv_button_create(root_);
    if (panic_btn_) {
        applyPanicButtonStyle(panic_btn_);
        lv_obj_set_pos(panic_btn_, 722, 4);
        lv_obj_set_size(panic_btn_, 74, 36);
        if (lv_obj_t* plbl = lv_label_create(panic_btn_)) {
            lv_label_set_text_static(plbl, "PANIC");
            lv_obj_set_style_text_font(plbl, &lv_font_montserrat_14, 0);
            lv_obj_center(plbl);
        }
        attachLvContext<StripCtx>(
            panic_btn_, StripCtx{this, &backend, &nav},
            [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* ctx = static_cast<StripCtx*>(lv_event_get_user_data(e));
                if (ctx && ctx->backend) {
                    ctx->backend->requestPanic();
                    if (ctx->nav) ctx->nav->refreshActive();
                    if (ctx->strip && ctx->nav) ctx->strip->refresh(*ctx->backend, *ctx->nav);
                }
            },
            LV_EVENT_CLICKED);
    }

    refresh(backend, nav);
    return true;
}

void StatusStrip::refresh(IUiBackend& backend, UiNavigationManager& nav) {
    if (!root_) return;
    backend_ = &backend;
    nav_ = &nav;

    const auto& patch = backend.model().activePatch();
    if (patch_lbl_) {
        char next_patch[sizeof(patch_buf_)]{};
        if (!backend.isDemoMode() && !backend.isConnected()) {
            std::snprintf(next_patch, sizeof(next_patch), "PATCH: WAIT OWNER");
        } else {
            std::snprintf(next_patch, sizeof(next_patch), "%03u %s", patch.id, patch.name);
        }
        setStaticBufIfChanged(patch_lbl_, patch_buf_, sizeof(patch_buf_), next_patch);
    }

    const auto& diag = backend.model().diagnostics();
    if (midi_lbl_) {
        const char* midi_txt = nullptr;
        if (backend.isDemoMode()) {
            midi_txt = diag.active_voices > 0 ? "MIDI: DEMO*" : "MIDI: DEMO";
        } else if (backend.hasMidiError()) {
            midi_txt = "MIDI: ERR";
        } else if (diag.midi_connected) {
            midi_txt = diag.active_voices > 0 ? "MIDI: ON*" : "MIDI: OK";
        } else {
            midi_txt = "MIDI: WAIT";
        }
        setStaticTextIfChanged(midi_lbl_, midi_txt);
    }
    if (audio_lbl_) {
        const char* aud_txt = backend.isDemoMode() ? "AUD: SIM" : diag.audio_live ? "AUD: RUN" : "AUD: OFF";
        setStaticTextIfChanged(audio_lbl_, aud_txt);
    }

    const bool harm_avail = harmonyControlAvailable(backend);
    const auto& lh = backend.model().live_harmony;
    const bool q_on = backend.model().getBool("harmony.scale_enable", lh.configuration.scale_on);
    const bool ch_on = backend.model().getBool("harmony.chord_enable", lh.configuration.chord_on);
    const int32_t root_i = std::clamp<int32_t>(backend.model().getInt("harmony.root_note", lh.configuration.root), 0, 11);
    const int32_t scale_i = std::clamp<int32_t>(backend.model().getInt("harmony.scale_type", lh.configuration.scale), 0, 10);

    const bool pending = !backend.isDemoMode() && lh.pending;
    const char* last_id = backend.lastCommandParameterId();
    const bool is_harm_cmd = (last_id != nullptr) && (std::strncmp(last_id, "harmony.", 8) == 0);
    const bool rejected = !lh.pending && is_harm_cmd &&
                          (backend.lastCommandState() == UiCommandState::Rejected ||
                           backend.lastCommandState() == UiCommandState::Unsupported);

    if (harm_lbl_) {
        char hbuf[24]{};
        if (!harm_avail) {
            std::snprintf(hbuf, sizeof(hbuf), "HARM N/A");
        } else if (pending) {
            std::snprintf(hbuf, sizeof(hbuf), "%s %s*", kRootNames[root_i], kScaleShort[scale_i]);
        } else {
            std::snprintf(hbuf, sizeof(hbuf), "%s %s", kRootNames[root_i], kScaleShort[scale_i]);
        }
        setStaticBufIfChanged(harm_lbl_, harm_buf_, sizeof(harm_buf_), hbuf);
        ToggleButtonStyleHelper::setState(harm_btn_, active_overlay_ == OverlayKind::Harmony, pending);
    }

    if (q_lbl_) {
        const char* qtxt = !harm_avail ? "Q N/A" : pending ? "Q WAIT" : rejected ? "Q REJ" : (q_on ? "Q ON" : "Q OFF");
        setStaticBufIfChanged(q_lbl_, q_buf_, sizeof(q_buf_), qtxt);
        ToggleButtonStyleHelper::setState(q_btn_, harm_avail && q_on, pending || rejected);
    }

    if (ch_lbl_) {
        const char* chtxt = !harm_avail ? "CH N/A" : pending ? "CH WAIT" : rejected ? "CH REJ" : (ch_on ? "CH ON" : "CH OFF");
        setStaticBufIfChanged(ch_lbl_, ch_buf_, sizeof(ch_buf_), chtxt);
        ToggleButtonStyleHelper::setState(ch_btn_, harm_avail && ch_on, pending || rejected);
    }

    if (mods_btn_ && mods_lbl_) {
        const UiArea area = nav.currentArea();
        const bool has_mods = (area == UiArea::Timbres || area == UiArea::Ritmo || area == UiArea::Mais);
        const char* mtxt = has_mods ? "MODS" : "TOCAR";
        setStaticTextIfChanged(mods_lbl_, mtxt);
        const bool is_perf = (nav.currentScreen() == ScreenId::Performance);
        const bool want_dis = (!has_mods && is_perf);
        const bool is_dis = lv_obj_has_state(mods_btn_, LV_STATE_DISABLED);
        if (want_dis && !is_dis) lv_obj_add_state(mods_btn_, LV_STATE_DISABLED);
        else if (!want_dis && is_dis) lv_obj_remove_state(mods_btn_, LV_STATE_DISABLED);
        ToggleButtonStyleHelper::setState(
            mods_btn_,
            active_overlay_ == OverlayKind::AreaModules,
            has_mods);
    }

    if (active_overlay_ == OverlayKind::Tempo && overlay_backdrop_) {
        if (tempo_slider_) refreshSlider(tempo_slider_);
        if (tempo_big_lbl_) {
            char bbuf[16]{};
            std::snprintf(bbuf, sizeof(bbuf), "%d BPM", int(backend.model().getInt("perf.bpm", 120)));
            setStaticBufIfChanged(tempo_big_lbl_, bpm_big_buf_, sizeof(bpm_big_buf_), bbuf);
        }
    } else if (active_overlay_ == OverlayKind::Harmony && overlay_backdrop_) {
        ToggleButtonStyleHelper::setState(harm_q_btn_, harm_avail && q_on, pending);
        ToggleButtonStyleHelper::setState(harm_ch_btn_, harm_avail && ch_on, pending);
        if (harm_q_btn_) setStaticTextIfChanged(lv_obj_get_child(harm_q_btn_, 0), q_on ? "Q QUANT: ON" : "Q QUANT: OFF");
        if (harm_ch_btn_) setStaticTextIfChanged(lv_obj_get_child(harm_ch_btn_, 0), ch_on ? "CH ACORDE: ON" : "CH ACORDE: OFF");
        if (harm_status_lbl_) {
            const char* st = !harm_avail ? "INDISPONIVEL" : pending ? "PENDING" : rejected ? "REJEITADO" : "CONFIRMADO";
            const int32_t usb_ch = backend.model().getInt("harmony.usb_channel", lh.configuration.channel + 1);
            const bool touch_on = backend.model().getBool("harmony.apply_touch", lh.configuration.touch);
            char sbuf[128]{};
            std::snprintf(sbuf, sizeof(sbuf),
                          "Estado: %s | Escopo Q/CH: USB Ch %d + Arp + Touch(%s) | SEQ usa SEQ QUANT (CH=OFF)",
                          st, int(usb_ch), touch_on ? "ON" : "OFF");
            setStaticBufIfChanged(harm_status_lbl_, harm_status_buf_, sizeof(harm_status_buf_), sbuf);
        }
    }

    refreshTempo(backend);
}

void StatusStrip::refreshTempo(IUiBackend& backend) {
    if (tempo_label_) {
        const auto* status = backend.arpStatus();
        const bool pulse = status && status->tempo.tick() % tempo::kPpqn < 3;
        char text[24]{};
        if (status || backend.isDemoMode()) {
            std::snprintf(text, sizeof(text), "%c%d BPM INT",
                          pulse ? '*' : ' ', int(backend.model().getInt("perf.bpm", 120)));
        } else {
            std::snprintf(text, sizeof(text), "TEMPO --");
        }
        setStaticBufIfChanged(tempo_label_, tempo_buf_, sizeof(tempo_buf_), text);
    }
    if (tempo_notice_ && active_overlay_ == OverlayKind::Tempo && overlay_backdrop_) {
        const char* text = backend.tempoSaveNotice();
        if (!text[0] && backend.arpNotice()) text = backend.arpNotice();
        if (std::strcmp(lv_label_get_text(tempo_notice_), text) != 0) {
            lv_label_set_text(tempo_notice_, text);
        }
    }
}

void StatusStrip::closeOverlays() {
    if (overlay_backdrop_) {
        lv_obj_delete(overlay_backdrop_);
        overlay_backdrop_ = nullptr;
        overlay_card_ = nullptr;
    }
    tempo_notice_ = nullptr;
    tempo_slider_ = nullptr;
    tempo_big_lbl_ = nullptr;
    harm_status_lbl_ = nullptr;
    harm_q_btn_ = nullptr;
    harm_ch_btn_ = nullptr;
    active_overlay_ = OverlayKind::None;
    ToggleButtonStyleHelper::setState(tempo_btn_, false, false);
    ToggleButtonStyleHelper::setState(harm_btn_, false, false);
}

void StatusStrip::showOverlayBackdrop() {
    if (overlay_backdrop_) {
        lv_obj_delete(overlay_backdrop_);
        overlay_backdrop_ = nullptr;
        overlay_card_ = nullptr;
    }
    if (!parent_root_) return;

    // Cover ONLY content area (y = 44..431) so Header + PANIC and Footer stay 100% accessible!
    overlay_backdrop_ = lv_obj_create(parent_root_);
    lv_obj_remove_style_all(overlay_backdrop_);
    lv_obj_set_pos(overlay_backdrop_, 0, kContentTop);
    lv_obj_set_size(overlay_backdrop_, kScreenWidth, kContentHeight);
    lv_obj_set_style_bg_opa(overlay_backdrop_, LV_OPA_70, 0);
    lv_obj_set_style_bg_color(overlay_backdrop_, lv_color_hex(0x05090D), 0);
    lv_obj_remove_flag(overlay_backdrop_, LV_OBJ_FLAG_SCROLLABLE);

    overlay_card_ = lv_obj_create(overlay_backdrop_);
    applyPanelStyle(overlay_card_, true);
    lv_obj_set_pos(overlay_card_, 24, 10);
    lv_obj_set_size(overlay_card_, kScreenWidth - 48, kContentHeight - 20);
    lv_obj_set_style_pad_all(overlay_card_, 6, 0);
    lv_obj_remove_flag(overlay_card_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* close_b = createActionButton(overlay_card_, "FECHAR", kScreenWidth - 48 - 118, 6, 108, 36,
                                           nullptr, nullptr, false, true);
    attachLvContext<StripCtx>(
        close_b, StripCtx{this, backend_, nav_},
        [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* ctx = static_cast<StripCtx*>(lv_event_get_user_data(e));
            if (!ctx || !ctx->strip) return;
            StatusStrip* strip = ctx->strip;
            IUiBackend* backend = ctx->backend;
            UiNavigationManager* nav = ctx->nav;
            strip->closeOverlays();
            if (backend && nav) strip->refresh(*backend, *nav);
        },
        LV_EVENT_CLICKED);
}

void StatusStrip::openTempo(IUiBackend& backend) {
    backend_ = &backend;
    if (!backend.arpStatus() && !backend.isDemoMode()) return;
    active_overlay_ = OverlayKind::Tempo;
    showOverlayBackdrop();
    if (!overlay_card_) return;

    createLabel(overlay_card_, "GLOBAL TEMPO  /  INTERNAL", 10, 12, 480, &lv_font_montserrat_14, palette::kAccentPrimary);

    std::snprintf(bpm_big_buf_, sizeof(bpm_big_buf_), "%d BPM", int(backend.model().getInt("perf.bpm", 120)));
    tempo_big_lbl_ = createLabel(overlay_card_, bpm_big_buf_, 16, 48, 180, &lv_font_montserrat_28, palette::kTextPrimary);
    if (tempo_big_lbl_) lv_label_set_text_static(tempo_big_lbl_, bpm_big_buf_);

    tempo_slider_ = createParameterCard(overlay_card_, backend, "perf.bpm", 210, 44, 516, 78, PresentationLevel::StageStacked);

    struct TempoControl { StatusStrip* strip; IUiBackend* backend; int action; };
    const auto add_btn = [&](const char* label, int32_t x, int32_t y, int32_t w, int action, bool accent = false) {
        lv_obj_t* obj = createActionButton(overlay_card_, label, x, y, w, 42, nullptr, nullptr, accent, accent);
        if (!obj) return;
        attachLvContext<TempoControl>(obj, {this, &backend, action}, [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* c = static_cast<TempoControl*>(lv_event_get_user_data(e));
            if (!c || !c->backend || !c->strip) return;
            if (c->action == 0) {
                c->backend->tapTempo(lv_tick_get());
            } else if (c->action == 2) {
                c->backend->requestSaveTempo();
            } else {
                const int bpm = c->backend->model().getInt("perf.bpm", 120);
                c->backend->requestParameterInt("perf.bpm", std::clamp(bpm + c->action, 40, 240));
            }
            if (c->strip->nav_) c->strip->refresh(*c->backend, *c->strip->nav_);
        }, LV_EVENT_CLICKED);
    };

    add_btn("-5 BPM", 16, 134, 108, -5);
    add_btn("-1 BPM", 132, 134, 108, -1);
    add_btn("+1 BPM", 248, 134, 108, 1);
    add_btn("+5 BPM", 364, 134, 108, 5);
    add_btn("TAP", 488, 134, 100, 0, true);
    if (backend.canSaveTempo()) {
        add_btn("SAVE GLOBAL TEMPO", 16, 190, 220, 2, false);
    }

    tempo_notice_ = createLabel(overlay_card_, "", 16, 246, 700, &lv_font_montserrat_14, palette::kTextSecondary);
    createLabel(overlay_card_,
                "Relogio global unico para Arp, Sequencer e Delay SYNC. PANIC no topo permanece ativo.",
                16, 276, 700, &lv_font_montserrat_14, palette::kTextSecondary);
    if (nav_) refresh(backend, *nav_);
}

void StatusStrip::openHarmony(IUiBackend& backend) {
    backend_ = &backend;
    active_overlay_ = OverlayKind::Harmony;
    showOverlayBackdrop();
    if (!overlay_card_) return;

    createLabel(overlay_card_, "HARMONIA > ESCALA (Q), ACORDE (CH) E ESCOPO", 10, 12, 580,
                &lv_font_montserrat_14, palette::kAccentPrimary);

    const auto& hc = backend.model().live_harmony.configuration;
    const bool q_on = backend.model().getBool("harmony.scale_enable", hc.scale_on);
    const bool ch_on = backend.model().getBool("harmony.chord_enable", hc.chord_on);
    const bool touch_on = backend.model().getBool("harmony.apply_touch", hc.touch);

    harm_q_btn_ = ToggleButtonStyleHelper::create(overlay_card_, q_on ? "Q QUANT: ON" : "Q QUANT: OFF",
                                                  12, 48, 168, 40, q_on, q_on);
    harm_ch_btn_ = ToggleButtonStyleHelper::create(overlay_card_, ch_on ? "CH ACORDE: ON" : "CH ACORDE: OFF",
                                                   188, 48, 178, 40, ch_on, ch_on);
    lv_obj_t* touch_b = createActionButton(overlay_card_, touch_on ? "TOUCH HARM: ON" : "TOUCH HARM: OFF",
                                           374, 48, 178, 40, nullptr, nullptr, touch_on, false);
    lv_obj_t* preset_b = createActionButton(overlay_card_, "PRESET C MAJ", 560, 48, 172, 40, nullptr, nullptr, false, false);

    struct HarmBtnCtx { StatusStrip* strip; IUiBackend* backend; int kind; const char* param_id; int delta; };
    const auto on_harm_btn = [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
        auto* c = static_cast<HarmBtnCtx*>(lv_event_get_user_data(e));
        if (!c || !c->backend || !c->strip) return;
        StatusStrip* strip = c->strip;
        IUiBackend* backend = c->backend;
        const int kind = c->kind;
        const char* param_id = c->param_id;
        const int delta = c->delta;
        if (kind == 0) {
            const bool cur = backend->model().getBool("harmony.scale_enable", false);
            backend->requestParameterBool("harmony.scale_enable", !cur);
        } else if (kind == 1) {
            const bool cur = backend->model().getBool("harmony.chord_enable", false);
            backend->requestParameterBool("harmony.chord_enable", !cur);
        } else if (kind == 2) {
            const bool cur = backend->model().getBool("harmony.apply_touch", false);
            backend->requestParameterBool("harmony.apply_touch", !cur);
            strip->openHarmony(*backend);
            return;
        } else if (kind == 3) {
            backend->requestParameterInt("harmony.preset", 1);
            strip->openHarmony(*backend);
            return;
        } else if (kind == 4 && param_id) {
            const ParameterDescriptor* d = ParameterRegistry::findById(param_id);
            if (d && d->enum_count > 0) {
                const int32_t cur = backend->model().getInt(param_id, 0);
                const int32_t next = (cur + delta + d->enum_count) % d->enum_count;
                backend->requestParameterInt(param_id, next);
            }
            strip->openHarmony(*backend);
            return;
        }
        if (strip->nav_) strip->refresh(*backend, *strip->nav_);
    };

    attachLvContext<HarmBtnCtx>(harm_q_btn_, {this, &backend, 0, nullptr, 0}, on_harm_btn, LV_EVENT_CLICKED);
    attachLvContext<HarmBtnCtx>(harm_ch_btn_, {this, &backend, 1, nullptr, 0}, on_harm_btn, LV_EVENT_CLICKED);
    attachLvContext<HarmBtnCtx>(touch_b, {this, &backend, 2, nullptr, 0}, on_harm_btn, LV_EVENT_CLICKED);
    attachLvContext<HarmBtnCtx>(preset_b, {this, &backend, 3, nullptr, 0}, on_harm_btn, LV_EVENT_CLICKED);

    struct EnumRow { const char* id; const char* lbl; int32_t val; };
    const EnumRow rows[5] = {
        {"harmony.root_note", "Fundamental", backend.model().getInt("harmony.root_note", hc.root)},
        {"harmony.scale_type", "Escala", backend.model().getInt("harmony.scale_type", hc.scale)},
        {"harmony.quant_mode", "Modo Quant.", backend.model().getInt("harmony.quant_mode", hc.quant)},
        {"harmony.chord_type", "Tipo Acorde", backend.model().getInt("harmony.chord_type", hc.chord)},
        {"harmony.voicing", "Voicing", backend.model().getInt("harmony.voicing", hc.open)}
    };
    for (int r = 0; r < 5; ++r) {
        const int32_t rx = 12 + (r % 2) * 368;
        const int32_t ry = 96 + (r / 2) * 44;
        const ParameterDescriptor* d = ParameterRegistry::findById(rows[r].id);
        const char* vtxt = (d && d->enum_values && rows[r].val >= 0 && rows[r].val < d->enum_count)
                               ? d->enum_values[rows[r].val]
                               : "--";
        char tbuf[48]{};
        std::snprintf(tbuf, sizeof(tbuf), "%s: %s", rows[r].lbl, vtxt);
        lv_obj_t* p = createActionButton(overlay_card_, "<", rx, ry, 42, 38, nullptr, nullptr, false, false);
        lv_obj_t* n = createActionButton(overlay_card_, ">", rx + 314, ry, 42, 38, nullptr, nullptr, false, false);
        createLabel(overlay_card_, tbuf, rx + 50, ry + 9, 260, &lv_font_montserrat_14, palette::kTextPrimary);
        attachLvContext<HarmBtnCtx>(p, {this, &backend, 4, rows[r].id, -1}, on_harm_btn, LV_EVENT_CLICKED);
        attachLvContext<HarmBtnCtx>(n, {this, &backend, 4, rows[r].id, 1}, on_harm_btn, LV_EVENT_CLICKED);
    }

    harm_status_lbl_ = lv_label_create(overlay_card_);
    if (harm_status_lbl_) {
        lv_label_set_long_mode(harm_status_lbl_, LV_LABEL_LONG_WRAP);
        lv_obj_set_pos(harm_status_lbl_, 12, 236);
        lv_obj_set_size(harm_status_lbl_, 720, LV_SIZE_CONTENT);
        lv_obj_set_style_text_font(harm_status_lbl_, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(harm_status_lbl_, lv_color_hex(palette::kTextSecondary), 0);
        lv_label_set_text_static(harm_status_lbl_, "");
    }
    createLabel(overlay_card_,
                "Nota: ADD9/9TH e DROP2/SPREAD/OCT+5 nao sao suportados e sao rejeitados pelo Audio Owner.",
                12, 294, 720, &lv_font_montserrat_14, palette::kWarning);
    if (nav_) refresh(backend, *nav_);
}

void StatusStrip::openAreaModules(UiArea area, IUiBackend& backend, UiNavigationManager& nav) {
    backend_ = &backend;
    nav_ = &nav;
    active_overlay_ = OverlayKind::AreaModules;
    overlay_area_ = area;
    showOverlayBackdrop();
    if (!overlay_card_) return;

    struct ModEntry { const char* title; const char* sub; ScreenId screen; uint8_t sub_tab; bool planned; };
    ModEntry entries[5]{};
    uint8_t count = 0;
    const char* heading = "MODULOS";

    if (area == UiArea::Timbres) {
        heading = "TIMBRES > SELECIONE O MODULO (LARGURA INTEGRAL 800 PX)";
        entries[0] = {"BIBLIOTECA", "Catalogo P4 ROM e User NVS", ScreenId::PatchBrowser, 0, false};
        entries[1] = {"SINTESE", "Editor FM / Subtrativo (7 abas)", ScreenId::SoundEditor, 0, false};
        entries[2] = {"CAMADAS", "Layer A/B e Split [PLANNED]", ScreenId::Layers, 0, true};
        entries[3] = {"MODULACAO", "Matriz, Routing Editor e Legacy", ScreenId::Modulation, 0, false};
        entries[4] = {"MIXER", "Niveis e balanco [PLANNED]", ScreenId::Mixer, 0, true};
        count = 5;
    } else if (area == UiArea::Ritmo) {
        heading = "RITMO > SELECIONE O MODULO (LARGURA INTEGRAL 800 PX)";
        entries[0] = {"SEQUENCIADOR", "16 passos, Play/Edit e Padroes", ScreenId::Sequencer, 0, false};
        entries[1] = {"ARPEJADOR", "Transporte independente e Latch", ScreenId::Arpeggiator, 0, false};
        entries[2] = {"BATERIA", "Drum tracks [PLANNED]", ScreenId::Drums, 0, true};
        count = 3;
    } else {
        heading = "MAIS > SELECIONE O MODULO (LARGURA INTEGRAL 800 PX)";
        entries[0] = {"MIDI / SMK25", "USB Host, mapeamento e Learn", ScreenId::MidiSettings, 0, false};
        entries[1] = {"PREFERENCIAS", "Performance Locks e Config", ScreenId::GlobalSettings, 0, false};
        entries[2] = {"DIAGNOSTICOS", "Heap LVGL, USB, AMY e vozes", ScreenId::Diagnostics, 0, false};
        entries[3] = {"HARMONIA", "Tela completa de visualizacao", ScreenId::Harmony, 0, false};
        count = 4;
    }

    createLabel(overlay_card_, heading, 10, 12, 600, &lv_font_montserrat_14, palette::kAccentPrimary);

    struct ModClick { StatusStrip* strip; UiNavigationManager* nav; ScreenId screen; uint8_t sub_tab; };
    const ScreenId cur = nav.currentScreen();
    for (uint8_t i = 0; i < count; ++i) {
        const int32_t col = i % 2;
        const int32_t row = i / 2;
        const int32_t x = 14 + col * 364;
        const int32_t y = 54 + row * 94;
        const bool is_cur = (entries[i].screen == cur);
        lv_obj_t* card = lv_obj_create(overlay_card_);
        applyPanelStyle(card, is_cur);
        lv_obj_set_pos(card, x, y);
        lv_obj_set_size(card, 352, 84);
        lv_obj_set_style_pad_all(card, 6, 0);
        lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* btn = createActionButton(card, entries[i].title, 4, 4, 154, 40, nullptr, nullptr,
                                           is_cur, !entries[i].planned && is_cur);
        createLabel(card, entries[i].sub, 4, 50, 336, &lv_font_montserrat_14,
                    entries[i].planned ? palette::kWarning : palette::kTextSecondary);
        if (entries[i].planned) {
            createStatusBadge(card, FeatureStatus::Planned, 242, 8);
        } else if (is_cur) {
            createLabel(card, "ATIVO", 268, 12, 70, &lv_font_montserrat_14, palette::kActive);
        }

        attachLvContext<ModClick>(btn, {this, &nav, entries[i].screen, entries[i].sub_tab}, [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* mc = static_cast<ModClick*>(lv_event_get_user_data(e));
            if (!mc || !mc->nav || !mc->strip) return;
            StatusStrip* strip = mc->strip;
            UiNavigationManager* nav = mc->nav;
            const ScreenId screen = mc->screen;
            const uint8_t sub_tab = mc->sub_tab;
            strip->closeOverlays();
            nav->requestNavigateTo(screen, sub_tab);
        }, LV_EVENT_CLICKED);
    }
    refresh(backend, nav);
}

void StatusStrip::toggleAreaModules(UiArea area, IUiBackend& backend, UiNavigationManager& nav) {
    if (active_overlay_ == OverlayKind::AreaModules && overlay_area_ == area) {
        closeOverlays();
        refresh(backend, nav);
        return;
    }
    openAreaModules(area, backend, nav);
}

void StatusStrip::toggleTempo(IUiBackend& backend) {
    if (active_overlay_ == OverlayKind::Tempo) {
        closeOverlays();
        if (nav_) refresh(backend, *nav_);
        return;
    }
    openTempo(backend);
}

void StatusStrip::toggleHarmony(IUiBackend& backend) {
    if (active_overlay_ == OverlayKind::Harmony) {
        closeOverlays();
        if (nav_) refresh(backend, *nav_);
        return;
    }
    openHarmony(backend);
}

} // namespace smk::p4::ui
