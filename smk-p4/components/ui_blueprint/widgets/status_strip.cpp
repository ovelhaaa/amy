#include "ui_widgets.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace smk::p4::ui {

namespace {
struct StripCtx {
    IUiBackend*          backend = nullptr;
    UiNavigationManager* nav     = nullptr;
};
void refreshSlider(lv_obj_t* card) {
    if(!card) return;
    const auto count=lv_obj_get_child_count(card);
    for(uint32_t i=0;i<count;++i) lv_obj_send_event(lv_obj_get_child(card,int32_t(i)),LV_EVENT_REFRESH,nullptr);
}
} // namespace

bool StatusStrip::build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) {
    if (!parent) return false;
    tempo_label_=tempo_panel_=tempo_notice_=tempo_slider_=nullptr;
    root_ = lv_obj_create(parent);
    if (!root_) return false;
    applyPanelStyle(root_, false);
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_set_size(root_, kScreenWidth, kStatusStripHeight);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);

    // Persistent discrete mode badge (mandatory requirement Section 7: UI DEMO always visible in demo mode)
    demo_badge_ = lv_obj_create(root_);
    lv_obj_remove_style_all(demo_badge_);
    lv_obj_add_style(demo_badge_, &theme().badge, 0);
    lv_obj_set_style_bg_color(
        demo_badge_,
        lv_color_hex(backend.isDemoMode() ? palette::kAccentPrimary : palette::kActive), 0);
    lv_obj_set_pos(demo_badge_, 6, 7);
    lv_obj_set_size(demo_badge_, 78, 28);
    lv_obj_t* badge_lbl = lv_label_create(demo_badge_);
    if (badge_lbl) {
        lv_label_set_text_static(badge_lbl, backend.modeBadgeText());
        lv_obj_set_style_text_font(badge_lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(badge_lbl, lv_color_hex(palette::kBackground), 0);
        lv_obj_center(badge_lbl);
    }

    // Screen title & Mode
    screen_lbl_ = createLabel(root_, "PERFORMANCE", 90, 11, 102, &lv_font_montserrat_14, palette::kTextPrimary);

    // Active Patch pill
    patch_lbl_ = createLabel(root_, "128 DX7 BRASS 1", 198, 11, 132, &lv_font_montserrat_14, palette::kAccentPrimary);

    auto* tempo_button=createActionButton(root_,"120 BPM INT",334,3,100,36,nullptr,nullptr);
    if(tempo_button) {
        tempo_label_=lv_obj_get_child(tempo_button,0);
        struct TempoContext { StatusStrip* strip; lv_obj_t* parent; IUiBackend* backend; };
        attachLvContext<TempoContext>(tempo_button,{this,parent,&backend},[](lv_event_t* e) {
            auto* c=static_cast<TempoContext*>(lv_event_get_user_data(e));
            c->strip->toggleTempo(c->parent,*c->backend);
        },LV_EVENT_CLICKED);
    }

    // MIDI, Audio, Voices indicators
    midi_lbl_   = createLabel(root_, "MIDI: DEMO", 438, 11, 92, &lv_font_montserrat_14, palette::kActive);
    audio_lbl_  = createLabel(root_, "AUD: SIM", 532, 11, 74, &lv_font_montserrat_14, palette::kTextSecondary);
    voices_lbl_ = createLabel(root_, "VX:--", 608, 11, 56, &lv_font_montserrat_14, palette::kTextSecondary);

    // Quick return to Performance button (enlarged touch target)
    home_btn_ = createActionButton(root_, "PERF", 666, 3, 58, 36, nullptr, nullptr, false, false);
    attachLvContext<StripCtx>(
        home_btn_, StripCtx{&backend, &nav},
        [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* ctx = static_cast<StripCtx*>(lv_event_get_user_data(e));
            if (ctx && ctx->nav) ctx->nav->returnToPerformance();
        },
        LV_EVENT_CLICKED);

    // Global PANIC button (enlarged touch target)
    panic_btn_ = lv_button_create(root_);
    if (panic_btn_) {
        applyPanicButtonStyle(panic_btn_);
        lv_obj_set_pos(panic_btn_, 728, 3);
        lv_obj_set_size(panic_btn_, 66, 36);
        lv_obj_t* plbl = lv_label_create(panic_btn_);
        if (plbl) {
            lv_label_set_text_static(plbl, "PANIC");
            lv_obj_set_style_text_font(plbl, &lv_font_montserrat_14, 0);
            lv_obj_center(plbl);
        }
        attachLvContext<StripCtx>(
            panic_btn_, StripCtx{&backend, &nav},
            [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* ctx = static_cast<StripCtx*>(lv_event_get_user_data(e));
                if (ctx && ctx->backend) {
                    ctx->backend->requestPanic();
                    if (ctx->nav) ctx->nav->refreshActive();
                }
            },
            LV_EVENT_CLICKED);
    }

    refresh(backend, nav);
    return true;
}

void StatusStrip::refresh(IUiBackend& backend, UiNavigationManager& nav) {
    if (!root_) return;
    if (screen_lbl_) {
        lv_label_set_text_static(screen_lbl_, screenTitle(nav.currentScreen()));
        lv_obj_invalidate(screen_lbl_);
    }
    const auto& patch = backend.model().activePatch();
    if (patch_lbl_) {
        std::snprintf(patch_buf_, sizeof(patch_buf_), "%03u %s", patch.id, patch.name);
        if (!backend.isDemoMode() && !backend.isConnected()) std::snprintf(patch_buf_, sizeof(patch_buf_), "PATCH: WAIT OWNER");
        lv_label_set_text_static(patch_lbl_, patch_buf_);
        lv_obj_invalidate(patch_lbl_);
    }
    const auto& diag = backend.model().diagnostics();
    if (midi_lbl_) {
        if (backend.isDemoMode()) {
            lv_label_set_text_static(midi_lbl_, diag.active_voices > 0 ? "MIDI: DEMO*" : "MIDI: DEMO");
        } else if (backend.hasMidiError()) {
            lv_label_set_text_static(midi_lbl_, "MIDI: ERR");
        } else if (diag.midi_connected) {
            lv_label_set_text_static(midi_lbl_, diag.active_voices > 0 ? "MIDI: ON*" : "MIDI: OK");
        } else {
            lv_label_set_text_static(midi_lbl_, "MIDI: WAIT");
        }
        lv_obj_invalidate(midi_lbl_);
    }
    if (audio_lbl_) {
        lv_label_set_text_static(audio_lbl_, backend.isDemoMode() ? "AUD: SIM" : diag.audio_live ? "AUD: RUN" : "AUD: OFF");
        lv_obj_invalidate(audio_lbl_);
    }
    if (voices_lbl_) {
        std::snprintf(voices_buf_, sizeof(voices_buf_), "VX:%u/%u", diag.active_voices, diag.max_voices);
        lv_label_set_text_static(voices_lbl_, voices_buf_);
        lv_obj_invalidate(voices_lbl_);
    }
    if (home_btn_) {
        if (nav.currentScreen() == ScreenId::Performance) {
            lv_obj_add_state(home_btn_, LV_STATE_DISABLED);
        } else {
            lv_obj_remove_state(home_btn_, LV_STATE_DISABLED);
        }
    }
    if(tempo_panel_ && !lv_obj_has_flag(tempo_panel_,LV_OBJ_FLAG_HIDDEN)) refreshSlider(tempo_slider_);
    refreshTempo(backend);
}

void StatusStrip::closeTempo() {
    if(tempo_panel_) lv_obj_add_flag(tempo_panel_,LV_OBJ_FLAG_HIDDEN);
}

void StatusStrip::refreshTempo(IUiBackend& backend) {
    if(tempo_label_) {
        const auto* status=backend.arpStatus();
        const bool pulse=status && status->tempo.tick()%tempo::kPpqn<3;
        char text[24];
        if(status || backend.isDemoMode())
            std::snprintf(text,sizeof(text),"%c%d BPM INT",pulse?'*':' ',int(backend.model().getInt("perf.bpm",120)));
        else std::snprintf(text,sizeof(text),"TEMPO --");
        if(std::strcmp(lv_label_get_text(tempo_label_),text)) lv_label_set_text(tempo_label_,text);
    }
    if(tempo_notice_ && !lv_obj_has_flag(tempo_panel_,LV_OBJ_FLAG_HIDDEN)) {
        const char* text=backend.tempoSaveNotice();
        if(!text[0] && backend.arpNotice()) text=backend.arpNotice();
        if(std::strcmp(lv_label_get_text(tempo_notice_),text)) lv_label_set_text(tempo_notice_,text);
    }
}

void StatusStrip::toggleTempo(lv_obj_t* parent,IUiBackend& backend) {
    if(!backend.arpStatus() && !backend.isDemoMode()) return;
    if(tempo_panel_) {
        if(lv_obj_has_flag(tempo_panel_,LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_remove_flag(tempo_panel_,LV_OBJ_FLAG_HIDDEN);
            refreshSlider(tempo_slider_);
        }
        else closeTempo();
        return;
    }
    tempo_panel_=lv_obj_create(parent);
    if(!tempo_panel_) return;
    applyPanelStyle(tempo_panel_,true);
    lv_obj_remove_flag(tempo_panel_,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(tempo_panel_,232,kStatusStripHeight+6);
    lv_obj_set_size(tempo_panel_,346,192);
    createLabel(tempo_panel_,"GLOBAL TEMPO  /  INTERNAL",8,4,260,&lv_font_montserrat_14,palette::kTextPrimary);
    tempo_slider_=createParameterCard(tempo_panel_,backend,"perf.bpm",6,29,320,72,PresentationLevel::StageMinimal);
    struct Control { StatusStrip* strip; IUiBackend* backend; int action; };
    const auto button=[&](const char* label,int x,int y,int width,int action) {
        auto* obj=createActionButton(tempo_panel_,label,x,y,width,34,nullptr,nullptr);
        if(!obj) return;
        attachLvContext<Control>(obj,{this,&backend,action},[](lv_event_t* e) {
            auto* c=static_cast<Control*>(lv_event_get_user_data(e));
            if(c->action==0) c->backend->tapTempo(lv_tick_get());
            else if(c->action==2) c->backend->requestSaveTempo();
            else if(c->action==3) c->strip->closeTempo();
            else {
                const int bpm=c->backend->model().getInt("perf.bpm",120);
                c->backend->requestParameterInt("perf.bpm",std::clamp(bpm+c->action,40,240));
            }
        },LV_EVENT_CLICKED);
    };
    button("-",6,110,38,-1);
    button("+",48,110,38,1);
    button("TAP",91,110,72,0);
    if(backend.canSaveTempo()) button("SAVE GLOBAL TEMPO",168,110,158,2);
    button("X",290,0,36,3);
    tempo_notice_=createLabel(tempo_panel_,"",8,153,316,&lv_font_montserrat_14,palette::kTextSecondary);
}

} // namespace smk::p4::ui
