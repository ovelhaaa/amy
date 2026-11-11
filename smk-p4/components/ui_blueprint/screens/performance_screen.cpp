#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>

namespace smk::p4::ui {

namespace {

struct PerfActionCtx {
    IUiBackend*          backend = nullptr;
    UiNavigationManager* nav     = nullptr;
    int                  action  = 0; // 0=CycleLayerMode, 1=GoHarmony, 2=ToggleStageMode, 3=OctDown, 4=OctUp, 5=MIDI
};

class PerformanceScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::Performance; }

    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) override {
        const uint8_t view_mode = nav.subTab(ScreenId::Performance) % 2; // 0=MACRO FOCUS, 1=KEYS FOCUS

        // 1. Dominant Stage Patch Banner (y = 4..62, StageMinimal)
        createPatchSelectorCard(parent, backend, nav, 8, 4, 784, 58, PresentationLevel::StageMinimal);

        // 2. Stage Control & Focus Strip (y = 66..104)
        lv_obj_t* strip = lv_obj_create(parent);
        applyPanelStyle(strip, false);
        lv_obj_set_pos(strip, 8, 66);
        lv_obj_set_size(strip, 784, 38);
        lv_obj_set_style_pad_all(strip, 2, 0);

        ParameterValue lmode_pv{};
        backend.model().getValue("perf.layer_mode", lmode_pv);
        const ParameterDescriptor* lmode_desc = ParameterRegistry::findById("perf.layer_mode");
        char lmode_buf[32] = "LAYER: SINGLE A";
        if (lmode_desc) {
            char tmp[20];
            ParameterRegistry::formatValue(*lmode_desc, lmode_pv, tmp, sizeof(tmp));
            std::snprintf(lmode_buf, sizeof(lmode_buf), "LAYER: %s", tmp);
        }

        const bool scale_on = backend.model().getBool("harmony.scale_enable", true);
        const bool chord_on = backend.model().getBool("harmony.chord_enable", true);
        ParameterValue root_pv{}, scale_pv{}, chord_pv{};
        backend.model().getValue("harmony.root_note", root_pv);
        backend.model().getValue("harmony.scale_type", scale_pv);
        backend.model().getValue("harmony.chord_type", chord_pv);
        char r_str[12] = "C", s_str[16] = "MINOR", c_str[16] = "MIN";
        if (const auto* d = ParameterRegistry::findById("harmony.root_note"))
            ParameterRegistry::formatValue(*d, root_pv, r_str, sizeof(r_str));
        if (const auto* d = ParameterRegistry::findById("harmony.scale_type"))
            ParameterRegistry::formatValue(*d, scale_pv, s_str, sizeof(s_str));
        if (const auto* d = ParameterRegistry::findById("harmony.chord_type"))
            ParameterRegistry::formatValue(*d, chord_pv, c_str, sizeof(c_str));

        char harm_buf[64];
        std::snprintf(harm_buf, sizeof(harm_buf), "HARM: %s %s | %s",
                      r_str, scale_on ? s_str : "OFF", chord_on ? c_str : "OFF");
        if (!backend.isDemoMode()) std::snprintf(harm_buf, sizeof(harm_buf), "HARMONY: PLANNED / PREVIEW");

        lv_obj_t* lmode_btn = createActionButton(strip, lmode_buf, 4, 1, 174, 32, nullptr, nullptr);
        if(lmode_btn && backend.hasMidiPerformance())lv_obj_add_flag(lmode_btn,LV_OBJ_FLAG_HIDDEN);
        lv_obj_t* midi_btn=backend.hasMidiPerformance()?
            createActionButton(strip,backend.midiBankName(),4,1,174,32,nullptr,nullptr):nullptr;
        lv_obj_t* harm_btn  = createActionButton(strip, harm_buf, 184, 1, 266, 32, nullptr, nullptr,
                                                 scale_on || chord_on, false);

        status_lbl_ = createLabel(strip, "", 460, 7, 152,
                                  &lv_font_montserrat_14, palette::kAccentPrimary);

        lv_obj_t* view_btn = createActionButton(
            strip,
            view_mode == 0 ? "KEYS FOCUS >" : "< MACRO FOCUS",
            620, 1, 156, 32, nullptr, nullptr, false, true);

        auto act_cb = [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* ctx = static_cast<PerfActionCtx*>(lv_event_get_user_data(e));
            if (!ctx || !ctx->backend || !ctx->nav) return;
            if (ctx->action == 0) {
                const int32_t cur = ctx->backend->model().getInt("perf.layer_mode", 0);
                ctx->backend->requestParameterInt("perf.layer_mode", (cur + 1) % 4);
                ctx->nav->navigateTo(ScreenId::Performance, ctx->nav->subTab(ScreenId::Performance));
            } else if (ctx->action == 1) {
                ctx->nav->navigateTo(ScreenId::Harmony);
            } else if (ctx->action == 2) {
                const uint8_t next_tab = (ctx->nav->subTab(ScreenId::Performance) == 0) ? 1 : 0;
                ctx->nav->navigateTo(ScreenId::Performance, next_tab);
            } else if (ctx->action == 3) {
                const int32_t oct = ctx->backend->model().getInt("perf.octave_shift", 0);
                ctx->backend->requestParameterInt("perf.octave_shift", oct - 1);
                ctx->nav->refreshActive();
            } else if (ctx->action == 4) {
                const int32_t oct = ctx->backend->model().getInt("perf.octave_shift", 0);
                ctx->backend->requestParameterInt("perf.octave_shift", oct + 1);
                ctx->nav->refreshActive();
            } else if(ctx->action==5){
                ctx->nav->navigateTo(ScreenId::MidiSettings,1);
            }
        };

        if (lmode_btn) attachLvContext<PerfActionCtx>(lmode_btn, PerfActionCtx{&backend, &nav, 0}, act_cb, LV_EVENT_CLICKED);
        if (harm_btn)  attachLvContext<PerfActionCtx>(harm_btn,  PerfActionCtx{&backend, &nav, 1}, act_cb, LV_EVENT_CLICKED);
        if (view_btn)  attachLvContext<PerfActionCtx>(view_btn,  PerfActionCtx{&backend, &nav, 2}, act_cb, LV_EVENT_CLICKED);
        if (midi_btn) attachLvContext<PerfActionCtx>(midi_btn,PerfActionCtx{&backend,&nav,5},act_cb,LV_EVENT_CLICKED);

        static constexpr const char* kMacroIds[8] = {
            "perf.macro.1", "perf.macro.2", "perf.macro.3", "perf.macro.4",
            "perf.macro.5", "perf.macro.6", "perf.macro.7", "perf.macro.8"
        };
        static constexpr const char* kMacroLabels[8] = {
            "CHARACTER", "BRIGHTNESS", "MOTION", "SHAPE", "ATTACK", "RELEASE", "SPACE", "EDGE"
        };

        if (view_mode == 0) {
            // ── MACRO FOCUS (SMK25 Physical Controller Stage View) ────────────
            for (uint8_t i = 0; i < 8; ++i) {
                const int32_t mx = 8 + (i % 4) * 198;
                const int32_t my = 108 + (i / 4) * 78;
                createParameterSlider(parent, backend, kMacroIds[i], mx, my, 190, 74,
                                      PresentationLevel::StageMinimal, kMacroLabels[i]);
            }

            // Bottom Performance Deck: Master Volume + Octave + 1-Octave Stage Keyboard (y = 268..388)
            createParameterSlider(parent, backend, "perf.master_volume", 8, 268, 232, 72,
                                  PresentationLevel::StageMinimal, "MASTER LEVEL");

            lv_obj_t* oct_dn = createActionButton(parent, "OCT -", 8, 344, 82, 44, nullptr, nullptr);
            oct_lbl_ = createLabel(parent, "OCT +0", 92, 356, 60, &lv_font_montserrat_14, palette::kAccentPrimary);
            if (oct_lbl_) lv_obj_set_style_text_align(oct_lbl_, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_t* oct_up = createActionButton(parent, "OCT +", 158, 344, 82, 44, nullptr, nullptr);

            if (oct_dn) attachLvContext<PerfActionCtx>(oct_dn, PerfActionCtx{&backend, &nav, 3}, act_cb, LV_EVENT_CLICKED);
            if (oct_up) attachLvContext<PerfActionCtx>(oct_up, PerfActionCtx{&backend, &nav, 4}, act_cb, LV_EVENT_CLICKED);

            kb_ = createVirtualKeyboard(parent, backend, nav, KeyboardDisplayMode::StagePlay, 248, 268, 544, 120);
        } else {
            // ── KEYS FOCUS (Direct Touchscreen Performance View) ──────────────
            // 4 Compact Macros (y = 108..214) + Master Volume + Octave + Large 2-Octave Keyboard (y = 218..388)
            for (uint8_t i = 0; i < 4; ++i) {
                createParameterKnob(parent, backend, kMacroIds[i], 8 + i * 136, 108, 130, 106,
                                    PresentationLevel::StageMinimal, kMacroLabels[i]);
            }
            createParameterSlider(parent, backend, "perf.master_volume", 552, 108, 240, 64,
                                  PresentationLevel::StageMinimal, "MASTER LEVEL");

            lv_obj_t* oct_dn = createActionButton(parent, "OCT -", 552, 176, 86, 38, nullptr, nullptr);
            oct_lbl_ = createLabel(parent, "OCT +0", 642, 185, 60, &lv_font_montserrat_14, palette::kAccentPrimary);
            if (oct_lbl_) lv_obj_set_style_text_align(oct_lbl_, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_t* oct_up = createActionButton(parent, "OCT +", 706, 176, 86, 38, nullptr, nullptr);

            if (oct_dn) attachLvContext<PerfActionCtx>(oct_dn, PerfActionCtx{&backend, &nav, 3}, act_cb, LV_EVENT_CLICKED);
            if (oct_up) attachLvContext<PerfActionCtx>(oct_up, PerfActionCtx{&backend, &nav, 4}, act_cb, LV_EVENT_CLICKED);

            kb_ = createVirtualKeyboard(parent, backend, nav, KeyboardDisplayMode::StageKeys2Oct, 8, 218, 784, 170);
        }

        refresh(backend);
        return true;
    }

    void refresh(IUiBackend& backend) override {
        if (oct_lbl_) {
            const int32_t oct = backend.model().getInt("perf.octave_shift", 0);
            char obuf[16];
            std::snprintf(obuf, sizeof(obuf), "OCT %+ld", static_cast<long>(oct));
            lv_label_set_text(oct_lbl_, obuf);
        }
        if (status_lbl_) {
            char st_buf[32];
            std::snprintf(st_buf, sizeof(st_buf), "CMD: %s", commandStateLabel(backend.lastCommandState()));
            lv_label_set_text(status_lbl_, st_buf);
        }
        if (kb_) {
            refreshVirtualKeyboard(kb_);
        }
    }

private:
    lv_obj_t* oct_lbl_    = nullptr;
    lv_obj_t* status_lbl_ = nullptr;
    lv_obj_t* kb_         = nullptr;
};

} // namespace

ScreenView* createPerformanceScreen() {
    return new PerformanceScreen();
}

} // namespace smk::p4::ui
