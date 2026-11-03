#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>

namespace smk::p4::ui {

namespace {

struct PerfActionCtx {
    UiDemoBackend*       backend = nullptr;
    UiNavigationManager* nav     = nullptr;
    int                  action  = 0; // 0=CycleLayerMode, 1=GoHarmony, 2=ToggleCompactView, 3=OctDown, 4=OctUp
};

class PerformanceScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::Performance; }

    bool build(lv_obj_t* parent, UiDemoBackend& backend, UiNavigationManager& nav) override {
        // 1. Top Patch Selector Card (y = 4..62)
        createPatchSelectorCard(parent, backend, nav, 8, 4, 784, 58);

        // 2. Stage Quick Status & Mode Strip (y = 66..104)
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
            std::snprintf(lmode_buf, sizeof(lmode_buf), "MODE: %s", tmp);
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
        std::snprintf(harm_buf, sizeof(harm_buf), "SCALE: %s %s [%s] | CHORD: %s",
                      r_str, s_str, scale_on ? "ON" : "OFF", chord_on ? c_str : "OFF");

        const uint8_t view_mode = nav.subTab(ScreenId::Performance);
        lv_obj_t* lmode_btn = createActionButton(strip, lmode_buf, 4, 1, 180, 32, nullptr, nullptr);
        lv_obj_t* harm_btn  = createActionButton(strip, harm_buf, 190, 1, 356, 32, nullptr, nullptr,
                                                 scale_on || chord_on, false);
        char bpm_buf[24];
        std::snprintf(bpm_buf, sizeof(bpm_buf), "%.1f BPM",
                      static_cast<double>(backend.model().getFloat("perf.bpm", 120.0f)));
        createLabel(strip, bpm_buf, 554, 8, 86, &lv_font_montserrat_14, palette::kAccentPrimary);

        lv_obj_t* view_btn = createActionButton(
            strip, view_mode == 0 ? "VIEW: 8 MACROS" : "VIEW: 2-OCT KEYS",
            644, 1, 132, 32, nullptr, nullptr, false, true);

        auto act_cb = [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* ctx = static_cast<PerfActionCtx*>(lv_event_get_user_data(e));
            if (!ctx || !ctx->backend || !ctx->nav) return;
            if (ctx->action == 0) {
                const int32_t cur = ctx->backend->model().getInt("perf.layer_mode", 0);
                ctx->backend->model().setInt("perf.layer_mode", (cur + 1) % 4);
                ctx->nav->navigateTo(ScreenId::Performance, ctx->nav->subTab(ScreenId::Performance));
            } else if (ctx->action == 1) {
                ctx->nav->navigateTo(ScreenId::Harmony);
            } else if (ctx->action == 2) {
                const uint8_t next_tab = (ctx->nav->subTab(ScreenId::Performance) == 0) ? 1 : 0;
                ctx->nav->navigateTo(ScreenId::Performance, next_tab);
            } else if (ctx->action == 3) {
                const int32_t oct = ctx->backend->model().getInt("perf.octave_shift", 0);
                ctx->backend->model().setInt("perf.octave_shift", oct - 1);
                ctx->nav->navigateTo(ScreenId::Performance, ctx->nav->subTab(ScreenId::Performance));
            } else if (ctx->action == 4) {
                const int32_t oct = ctx->backend->model().getInt("perf.octave_shift", 0);
                ctx->backend->model().setInt("perf.octave_shift", oct + 1);
                ctx->nav->navigateTo(ScreenId::Performance, ctx->nav->subTab(ScreenId::Performance));
            }
        };

        if (lmode_btn) attachLvContext<PerfActionCtx>(lmode_btn, PerfActionCtx{&backend, &nav, 0}, act_cb, LV_EVENT_CLICKED);
        if (harm_btn)  attachLvContext<PerfActionCtx>(harm_btn,  PerfActionCtx{&backend, &nav, 1}, act_cb, LV_EVENT_CLICKED);
        if (view_btn)  attachLvContext<PerfActionCtx>(view_btn,  PerfActionCtx{&backend, &nav, 2}, act_cb, LV_EVENT_CLICKED);

        static constexpr const char* kMacroIds[8] = {
            "perf.macro.1", "perf.macro.2", "perf.macro.3", "perf.macro.4",
            "perf.macro.5", "perf.macro.6", "perf.macro.7", "perf.macro.8"
        };

        if (view_mode == 0) {
            // Detailed 8-Macro Grid (4x2, y = 108..264)
            for (uint8_t i = 0; i < 8; ++i) {
                const int32_t mx = 8 + (i % 4) * 198;
                const int32_t my = 108 + (i / 4) * 78;
                createParameterSlider(parent, backend, kMacroIds[i], mx, my, 190, 74);
            }

            // Bottom Stage Control Bar: Master Volume + Octave + 1-Octave Stage Keyboard (y = 268..388)
            createParameterSlider(parent, backend, "perf.master_volume", 8, 268, 232, 74);

            lv_obj_t* oct_dn = createActionButton(parent, "OCT -", 8, 346, 112, 42, nullptr, nullptr);
            lv_obj_t* oct_up = createActionButton(parent, "OCT +", 128, 346, 112, 42, nullptr, nullptr);
            if (oct_dn) attachLvContext<PerfActionCtx>(oct_dn, PerfActionCtx{&backend, &nav, 3}, act_cb, LV_EVENT_CLICKED);
            if (oct_up) attachLvContext<PerfActionCtx>(oct_up, PerfActionCtx{&backend, &nav, 4}, act_cb, LV_EVENT_CLICKED);

            createVirtualKeyboard(parent, backend, nav, KeyboardDisplayMode::StagePlay, 248, 268, 544, 120);
        } else {
            // Compact 4-Knob + Full-Width 2-Octave Keyboard View
            for (uint8_t i = 0; i < 4; ++i) {
                createParameterKnob(parent, backend, kMacroIds[i], 8 + i * 136, 108, 130, 114);
            }
            createParameterSlider(parent, backend, "perf.master_volume", 552, 108, 240, 74);
            notice_lbl_ = createLabel(parent, backend.model().lastNotice(), 554, 190, 236,
                                      &lv_font_montserrat_14, palette::kTextSecondary);

            createVirtualKeyboard(parent, backend, nav, KeyboardDisplayMode::HarmonyView, 8, 226, 784, 162);
        }

        return true;
    }

    void refresh(UiDemoBackend& backend) override {
        if (notice_lbl_) {
            lv_label_set_text(notice_lbl_, backend.model().lastNotice());
        }
    }

private:
    lv_obj_t* notice_lbl_ = nullptr;
};

} // namespace

ScreenView* createPerformanceScreen() {
    return new PerformanceScreen();
}

} // namespace smk::p4::ui
