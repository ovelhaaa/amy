#include "ui_widgets.h"
#include <cstdio>

namespace smk::p4::ui {

namespace {
struct StripCtx {
    IUiBackend*          backend = nullptr;
    UiNavigationManager* nav     = nullptr;
};
} // namespace

bool StatusStrip::build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) {
    if (!parent) return false;
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
    screen_lbl_ = createLabel(root_, "01 PERFORMANCE STAGE", 90, 11, 184, &lv_font_montserrat_14, palette::kTextPrimary);

    // Active Patch pill
    patch_lbl_ = createLabel(root_, "128 DX7 BRASS 1", 278, 11, 156, &lv_font_montserrat_14, palette::kAccentPrimary);

    // MIDI, Audio, Voices indicators
    midi_lbl_   = createLabel(root_, "MIDI: DEMO", 438, 11, 92, &lv_font_montserrat_14, palette::kActive);
    audio_lbl_  = createLabel(root_, "AUD: SIM", 532, 11, 74, &lv_font_montserrat_14, palette::kTextSecondary);
    voices_lbl_ = createLabel(root_, "VX:0/4", 608, 11, 56, &lv_font_montserrat_14, palette::kTextSecondary);

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
        lv_label_set_text_static(patch_lbl_, patch_buf_);
        lv_obj_invalidate(patch_lbl_);
    }
    const auto& diag = backend.model().diagnostics();
    if (midi_lbl_) {
        if (backend.isDemoMode()) {
            lv_label_set_text_static(midi_lbl_, diag.active_voices > 0 ? "MIDI: DEMO*" : "MIDI: DEMO");
        } else if (backend.hasMidiError()) {
            lv_label_set_text_static(midi_lbl_, "MIDI: ERR");
        } else if (backend.isConnected()) {
            lv_label_set_text_static(midi_lbl_, diag.active_voices > 0 ? "MIDI: ON*" : "MIDI: OK");
        } else {
            lv_label_set_text_static(midi_lbl_, "MIDI: WAIT");
        }
        lv_obj_invalidate(midi_lbl_);
    }
    if (audio_lbl_) {
        lv_label_set_text_static(audio_lbl_, backend.isDemoMode() ? "AUD: SIM" : "AUD: 48K");
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
}

} // namespace smk::p4::ui
