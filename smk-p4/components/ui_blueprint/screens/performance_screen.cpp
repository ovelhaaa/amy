#include "ui_screens.h"
#include "ui_widgets.h"
#include "routing_editor.h"
#include <memory>
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
        legacy_ = nullptr;
        confirm_ = nullptr;
        oct_lbl_ = status_lbl_ = kb_ = nullptr;
        const uint8_t view_mode = nav.subTab(ScreenId::Performance);
        if (view_mode == 3) {
            routing_ = std::make_unique<RoutingEditor>();
            routing_root_ = lv_obj_create(parent);
            lv_obj_remove_style_all(routing_root_);
            lv_obj_set_size(routing_root_, lv_pct(100), lv_pct(100));
            lv_obj_remove_flag(routing_root_, LV_OBJ_FLAG_SCROLLABLE);
            return routing_->build(routing_root_, backend, nav);
        }
        if (view_mode == 2) return buildLegacy(parent, backend, nav);

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

            if (backend.hasSoundEditor()) {
                auto* routing = createActionButton(parent, "ROUTING EDITOR", 420, 226, 188, 34, nullptr, nullptr);
                attachLvContext<UiNavigationManager*>(routing, &nav, [](lv_event_t* e) {
                    auto* navigation = *static_cast<UiNavigationManager**>(lv_event_get_user_data(e));
                    navigation->navigateTo(ScreenId::Performance, 3);
                }, LV_EVENT_CLICKED);
                auto* legacy = createActionButton(parent, "LEGACY OFFSETS", 620, 226, 172, 34, nullptr, nullptr);
                attachLvContext<UiNavigationManager*>(legacy, &nav, [](lv_event_t* e) {
                    auto* navigation = *static_cast<UiNavigationManager**>(lv_event_get_user_data(e));
                    navigation->navigateTo(ScreenId::Performance, 2);
                }, LV_EVENT_CLICKED);
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

    void onDestroy() override {
        if (routing_root_) lv_obj_delete(routing_root_);
        routing_root_ = nullptr;
        routing_.reset();
    }

    void refresh(IUiBackend& backend) override {
        if (routing_) { routing_->refresh(); return; }
        if (legacy_) {
            IUiBackend::LegacyOffsets offsets;
            const bool available = backend.legacyOffsets(offsets);
            lv_label_set_text(legacy_, !available ? "LEGACY OFFSETS UNAVAILABLE" :
                offsets.fm ? (offsets.compare ? "DX7 | COMPARE ACTIVE" : "DX7") :
                             (offsets.compare ? "JUNO | COMPARE ACTIVE" : "JUNO"));
            constexpr const char* names[]{"BRIGHTNESS", "ATTACK", "RELEASE"};
            constexpr const char* juno[]{"Filter cutoff", "Amp envelope attack", "Amp envelope release"};
            constexpr const char* fm[]{"FM depth", "Juno only", "Juno only"};
            for (unsigned i = 0; i < 3; ++i) {
                char text[96]{};
                if (available) {
                    const char* state = offsets.positions[i] == 64 ? "NEUTRAL" :
                        offsets.compare ? "BYPASSED" : "ACTIVE";
                    std::snprintf(text, sizeof(text), "%s   %u / 64   %s   %s", names[i],
                        unsigned(offsets.positions[i]), state, offsets.fm ? fm[i] : juno[i]);
                }
                lv_label_set_text(legacy_rows_[i], text);
            }
        }
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
    bool buildLegacy(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) {
        confirming_ = false;
        createLabel(parent, "LEGACY MACRO OFFSETS", 12, 14, 560, &lv_font_montserrat_14, palette::kAccentPrimary);
        legacy_ = createLabel(parent, "", 12, 70, 760, &lv_font_montserrat_14, palette::kTextSecondary);
        for (unsigned i = 0; i < 3; ++i)
            legacy_rows_[i] = createLabel(parent, "", 12, 112 + 36 * i, 760,
                &lv_font_montserrat_14, palette::kTextPrimary);
        confirm_ = createLabel(parent, "", 12, 252, 760, &lv_font_montserrat_14, palette::kAccentPrimary);
        auto* back = createActionButton(parent, "BACK / CANCEL", 12, 302, 184, 44, nullptr, nullptr);
        attachLvContext<UiNavigationManager*>(back, &nav, [](lv_event_t* e) {
            auto* navigation = *static_cast<UiNavigationManager**>(lv_event_get_user_data(e));
            navigation->navigateTo(ScreenId::Performance, 0);
        }, LV_EVENT_CLICKED);
        struct ResetContext { PerformanceScreen* screen; IUiBackend* backend; };
        auto* reset = createActionButton(parent, "RESET LEGACY OFFSETS", 206, 302, 244, 44, nullptr, nullptr);
        attachLvContext<ResetContext>(reset, {this, &backend}, [](lv_event_t* e) {
            auto& context = *static_cast<ResetContext*>(lv_event_get_user_data(e));
            auto& screen = *context.screen;
            if (screen.confirming_) {
                const auto result = context.backend->resetLegacyOffsets(screen.expected_);
                lv_label_set_text(screen.confirm_, result.state == UiCommandState::Requested ?
                    "" : "RESET REJECTED.");
                screen.confirming_ = false;
            } else if (context.backend->legacyOffsets(screen.expected_) && !screen.expected_.compare) {
                screen.confirming_ = true;
                lv_label_set_text(screen.confirm_, "TAP RESET AGAIN TO SET ALL THREE OFFSETS TO 64.");
            }
        }, LV_EVENT_CLICKED);
        status_lbl_ = createLabel(parent, "", 462, 314, 300, &lv_font_montserrat_14, palette::kAccentPrimary);
        refresh(backend);
        return true;
    }

    std::unique_ptr<RoutingEditor> routing_;
    lv_obj_t* routing_root_ = nullptr;
    IUiBackend::LegacyOffsets expected_{};
    bool confirming_ = false;
    lv_obj_t* legacy_ = nullptr;
    lv_obj_t* legacy_rows_[3]{};
    lv_obj_t* confirm_ = nullptr;
    lv_obj_t* oct_lbl_    = nullptr;
    lv_obj_t* status_lbl_ = nullptr;
    lv_obj_t* kb_         = nullptr;
};

} // namespace

ScreenView* createPerformanceScreen() {
    return new PerformanceScreen();
}

} // namespace smk::p4::ui
