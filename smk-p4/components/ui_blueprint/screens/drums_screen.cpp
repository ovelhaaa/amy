#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>

namespace smk::p4::ui {

namespace {

constexpr const char* kDrumPads[8] = {
    "1 KICK", "2 SNARE", "3 CL HAT", "4 OP HAT",
    "5 LO TOM", "6 MD TOM", "7 CLAP", "8 RIM"
};

struct DrumPadCtx {
    UiDemoBackend*       backend = nullptr;
    UiNavigationManager* nav     = nullptr;
    uint8_t              track   = 0;
};

class DrumsScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::Drums; }

    bool build(lv_obj_t* parent, UiDemoBackend& backend, UiNavigationManager& nav) override {
        const uint8_t cur_trk = static_cast<uint8_t>(backend.model().getInt("drums.track", 0) % 8);

        // 1. 8 Percussion Pad / Track Selector Buttons (y = 4..46)
        for (uint8_t t = 0; t < 8; ++t) {
            const int32_t px = 8 + t * 98;
            lv_obj_t* btn = createActionButton(parent, kDrumPads[t], px, 4, 94, 40,
                                               nullptr, nullptr, t == cur_trk, false);
            if (btn) {
                attachLvContext<DrumPadCtx>(
                    btn, DrumPadCtx{&backend, &nav, t},
                    [](lv_event_t* e) {
                        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                        auto* ctx = static_cast<DrumPadCtx*>(lv_event_get_user_data(e));
                        if (!ctx || !ctx->backend || !ctx->nav) return;
                        ctx->backend->model().setInt("drums.track", ctx->track);
                        ctx->nav->navigateTo(ScreenId::Drums);
                    },
                    LV_EVENT_CLICKED);
            }
        }

        // 2. 16-Step Drum Pattern Grid for selected track (y = 50..182)
        createStepGrid(parent, backend, nav, StepGridKind::DrumTrack, 8, 50, 784, 132);

        // 3. Selected Drum Voice Synthesis / Sample Controls (y = 188..288)
        createParameterCard(parent, backend, "drums.level",    8,   188, 190, 96);
        createParameterCard(parent, backend, "drums.pan",      206, 188, 190, 96);
        createParameterCard(parent, backend, "drums.tune_st",  404, 188, 190, 96);
        createParameterCard(parent, backend, "drums.decay_ms", 602, 188, 190, 96);

        // 4. Drum Bus Master & Status Card (y = 290..386)
        createParameterCard(parent, backend, "mixer.drums.level", 8,   290, 256, 94);
        createParameterCard(parent, backend, "mixer.drums.mute",  272, 290, 190, 94);

        lv_obj_t* info = lv_obj_create(parent);
        applyPanelStyle(info, false);
        lv_obj_set_pos(info, 470, 290);
        lv_obj_set_size(info, 322, 94);
        createLabel(info, "808 PCM DRUM ENGINE", 8, 6, 226,
                    &lv_font_montserrat_14, palette::kAccentPrimary);
        createStatusBadge(info, FeatureStatus::Planned, 250, 6);
        createLabel(info,
                    "808 PCM samples in src/pcm_tiny.h (P4 PLANNED).",
                    8, 34, 300, &lv_font_montserrat_14, palette::kTextSecondary);

        return true;
    }

    void refresh(UiDemoBackend&) override {}
};

} // namespace

ScreenView* createDrumsScreen() {
    return new DrumsScreen();
}

} // namespace smk::p4::ui
