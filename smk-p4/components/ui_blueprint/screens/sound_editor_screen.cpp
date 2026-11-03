#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>

namespace smk::p4::ui {

namespace {

constexpr const char* kEditorTabs[7] = {"OSC", "FM", "FILTER", "AMP", "ENV", "LFO", "MOD"};

class SoundEditorScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::SoundEditor; }

    bool build(lv_obj_t* parent, UiDemoBackend& backend, UiNavigationManager& nav) override {
        const uint8_t tab = nav.subTab(ScreenId::SoundEditor) % 7;

        // 1. Sub-tab bar (y = 4..40)
        createSubTabBar(parent, kEditorTabs, 7, tab, ScreenId::SoundEditor, nav, 8, 4, 784, 36);

        // 2. Contextual Patch & Synthesis Family Banner (y = 44..76)
        lv_obj_t* banner = lv_obj_create(parent);
        applyPanelStyle(banner, false);
        lv_obj_set_pos(banner, 8, 44);
        lv_obj_set_size(banner, 784, 32);
        lv_obj_set_style_pad_all(banner, 4, 0);

        const auto& patch = backend.model().activePatch();
        char btxt[96];
        std::snprintf(btxt, sizeof(btxt),
                      "EDITING: %03u %s  |  FAMILY: %s  |  DEMO STATE (NO AMY DSP WRITES)",
                      patch.id, patch.name, synthFamilyName(patch.family));
        createLabel(banner, btxt, 6, 2, 760, &lv_font_montserrat_14, palette::kAccentPrimary);

        // 3. Tab-specific parameter grid (y = 82..386)
        switch (tab) {
            case 0: // OSC
                createParameterCard(parent, backend, "osc.1.wave",        8,   82, 256, 94);
                createParameterCard(parent, backend, "osc.1.pitch_st",    272, 82, 256, 94);
                createParameterCard(parent, backend, "osc.1.detune_ct",   536, 82, 256, 94);

                createParameterCard(parent, backend, "osc.1.level",       8,   184, 256, 94);
                createParameterCard(parent, backend, "osc.1.pan",         272, 184, 256, 94);
                createParameterCard(parent, backend, "osc.1.pulse_width", 536, 184, 256, 94);

                createParameterCard(parent, backend, "osc.1.unison",      8,   286, 256, 94);
                createParameterCard(parent, backend, "osc.sub_level",     272, 286, 256, 94);
                createParameterCard(parent, backend, "osc.noise_level",   536, 286, 256, 94);
                break;

            case 1: // FM
                createParameterCard(parent, backend, "fm.algo",        8,   82, 190, 94);
                createParameterCard(parent, backend, "fm.selected_op", 206, 82, 190, 94);
                createParameterCard(parent, backend, "fm.op.ratio",    404, 82, 190, 94);
                createParameterCard(parent, backend, "fm.op.level",    602, 82, 190, 94);

                createParameterCard(parent, backend, "fm.feedback",    8,   184, 190, 94);
                createParameterCard(parent, backend, "fm.op.attack",   206, 184, 190, 94);
                createParameterCard(parent, backend, "fm.op.decay",    404, 184, 190, 94);
                createParameterCard(parent, backend, "fm.op.vel_sens", 602, 184, 190, 94);

                {
                    lv_obj_t* algo_card = lv_obj_create(parent);
                    applyPanelStyle(algo_card, true);
                    lv_obj_set_pos(algo_card, 8, 286);
                    lv_obj_set_size(algo_card, 784, 94);
                    char atxt[120];
                    std::snprintf(atxt, sizeof(atxt),
                                  "DX7 6-OP ROUTING [ALGO %ld] : [OP6 fb] -> [OP5] -> [OP4] -> [OP3]  +  [OP2] -> [OP1 CARRIER]",
                                  static_cast<long>(backend.model().getInt("fm.algo", 1)));
                    createLabel(algo_card, atxt, 8, 8, 756, &lv_font_montserrat_14, palette::kTextPrimary);
                    createLabel(algo_card,
                                patch.family == SynthFamily::FM
                                    ? "FM FAMILY ACTIVE: Operator controls enabled in UI Demo; runtime AMY uses pre-materialized ROM 128/135."
                                    : "NON-FM PATCH SELECTED: FM operator controls marked UNAVAILABLE for subtractive/PCM patches.",
                                8, 38, 756, &lv_font_montserrat_14, palette::kTextSecondary);
                }
                break;

            case 2: // FILTER
                createParameterCard(parent, backend, "filter.type",       8,   82, 256, 94);
                createParameterCard(parent, backend, "filter.cutoff_hz",  272, 82, 256, 94);
                createParameterCard(parent, backend, "filter.resonance",  536, 82, 256, 94);

                createParameterCard(parent, backend, "filter.drive",      8,   184, 256, 94);
                createParameterCard(parent, backend, "filter.keytrack",   272, 184, 256, 94);
                createParameterCard(parent, backend, "filter.env_amount", 536, 184, 256, 94);

                {
                    lv_obj_t* fcard = lv_obj_create(parent);
                    applyPanelStyle(fcard, false);
                    lv_obj_set_pos(fcard, 8, 286);
                    lv_obj_set_size(fcard, 784, 94);
                    createLabel(fcard, "BIQUAD / ZDF FILTER TOPOLOGY NOTES", 8, 6, 420,
                                &lv_font_montserrat_14, palette::kAccentPrimary);
                    createLabel(fcard,
                                "AMY supports LPF12/BPF/HPF/LPF24 per voice. Real-time filter modulation is PLANNED for P4 standalone.",
                                8, 34, 756, &lv_font_montserrat_14, palette::kTextSecondary);
                }
                break;

            case 3: // AMP
                createParameterCard(parent, backend, "amp.level",          8,   82, 190, 110);
                createParameterCard(parent, backend, "amp.pan",            206, 82, 190, 110);
                createParameterCard(parent, backend, "amp.vel_sens",       404, 82, 190, 110);
                createParameterCard(parent, backend, "amp.portamento_ms",  602, 82, 190, 110);
                createParameterCard(parent, backend, "perf.master_volume", 8,   202, 388, 110);
                break;

            case 4: // ENV
                createEnvelopeEditor(parent, backend, 8, 82, 784, 148);
                createParameterCard(parent, backend, "env.amp.attack_ms",  8,   238, 150, 144);
                createParameterCard(parent, backend, "env.amp.decay_ms",   166, 238, 150, 144);
                createParameterCard(parent, backend, "env.amp.sustain",    324, 238, 150, 144);
                createParameterCard(parent, backend, "env.amp.release_ms", 482, 238, 150, 144);
                createParameterCard(parent, backend, "env.amp.curve",      640, 238, 152, 144);
                break;

            case 5: // LFO
                createParameterCard(parent, backend, "lfo.1.wave",      8,   82, 256, 110);
                createParameterCard(parent, backend, "lfo.1.rate_hz",   272, 82, 256, 110);
                createParameterCard(parent, backend, "lfo.1.depth",     536, 82, 256, 110);

                createParameterCard(parent, backend, "lfo.1.phase_deg", 8,   202, 256, 110);
                createParameterCard(parent, backend, "lfo.1.sync",      272, 202, 256, 110);
                createParameterCard(parent, backend, "lfo.1.dest",      536, 202, 256, 110);
                break;

            case 6: // MOD
                createModulationMatrixWidget(parent, backend, nav, 0, 8, 82, 784, 298);
                break;
        }

        return true;
    }

    void refresh(UiDemoBackend&) override {}
};

} // namespace

ScreenView* createSoundEditorScreen() {
    return new SoundEditorScreen();
}

} // namespace smk::p4::ui
