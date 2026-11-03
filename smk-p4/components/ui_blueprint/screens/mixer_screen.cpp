#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>

namespace smk::p4::ui {

namespace {

class MixerScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::Mixer; }

    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager&) override {
        // Explicit simulation disclaimer banner (Section 5 Tela 05 requirement)
        lv_obj_t* banner = lv_obj_create(parent);
        applyPanelStyle(banner, false);
        lv_obj_set_pos(banner, 8, 4);
        lv_obj_set_size(banner, 784, 32);
        lv_obj_set_style_pad_all(banner, 4, 0);
        createLabel(banner,
                    "PERFORMANCE MIXER  |  SIGNAL METERS ARE SIMULATED IN UI DEMO (NOT REAL AUDIO DSP LEVELS)",
                    8, 2, 760, &lv_font_montserrat_14, palette::kWarning);

        const auto& m = backend.meters();

        // 4 Mixer Strips: Layer A, Layer B, Drums, Master (each 192px wide)
        // Strip 1: Layer A
        createSignalMeter(parent, "LAYER A", m.layer_a_l, m.layer_a_r, m.is_simulated, 8, 40, 190, 66);
        createParameterCard(parent, backend, "mixer.layer_a.level", 8, 110, 190, 88);
        createParameterCard(parent, backend, "mixer.layer_a.pan",   8, 202, 190, 88);
        createParameterCard(parent, backend, "mixer.layer_a.mute",  8, 294, 190, 92);

        // Strip 2: Layer B
        createSignalMeter(parent, "LAYER B", m.layer_b_l, m.layer_b_r, m.is_simulated, 206, 40, 190, 66);
        createParameterCard(parent, backend, "mixer.layer_b.level", 206, 110, 190, 88);
        createParameterCard(parent, backend, "mixer.layer_b.pan",   206, 202, 190, 88);
        createParameterCard(parent, backend, "mixer.layer_b.mute",  206, 294, 190, 92);

        // Strip 3: Drums Bus
        createSignalMeter(parent, "DRUMS", m.drums_l, m.drums_r, m.is_simulated, 404, 40, 190, 66);
        createParameterCard(parent, backend, "mixer.drums.level", 404, 110, 190, 88);
        createParameterCard(parent, backend, "mixer.fx_send",     404, 202, 190, 88);
        createParameterCard(parent, backend, "mixer.drums.mute",  404, 294, 190, 92);

        // Strip 4: Master Out
        createSignalMeter(parent, "MASTER", m.master_l, m.master_r, m.is_simulated, 602, 40, 190, 66);
        createParameterCard(parent, backend, "perf.master_volume", 602, 110, 190, 88);
        createParameterCard(parent, backend, "fx.drive",           602, 202, 190, 88);
        createParameterCard(parent, backend, "fx.master_tone",     602, 294, 190, 92);

        return true;
    }

    void refresh(IUiBackend&) override {}
};

} // namespace

ScreenView* createMixerScreen() {
    return new MixerScreen();
}

} // namespace smk::p4::ui
