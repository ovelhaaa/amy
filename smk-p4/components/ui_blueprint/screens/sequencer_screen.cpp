#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>

namespace smk::p4::ui {

namespace {

struct SeqActionCtx {
    IUiBackend*          backend = nullptr;
    UiNavigationManager* nav     = nullptr;
    int                  action  = 0; // 0=PlayStop, 1=Clear, 2=DemoSave, 3=DemoLoad
};

class SequencerScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::Sequencer; }

    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) override {
        // 1. Top Transport & Pattern Bar (y = 4..46)
        lv_obj_t* tbar = lv_obj_create(parent);
        applyPanelStyle(tbar, false);
        lv_obj_set_pos(tbar, 8, 4);
        lv_obj_set_size(tbar, 784, 42);
        lv_obj_set_style_pad_all(tbar, 3, 0);

        const bool playing = backend.model().getBool("seq.playing", false);
        lv_obj_t* play_btn = createActionButton(
            tbar, playing ? "STOP DEMO SEQ" : "PLAY DEMO SEQ", 4, 2, 146, 34, nullptr, nullptr, playing, !playing);
        lv_obj_t* clr_btn  = createActionButton(tbar, "RESET STEPS", 156, 2, 118, 34, nullptr, nullptr);
        lv_obj_t* sav_btn  = createActionButton(tbar, "SAVE (RAM)", 280, 2, 110, 34, nullptr, nullptr);
        lv_obj_t* lod_btn  = createActionButton(tbar, "LOAD (RAM)", 396, 2, 110, 34, nullptr, nullptr);

        const size_t sel = backend.model().selectedSeqStep();
        const auto& cell = backend.model().seqStep(sel);
        char sel_txt[64];
        std::snprintf(sel_txt, sizeof(sel_txt), "EDIT STEP %02u/%ld  |  NOTE %u  VEL %u",
                      unsigned(sel + 1),
                      static_cast<long>(backend.model().getInt("seq.length", 16)),
                      cell.note, cell.velocity);
        createLabel(tbar, sel_txt, 516, 8, 256, &lv_font_montserrat_14, palette::kAccentPrimary);

        auto t_cb = [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* ctx = static_cast<SeqActionCtx*>(lv_event_get_user_data(e));
            if (!ctx || !ctx->backend || !ctx->nav) return;
            if (ctx->action == 0) {
                const bool next = !ctx->backend->model().getBool("seq.playing", false);
                ctx->backend->requestParameterBool("seq.playing", next);
                ctx->backend->model().setNotice(next ? "DEMO SEQUENCER RUNNING (VISUAL ONLY)"
                                                     : "DEMO SEQUENCER STOPPED");
            } else if (ctx->action == 1) {
                for (size_t i = 0; i < UiModel::kSeqSteps; ++i) {
                    if (ctx->backend->model().seqStep(i).active) {
                        ctx->backend->model().toggleSeqStep(i);
                    }
                }
                ctx->backend->model().setNotice("DEMO SEQUENCER STEPS CLEARED IN RAM");
            } else if (ctx->action == 2) {
                ctx->backend->model().setNotice("DEMO PATTERN SNAPSHOT KEPT IN RAM (NO NVS WRITE)");
            } else if (ctx->action == 3) {
                ctx->backend->model().resetToDefaults();
                ctx->backend->model().setNotice("DEFAULT DEMO PATTERN RESTORED IN RAM");
            }
            ctx->nav->navigateTo(ScreenId::Sequencer);
        };

        if (play_btn) attachLvContext<SeqActionCtx>(play_btn, SeqActionCtx{&backend, &nav, 0}, t_cb, LV_EVENT_CLICKED);
        if (clr_btn)  attachLvContext<SeqActionCtx>(clr_btn,  SeqActionCtx{&backend, &nav, 1}, t_cb, LV_EVENT_CLICKED);
        if (sav_btn)  attachLvContext<SeqActionCtx>(sav_btn,  SeqActionCtx{&backend, &nav, 2}, t_cb, LV_EVENT_CLICKED);
        if (lod_btn)  attachLvContext<SeqActionCtx>(lod_btn,  SeqActionCtx{&backend, &nav, 3}, t_cb, LV_EVENT_CLICKED);

        // 2. Touch 16-Step Grid (y = 50..186)
        createStepGrid(parent, backend, nav, StepGridKind::MelodicSequencer, 8, 50, 784, 136);

        // 3. Pattern & Step Parameters (y = 192..386)
        createParameterCard(parent, backend, "seq.pattern",      8,   192, 190, 92);
        createParameterCard(parent, backend, "seq.length",       206, 192, 190, 92);
        createParameterCard(parent, backend, "seq.transpose",    404, 192, 190, 92);
        createParameterCard(parent, backend, "perf.bpm",         602, 192, 190, 92);

        createParameterCard(parent, backend, "seq.step_vel",     8,   290, 256, 94);
        createParameterCard(parent, backend, "seq.step_prob",    272, 290, 256, 94);
        createParameterCard(parent, backend, "seq.step_ratchet", 536, 290, 256, 94);

        return true;
    }

    void refresh(IUiBackend&) override {}
};

} // namespace

ScreenView* createSequencerScreen() {
    return new SequencerScreen();
}

} // namespace smk::p4::ui
