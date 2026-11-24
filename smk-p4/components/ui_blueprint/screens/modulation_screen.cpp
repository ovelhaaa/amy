#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>

namespace smk::p4::ui {

namespace {

constexpr const char* kModTabs[2] = {"SLOTS 1-4 (PRIMARY)", "SLOTS 5-8 (AUX)"};

class ModulationScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::Modulation; }

    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) override {
        const uint8_t page = nav.subTab(ScreenId::Modulation) % 2;

        // 1. Page selector & Routing/Legacy routes (y = 4..40)
        createSubTabBar(parent, kModTabs, 2, page, ScreenId::Modulation, nav, 8, 4, 420, 36);

        if (backend.hasSoundEditor()) {
            lv_obj_t* routing = createActionButton(parent, "ROUTING EDITOR", 436, 4, 174, 36, nullptr, nullptr);
            attachLvContext<UiNavigationManager*>(routing, &nav, [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* n = *static_cast<UiNavigationManager**>(lv_event_get_user_data(e));
                if (n) n->navigateTo(ScreenId::Performance, 3);
            }, LV_EVENT_CLICKED);
            lv_obj_t* legacy = createActionButton(parent, "LEGACY OFFSETS", 618, 4, 174, 36, nullptr, nullptr);
            attachLvContext<UiNavigationManager*>(legacy, &nav, [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* n = *static_cast<UiNavigationManager**>(lv_event_get_user_data(e));
                if (n) n->navigateTo(ScreenId::Performance, 2);
            }, LV_EVENT_CLICKED);
        } else {
            lv_obj_t* info = lv_obj_create(parent);
            applyPanelStyle(info, false);
            lv_obj_set_pos(info, 436, 4);
            lv_obj_set_size(info, 356, 36);
            lv_obj_set_style_pad_all(info, 4, 0);
            createLabel(info, "P4 LIVE: CC1 -> 5Hz VIBRATO (AVAIL)", 6, 4, 340,
                        &lv_font_montserrat_14, palette::kActive);
        }

        // 2. Paginated 4-Slot Modulation Matrix (y = 44..294)
        createModulationMatrixWidget(parent, backend, nav, page, 8, 44, 784, 250);

        // 3. Bottom LFO 1 & Expressive Source Quick Strip (y = 298..388)
        createParameterCard(parent, backend, "lfo.1.wave",    8,   298, 190, 88);
        createParameterCard(parent, backend, "lfo.1.rate_hz", 206, 298, 190, 88);
        createParameterCard(parent, backend, "lfo.1.depth",   404, 298, 190, 88);
        createParameterCard(parent, backend, "lfo.1.dest",    602, 298, 190, 88);

        return true;
    }

    void refresh(IUiBackend&) override {}
};

} // namespace

ScreenView* createModulationScreen() {
    return new ModulationScreen();
}

} // namespace smk::p4::ui
