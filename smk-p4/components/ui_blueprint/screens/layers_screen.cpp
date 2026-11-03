#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>

namespace smk::p4::ui {

namespace {

struct LayerModeBtnCtx {
    IUiBackend*          backend = nullptr;
    UiNavigationManager* nav     = nullptr;
    int32_t              mode    = 0;
};

class LayersScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::Layers; }

    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) override {
        // 1. Top Performance Mode Selector (SINGLE A | SINGLE B | DUAL LAYER | SPLIT A/B)
        static constexpr const char* kModes[4] = {
            "SINGLE LAYER A", "SINGLE LAYER B", "DUAL LAYER A+B", "KEYBOARD SPLIT A/B"
        };
        const int32_t cur_mode = backend.model().getInt("perf.layer_mode", 0);
        for (int32_t m = 0; m < 4; ++m) {
            lv_obj_t* btn = createActionButton(parent, kModes[m], 8 + m * 198, 4, 190, 36,
                                               nullptr, nullptr, cur_mode == m, false);
            if (btn) {
                attachLvContext<LayerModeBtnCtx>(
                    btn, LayerModeBtnCtx{&backend, &nav, m},
                    [](lv_event_t* e) {
                        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                        auto* ctx = static_cast<LayerModeBtnCtx*>(lv_event_get_user_data(e));
                        if (!ctx || !ctx->backend || !ctx->nav) return;
                        ctx->backend->requestParameterInt("perf.layer_mode", ctx->mode);
                        ctx->nav->navigateTo(ScreenId::Layers);
                    },
                    LV_EVENT_CLICKED);
            }
        }

        // 2. Layer A Panel (x = 8..396, y = 44..266)
        lv_obj_t* panel_a = lv_obj_create(parent);
        applyPanelStyle(panel_a, cur_mode != 1);
        lv_obj_set_pos(panel_a, 8, 44);
        lv_obj_set_size(panel_a, 388, 222);

        const auto& patch_a = backend.model().layerPatch(0);
        char hdr_a[64];
        std::snprintf(hdr_a, sizeof(hdr_a), "LAYER A (UPPER/MAIN): %03u %s", patch_a.id, patch_a.name);
        createLabel(panel_a, hdr_a, 6, 2, 300, &lv_font_montserrat_14, palette::kAccentPrimary);
        createStatusBadge(panel_a, FeatureStatus::Available, 310, 2);

        createParameterCard(panel_a, backend, "layer_a.enable",    4,   26, 120, 88);
        createParameterCard(panel_a, backend, "layer_a.channel",   130, 26, 120, 88);
        createParameterCard(panel_a, backend, "layer_a.octave",    256, 26, 120, 88);

        createParameterCard(panel_a, backend, "layer_a.transpose", 4,   120, 120, 88);
        createParameterCard(panel_a, backend, "layer_a.key_low",   130, 120, 120, 88);
        createParameterCard(panel_a, backend, "layer_a.key_high",  256, 120, 120, 88);

        // 3. Layer B Panel (x = 404..792, y = 44..266)
        lv_obj_t* panel_b = lv_obj_create(parent);
        applyPanelStyle(panel_b, cur_mode >= 1);
        lv_obj_set_pos(panel_b, 404, 44);
        lv_obj_set_size(panel_b, 388, 222);

        const auto& patch_b = backend.model().layerPatch(1);
        char hdr_b[64];
        std::snprintf(hdr_b, sizeof(hdr_b), "LAYER B (LOWER/AUX): %03u %s", patch_b.id, patch_b.name);
        createLabel(panel_b, hdr_b, 6, 2, 300, &lv_font_montserrat_14, palette::kActive);
        createStatusBadge(panel_b, FeatureStatus::Planned, 310, 2);

        createParameterCard(panel_b, backend, "layer_b.enable",    4,   26, 120, 88);
        createParameterCard(panel_b, backend, "layer_b.channel",   130, 26, 120, 88);
        createParameterCard(panel_b, backend, "layer_b.octave",    256, 26, 120, 88);

        createParameterCard(panel_b, backend, "layer_b.transpose", 4,   120, 120, 88);
        createParameterCard(panel_b, backend, "layer_b.key_low",   130, 120, 120, 88);
        createParameterCard(panel_b, backend, "layer_b.key_high",  256, 120, 120, 88);

        // 4. Visual Keyboard Zone Map (y = 270..388)
        char zone_info[96];
        std::snprintf(zone_info, sizeof(zone_info),
                      "KEYBOARD ZONE VISUALIZER (BLUE=LAYER A [%ld..%ld], GREEN=LAYER B [%ld..%ld], AMBER=OVERLAP)",
                      static_cast<long>(backend.model().getInt("layer_a.key_low", 36)),
                      static_cast<long>(backend.model().getInt("layer_a.key_high", 59)),
                      static_cast<long>(backend.model().getInt("layer_b.key_low", 60)),
                      static_cast<long>(backend.model().getInt("layer_b.key_high", 96)));
        createLabel(parent, zone_info, 10, 270, 780, &lv_font_montserrat_14, palette::kTextSecondary);

        kb_ = createVirtualKeyboard(parent, backend, nav, KeyboardDisplayMode::SplitZones, 8, 290, 784, 98);
        return true;
    }

    void refresh(IUiBackend&) override {
        if (kb_) refreshVirtualKeyboard(kb_);
    }

private:
    lv_obj_t* kb_ = nullptr;
};

} // namespace

ScreenView* createLayersScreen() {
    return new LayersScreen();
}

} // namespace smk::p4::ui
