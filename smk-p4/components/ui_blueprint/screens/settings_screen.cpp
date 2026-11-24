#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>

namespace smk::p4::ui {

namespace {

struct SysActionCtx {
    IUiBackend*          backend = nullptr;
    UiNavigationManager* nav     = nullptr;
    int                  action  = 0;
};

class SettingsScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::GlobalSettings; }

    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) override {
        parent_ = parent;
        backend_ = &backend;
        nav_ = &nav;
        confirm_dialog_.close(false);

        const uint8_t mod = nav.subTab(ScreenId::GlobalSettings) % 4;

        // 1. Left Contextual Sidebar (x = 4..160, y = 4..384)
        lv_obj_t* sidebar = lv_obj_create(parent);
        applyPanelStyle(sidebar, false);
        lv_obj_set_pos(sidebar, 4, 4);
        lv_obj_set_size(sidebar, 156, 380);
        lv_obj_set_style_pad_all(sidebar, 4, 0);
        lv_obj_remove_flag(sidebar, LV_OBJ_FLAG_SCROLLABLE);

        createLabel(sidebar, "PREFERENCIAS", 4, 2, 140, &lv_font_montserrat_14, palette::kAccentPrimary);

        static constexpr const char* kMods[4] = {"SISTEMA", "AUDIO", "PERFORMANCE", "ARMAZENAMENTO"};
        struct ModCtx { UiNavigationManager* nav; uint8_t mod; };
        for (uint8_t i = 0; i < 4; ++i) {
            lv_obj_t* b = createActionButton(sidebar, kMods[i], 2, 28 + i * 48, 144, 42, nullptr, nullptr, mod == i, false);
            if (b) {
                attachLvContext<ModCtx>(b, {&nav, i}, [](lv_event_t* e) {
                    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                    auto* c = static_cast<ModCtx*>(lv_event_get_user_data(e));
                    if (c && c->nav) c->nav->requestNavigateTo(ScreenId::GlobalSettings, c->mod);
                }, LV_EVENT_CLICKED);
            }
        }

        lv_obj_t* pf_btn = createActionButton(sidebar, "TOCAR >", 2, 336, 144, 34, nullptr, nullptr);
        if (pf_btn) {
            attachLvContext<UiNavigationManager*>(pf_btn, &nav, [](lv_event_t* e) {
                if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                auto* n = *static_cast<UiNavigationManager**>(lv_event_get_user_data(e));
                if (n) n->requestReturnToPerformance();
            }, LV_EVENT_CLICKED);
        }

        // 2. Right Contextual Panel (x = 166..794, y = 4..384)
        constexpr int32_t kX = 166;
        switch (mod) {
            case 0: { // SISTEMA
                createParameterCard(parent, backend, "sys.brightness", kX,       4, 204, 114);
                createParameterCard(parent, backend, "sys.theme",      kX + 212, 4, 204, 114);
                createParameterCard(parent, backend, "sys.boot_mode",  kX + 424, 4, 204, 114);

                lv_obj_t* touch_card = lv_obj_create(parent);
                applyPanelStyle(touch_card, false);
                lv_obj_set_pos(touch_card, kX, 128);
                lv_obj_set_size(touch_card, 628, 254);
                createLabel(touch_card, "DISPLAY & TOUCH HAL", 8, 6, 300, &lv_font_montserrat_14, palette::kAccentPrimary);
                createStatusBadge(touch_card, FeatureStatus::Available, 540, 6);
                createLabel(touch_card, "LCD: ST7701S 800x480 RGB565 (GUITION JC-ESP32P4-M3)", 8, 40, 600,
                            &lv_font_montserrat_14, palette::kTextPrimary);
                createLabel(touch_card, "TOUCH: GT911 CAPACITIVE I2C (NATIVO 800x480)", 8, 68, 600,
                            &lv_font_montserrat_14, palette::kTextPrimary);
                createLabel(touch_card, "BRILHO / TEMA: Marcados PLANNED quando controle de backlight nao esta ligado.", 8, 96, 600,
                            &lv_font_montserrat_14, palette::kTextSecondary);
                break;
            }
            case 1: { // AUDIO
                createParameterCard(parent, backend, "sys.sample_rate_hz",   kX,       4, 310, 114);
                createParameterCard(parent, backend, "sys.master_tuning_hz", kX + 318, 4, 310, 114);

                lv_obj_t* aud_card = lv_obj_create(parent);
                applyPanelStyle(aud_card, false);
                lv_obj_set_pos(aud_card, kX, 128);
                lv_obj_set_size(aud_card, 628, 254);
                createLabel(aud_card, "CONFIGURACAO DO DAC DE AUDIO", 8, 6, 340, &lv_font_montserrat_14, palette::kAccentPrimary);
                createStatusBadge(aud_card, FeatureStatus::Available, 540, 6);
                createLabel(aud_card, "DAC: PCM5102A STEREO I2S", 8, 40, 600, &lv_font_montserrat_14, palette::kTextPrimary);
                createLabel(aud_card, "TAXA DE AMOSTRAGEM: 48000 Hz (TRAVADA NO AUDIO OWNER)", 8, 68, 600,
                            &lv_font_montserrat_14, palette::kTextPrimary);
                createLabel(aud_card, "DMA: 4 x 256 FRAMES (16-BIT) | QUALIFICACAO FISICA PENDENTE", 8, 96, 600,
                            &lv_font_montserrat_14, palette::kTextSecondary);
                break;
            }
            case 2: { // PERFORMANCE (Session Locks)
                lv_obj_t* pcard = lv_obj_create(parent);
                applyPanelStyle(pcard, true);
                lv_obj_set_pos(pcard, kX, 4);
                lv_obj_set_size(pcard, 628, 380);
                createLabel(pcard, "PERFORMANCE LOCKS DE SESSAO", 8, 6, 400, &lv_font_montserrat_14, palette::kAccentPrimary);
                createStatusBadge(pcard, FeatureStatus::Available, 540, 6);
                createLabel(pcard,
                            "Bloqueia dominios durante troca de Patch e Recall de Cenas (preservado em toda sessao):",
                            8, 36, 608, &lv_font_montserrat_14, palette::kTextSecondary);

                static constexpr const char* kLockLabels[4] = {
                    "LOCK FX (EFEITOS)", "LOCK HARMONY (Q/CH)", "LOCK ARP (ARPEJADOR)", "LOCK MACROS (1-8)"
                };
                static constexpr uint8_t kLockBits[4] = {
                    perf_lock::Fx, perf_lock::Harmony, perf_lock::Arp, perf_lock::Macros
                };
                const uint8_t cur_mask = sessionLockMask();
                for (uint8_t li = 0; li < 4; ++li) {
                    const bool on = (cur_mask & kLockBits[li]) != 0U;
                    lv_obj_t* lb = createActionButton(
                        pcard, kLockLabels[li], 8 + (li % 2) * 308, 76 + (li / 2) * 58, 298, 48,
                        nullptr, nullptr, on, false);
                    if (lb) {
                        attachLvContext<SysActionCtx>(lb, SysActionCtx{&backend, &nav, int(kLockBits[li])}, [](lv_event_t* e) {
                            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                            auto* c = static_cast<SysActionCtx*>(lv_event_get_user_data(e));
                            if (!c || !c->nav) return;
                            setSessionLockMask(sessionLockMask() ^ uint8_t(c->action));
                            c->nav->requestNavigateTo(ScreenId::GlobalSettings, 2);
                        }, LV_EVENT_CLICKED);
                    }
                }
                createLabel(pcard,
                            "Nota: LK FX impede aplicacao de cenas que incluam FX sem desbloqueio.",
                            8, 210, 608, &lv_font_montserrat_14, palette::kWarning);
                break;
            }
            case 3: { // ARMAZENAMENTO
                lv_obj_t* store_card = lv_obj_create(parent);
                applyPanelStyle(store_card, true);
                lv_obj_set_pos(store_card, kX, 4);
                lv_obj_set_size(store_card, 628, 380);

                createLabel(store_card, "ARMAZENAMENTO NVS, PRESETS & BACKUP", 8, 6, 460,
                            &lv_font_montserrat_14, palette::kAccentPrimary);
                createStatusBadge(store_card, backend.isDemoMode() ? FeatureStatus::Demo : FeatureStatus::Available, 540, 6);

                createLabel(store_card,
                            "FW: SMK-P4 UI 0.18-C.2 | TARGET: GUITION JC-ESP32P4-M3 | LVGL 9.3.0 / ESP-IDF 5.5.2",
                            8, 40, 608, &lv_font_montserrat_14, palette::kTextPrimary);
                createLabel(store_card,
                            backend.isDemoMode()
                                ? "POLITICA DEMO: Escritas em NVS/Flash estritamente desativadas (somente RAM)."
                                : "POLITICA NVS: Gravacao apenas com notas soltas (Quiet) e CRC verificado.",
                            8, 68, 608, &lv_font_montserrat_14, palette::kWarning);

                auto act_cb = [](lv_event_t* e) {
                    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
                    auto* ctx = static_cast<SysActionCtx*>(lv_event_get_user_data(e));
                    if (!ctx || !ctx->backend || !ctx->nav) return;
                    if (ctx->action == 0) {
                        if (!ctx->backend->isDemoMode()) {
                            ctx->backend->model().setNotice("RESET UNSUPPORTED IN LIVE MODE; NO DSP CHANGE");
                            return;
                        }
                        ctx->backend->model().resetToDefaults();
                        ctx->backend->model().setNotice("ALL UI DEMO PARAMETERS RESET TO DEFAULTS IN RAM");
                        ctx->nav->requestNavigateTo(ScreenId::GlobalSettings, 3);
                    } else if (ctx->action == 1) {
                        ctx->backend->model().setNotice("EXPORT BACKUP: PLANNED (NAO IMPLEMENTADO)");
                    } else if (ctx->action == 2) {
                        ctx->backend->model().setNotice("IMPORT BACKUP: PLANNED (NAO IMPLEMENTADO)");
                    }
                };

                lv_obj_t* rst_btn = createActionButton(
                    store_card, "RESET DEMO RAM DEFAULTS", 8, 120, 296, 44, nullptr, nullptr,
                    false, backend.isDemoMode());
                if (rst_btn && !backend.isDemoMode()) {
                    lv_obj_add_state(rst_btn, LV_STATE_DISABLED);
                }

                lv_obj_t* bkp_btn = createActionButton(
                    store_card, "EXPORT BACKUP (PLANNED)", 8, 180, 296, 44, nullptr, nullptr);
                lv_obj_t* lod_btn = createActionButton(
                    store_card, "IMPORT BACKUP (PLANNED)", 318, 180, 296, 44, nullptr, nullptr);
                if (bkp_btn) lv_obj_add_state(bkp_btn, LV_STATE_DISABLED);
                if (lod_btn) lv_obj_add_state(lod_btn, LV_STATE_DISABLED);

                createLabel(store_card,
                            "Backup/Restore via SD/USB permanece PLANNED (controles desabilitados).",
                            8, 240, 608, &lv_font_montserrat_14, palette::kTextSecondary);

                if (rst_btn) attachLvContext<SysActionCtx>(rst_btn, SysActionCtx{&backend, &nav, 0}, act_cb, LV_EVENT_CLICKED);
                if (bkp_btn) attachLvContext<SysActionCtx>(bkp_btn, SysActionCtx{&backend, &nav, 1}, act_cb, LV_EVENT_CLICKED);
                if (lod_btn) attachLvContext<SysActionCtx>(lod_btn, SysActionCtx{&backend, &nav, 2}, act_cb, LV_EVENT_CLICKED);
                break;
            }
        }

        return true;
    }

    void onDestroy() override {
        confirm_dialog_.close(false);
    }

    void refresh(IUiBackend&) override {}

private:
    lv_obj_t* parent_ = nullptr;
    IUiBackend* backend_ = nullptr;
    UiNavigationManager* nav_ = nullptr;
    ConfirmDialog confirm_dialog_{};
};

} // namespace

ScreenView* createSettingsScreen() {
    return new SettingsScreen();
}

} // namespace smk::p4::ui
