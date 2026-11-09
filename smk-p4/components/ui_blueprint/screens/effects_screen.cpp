#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>

namespace smk::p4::ui {

namespace {

class EffectsScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::Effects; }

    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager&) override {
        // 1. Top FX Chain & AMY Support Classification Banner (y = 4..42)
        lv_obj_t* banner = lv_obj_create(parent);
        applyPanelStyle(banner, true);
        lv_obj_set_pos(banner, 8, 4);
        lv_obj_set_size(banner, 784, 38);
        lv_obj_set_style_pad_all(banner, 4, 0);
        createLabel(banner,
                    "NATIVE AMY FX CHAIN: EQ -> CHORUS -> DELAY -> REVERB  |  HARD REAL-TIME BUS 0",
                    6, 4, 768, &lv_font_montserrat_14, palette::kAccentPrimary);

        // 2. Row 1: Chorus & Delay (y = 46..156)
        createParameterCard(parent, backend, "fx.chorus.mode",    8,   46, 190, 108);
        createParameterCard(parent, backend, "fx.chorus.depth",   206, 46, 190, 108);
        createParameterCard(parent, backend, "fx.delay.time_ms",  404, 46, 190, 108);
        createParameterCard(parent, backend, "fx.delay.feedback", 602, 46, 190, 108);

        // 3. Row 2: Delay Mix, Reverb Size, Reverb Mix, Bus FX Send (y = 160..270)
        createParameterCard(parent, backend, "fx.delay.mix",   8,   160, 190, 108);
        createParameterCard(parent, backend, "fx.reverb.size", 206, 160, 190, 108);
        createParameterCard(parent, backend, "fx.reverb.mix",  404, 160, 190, 108);
        createParameterCard(parent, backend, "mixer.fx_send",  602, 160, 190, 108);

        // 4. Row 3: Bus Saturation Drive, Tilt EQ, Chain Routing Order (y = 274..386)
        createParameterCard(parent, backend, "fx.drive",       8,   274, 256, 112);
        createParameterCard(parent, backend, "fx.master_tone", 272, 274, 256, 112);
        createParameterCard(parent, backend, "fx.routing",     536, 274, 256, 112);

        return true;
    }

    void refresh(IUiBackend&) override {}
};

} // namespace

ScreenView* createEffectsScreen() {
    return new EffectsScreen();
}

} // namespace smk::p4::ui
