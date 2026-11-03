#include "ui_widgets.h"
#include <cstdio>

namespace smk::p4::ui {

namespace {
struct StepCtx {
    UiDemoBackend*       backend = nullptr;
    UiNavigationManager* nav     = nullptr;
    StepGridKind         kind    = StepGridKind::MelodicSequencer;
    uint8_t              step    = 0;
};
} // namespace

lv_obj_t* createStepGrid(lv_obj_t* parent, UiDemoBackend& backend, UiNavigationManager& nav,
                         StepGridKind kind, int32_t x, int32_t y, int32_t w, int32_t h) {
    if (!parent) return nullptr;
    lv_obj_t* panel = lv_obj_create(parent);
    if (!panel) return nullptr;
    applyPanelStyle(panel, false);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_style_pad_all(panel, 4, 0);

    const int32_t inner_w = w - 12;
    const int32_t gap = 3;
    const int32_t step_w = (inner_w - gap * 15) / 16;
    const int32_t step_h = h - 12;

    const uint8_t play_step = backend.activeDemoStep();
    const size_t sel_step = backend.model().selectedSeqStep();
    const size_t drum_track = static_cast<size_t>(backend.model().getInt("drums.track", 0));
    const int32_t seq_len = backend.model().getInt("seq.length", 16);

    for (uint8_t s = 0; s < 16; ++s) {
        const int32_t sx = 2 + s * (step_w + gap);
        bool is_active = false;
        if (kind == StepGridKind::MelodicSequencer) {
            is_active = backend.model().seqStep(s).active;
        } else if (kind == StepGridKind::DrumTrack) {
            is_active = backend.model().drumStep(drum_track, s);
        } else {
            is_active = (s % 2 == 0) || (s % 3 == 0);
        }

        const bool in_len = (s < seq_len);
        const bool is_selected = (kind == StepGridKind::MelodicSequencer && s == sel_step);
        const bool is_playhead = (s == play_step) &&
                                 (backend.model().getBool("seq.playing", false) ||
                                  backend.model().getBool("arp.enable", false));

        lv_obj_t* btn = lv_button_create(panel);
        if (!btn) continue;
        lv_obj_remove_style_all(btn);
        lv_obj_set_pos(btn, sx, 2);
        lv_obj_set_size(btn, step_w, step_h);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);

        uint32_t bg = palette::kBackground;
        if (is_active) {
            bg = (s % 4 == 0) ? palette::kAccentPrimary : palette::kActive;
        } else if (!in_len) {
            bg = palette::kUnavailableBadge;
        } else {
            bg = palette::kPanelHighlight;
        }
        lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);

        const uint32_t bcol = is_playhead ? palette::kTextPrimary
                            : is_selected ? palette::kAccentPrimary
                                          : palette::kBorder;
        lv_obj_set_style_border_width(btn, (is_playhead || is_selected) ? 2 : 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(bcol), 0);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

        char sbuf[16];
        if (kind == StepGridKind::MelodicSequencer) {
            static constexpr const char* kNoteNames[12] = {
                "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
            };
            const uint8_t n = backend.model().seqStep(s).note;
            std::snprintf(sbuf, sizeof(sbuf), "%02u\n%s", unsigned(s + 1), kNoteNames[n % 12]);
        } else {
            std::snprintf(sbuf, sizeof(sbuf), "%02u\n%s", unsigned(s + 1), is_active ? "ON" : "--");
        }

        lv_obj_t* lbl = lv_label_create(btn);
        if (lbl) {
            lv_label_set_text(lbl, sbuf);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
            const uint32_t tcol = is_active ? palette::kBackground : palette::kTextPrimary;
            lv_obj_set_style_text_color(lbl, lv_color_hex(tcol), 0);
            lv_obj_center(lbl);
        }

        attachLvContext<StepCtx>(
            btn, StepCtx{&backend, &nav, kind, s},
            [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* ctx = static_cast<StepCtx*>(lv_event_get_user_data(e));
                if (!ctx || !ctx->backend || !ctx->nav) return;
                if (ctx->kind == StepGridKind::MelodicSequencer) {
                    ctx->backend->model().toggleSeqStep(ctx->step);
                } else if (ctx->kind == StepGridKind::DrumTrack) {
                    const size_t trk = static_cast<size_t>(ctx->backend->model().getInt("drums.track", 0));
                    ctx->backend->model().toggleDrumStep(trk, ctx->step);
                }
                ctx->nav->navigateTo(ctx->nav->currentScreen(), ctx->nav->subTab(ctx->nav->currentScreen()));
            },
            LV_EVENT_CLICKED);
    }

    return panel;
}

} // namespace smk::p4::ui
