#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>

namespace smk::p4::ui {

namespace {

class ModulationScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::Modulation; }

    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) override {
        const uint8_t page = nav.subTab(ScreenId::Modulation) % 2;

        // 1. Left Contextual Sidebar (x = 4..160, y = 4..384)
        lv_obj_t* sidebar = lv_obj_create(parent);
        applyPanelStyle(sidebar, false);
        lv_obj_set_pos(sidebar, 4, 4);
        lv_obj_set_size(sidebar, 156, 380);
        lv_obj_set_style_pad_all(sidebar, 4, 0);
        lv_obj_remove_flag(sidebar, LV_OBJ_FLAG_SCROLLABLE);

        createLabel(sidebar, "MODULACAO", 4, 2, 140, &lv_font_montserrat_14, palette::kAccentPrimary);

        struct ModCtx { UiNavigationManager* nav; ScreenId screen; uint8_t tab; };
        const auto add_nav = [&](const char* lbl, int32_t y, ScreenId scr, uint8_t t, bool act, bool acc = false) {
            lv_obj_t* b = createActionButton(sidebar, lbl, 2, y, 144, 42, nullptr, nullptr, act, acc);
            if (b) {
                attachLvContext<ModCtx>(b, {&nav, scr, t}, [](lv_event_t* e) {
                    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                    auto* c = static_cast<ModCtx*>(lv_event_get_user_data(e));
                    if (c && c->nav) c->nav->requestNavigateTo(c->screen, c->tab);
                }, LV_EVENT_CLICKED);
            }
        };

        add_nav("MATRIZ 1-4", 26, ScreenId::Modulation, 0, page == 0);
        add_nav("MATRIZ 5-8", 74, ScreenId::Modulation, 1, page == 1);
        if (backend.hasSoundEditor()) {
            add_nav("ROUTING EDITOR", 128, ScreenId::Performance, 3, false, true);
            add_nav("LEGACY OFFSETS", 176, ScreenId::Performance, 2, false, false);
        }

        lv_obj_t* ed_btn = createActionButton(sidebar, "SINTESE >", 2, 296, 144, 34, nullptr, nullptr);
        lv_obj_t* pf_btn = createActionButton(sidebar, "TOCAR >", 2, 336, 144, 34, nullptr, nullptr);
        if (ed_btn) {
            attachLvContext<UiNavigationManager*>(ed_btn, &nav, [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* n = *static_cast<UiNavigationManager**>(lv_event_get_user_data(e));
                if (n) n->requestNavigateTo(ScreenId::SoundEditor);
            }, LV_EVENT_CLICKED);
        }
        if (pf_btn) {
            attachLvContext<UiNavigationManager*>(pf_btn, &nav, [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* n = *static_cast<UiNavigationManager**>(lv_event_get_user_data(e));
                if (n) n->requestReturnToPerformance();
            }, LV_EVENT_CLICKED);
        }

        // 2. Right Contextual Area (x = 166..794, y = 4..384)
        lv_obj_t* info = lv_obj_create(parent);
        applyPanelStyle(info, false);
        lv_obj_set_pos(info, 166, 4);
        lv_obj_set_size(info, 628, 34);
        lv_obj_set_style_pad_all(info, 4, 0);
        lv_obj_remove_flag(info, LV_OBJ_FLAG_SCROLLABLE);
        if (backend.hasSoundEditor()) {
            createLabel(info, page == 0 ? "MATRIZ DE MODULACAO (SLOTS 1-4) & LFO 1"
                                        : "MATRIZ DE MODULACAO (SLOTS 5-8) & LFO 1",
                        6, 4, 610, &lv_font_montserrat_14, palette::kAccentPrimary);
        } else {
            createLabel(info, "P4 LIVE: CC1 -> 5Hz VIBRATO (AVAIL) | DEMO RAM",
                        6, 4, 610, &lv_font_montserrat_14, palette::kActive);
        }

        // Paginated 4-Slot Modulation Matrix (w = 628, h = 236)
        createModulationMatrixWidget(parent, backend, nav, page, 166, 42, 628, 236);

        // Bottom LFO 1 Quick Strip (y = 284..384)
        createParameterCard(parent, backend, "lfo.1.wave",    166, 284, 152, 98);
        createParameterCard(parent, backend, "lfo.1.rate_hz", 324, 284, 152, 98);
        createParameterCard(parent, backend, "lfo.1.depth",   482, 284, 152, 98);
        createParameterCard(parent, backend, "lfo.1.dest",    640, 284, 154, 98);

        return true;
    }

    void refresh(IUiBackend&) override {}
};

} // namespace

ScreenView* createModulationScreen() {
    return new ModulationScreen();
}

} // namespace smk::p4::ui
