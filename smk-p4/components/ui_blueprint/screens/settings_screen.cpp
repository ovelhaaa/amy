#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>

namespace smk::p4::ui {

namespace {

struct SysActionCtx {
    UiDemoBackend*       backend = nullptr;
    UiNavigationManager* nav     = nullptr;
    int                  action  = 0; // 0=ResetDefaults, 1=BackupNotice, 2=RestoreNotice
};

class SettingsScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::GlobalSettings; }

    bool build(lv_obj_t* parent, UiDemoBackend& backend, UiNavigationManager& nav) override {
        // 1. Row 1: Display, Theme, Sample Rate (Read-Only), Master Tuning, Boot Mode (y = 4..116)
        createParameterCard(parent, backend, "sys.brightness",       8,   4, 190, 108);
        createParameterCard(parent, backend, "sys.theme",            206, 4, 190, 108);
        createParameterCard(parent, backend, "sys.sample_rate_hz",   404, 4, 190, 108);
        createParameterCard(parent, backend, "sys.master_tuning_hz", 602, 4, 190, 108);

        // 2. Row 2: Boot State + Touch Hardware Card + Audio Architecture Card (y = 120..244)
        createParameterCard(parent, backend, "sys.boot_mode", 8, 120, 256, 120);

        lv_obj_t* touch_card = lv_obj_create(parent);
        applyPanelStyle(touch_card, false);
        lv_obj_set_pos(touch_card, 272, 120);
        lv_obj_set_size(touch_card, 256, 120);
        createLabel(touch_card, "DISPLAY & TOUCH HAL", 6, 4, 170, &lv_font_montserrat_14, palette::kAccentPrimary);
        createStatusBadge(touch_card, FeatureStatus::Available, 178, 4);
        createLabel(touch_card, "LCD: ST7701S 800x480 RGB565", 6, 30, 236, &lv_font_montserrat_14, palette::kTextPrimary);
        createLabel(touch_card, "TOUCH: GT911 CAPACITIVE I2C", 6, 54, 236, &lv_font_montserrat_14, palette::kTextPrimary);
        createLabel(touch_card, "CALIB: FACTORY NATIVE 800x480", 6, 78, 236, &lv_font_montserrat_14, palette::kTextSecondary);

        lv_obj_t* aud_card = lv_obj_create(parent);
        applyPanelStyle(aud_card, false);
        lv_obj_set_pos(aud_card, 536, 120);
        lv_obj_set_size(aud_card, 256, 120);
        createLabel(aud_card, "AUDIO DAC CONFIG", 6, 4, 170, &lv_font_montserrat_14, palette::kAccentPrimary);
        createStatusBadge(aud_card, FeatureStatus::Available, 178, 4);
        createLabel(aud_card, "DAC: PCM5102A STEREO I2S", 6, 30, 236, &lv_font_montserrat_14, palette::kTextPrimary);
        createLabel(aud_card, "RATE: 48000 Hz (LOCKED)", 6, 54, 236, &lv_font_montserrat_14, palette::kTextPrimary);
        createLabel(aud_card, "DMA: 4 x 256 FRAMES (16-BIT)", 6, 78, 236, &lv_font_montserrat_14, palette::kTextSecondary);

        // 3. Row 3: Preset Management, Backup/Restore & Firmware Info (y = 250..386)
        lv_obj_t* store_card = lv_obj_create(parent);
        applyPanelStyle(store_card, true);
        lv_obj_set_pos(store_card, 8, 250);
        lv_obj_set_size(store_card, 784, 136);

        createLabel(store_card, "PRESET STORAGE, BACKUP & FIRMWARE INFORMATION", 8, 4, 520,
                    &lv_font_montserrat_14, palette::kAccentPrimary);
        createStatusBadge(store_card, FeatureStatus::Demo, 700, 4);

        createLabel(store_card,
                    "FW: SMK-P4 UI BLUEPRINT 1.0  |  TARGET: GUITION JC-ESP32P4-M3  |  LVGL 9.3.0 / ESP-IDF 5.5.2",
                    8, 28, 760, &lv_font_montserrat_14, palette::kTextPrimary);
        createLabel(store_card,
                    "PERSISTENCE POLICY: NVS/Flash writes are strictly disabled in UI Demo. All edits live only in RAM.",
                    8, 52, 760, &lv_font_montserrat_14, palette::kWarning);

        auto act_cb = [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            auto* ctx = static_cast<SysActionCtx*>(lv_event_get_user_data(e));
            if (!ctx || !ctx->backend || !ctx->nav) return;
            if (ctx->action == 0) {
                ctx->backend->model().resetToDefaults();
                ctx->backend->model().setNotice("ALL UI DEMO PARAMETERS RESET TO DEFAULTS IN RAM");
            } else if (ctx->action == 1) {
                ctx->backend->model().setNotice("BACKUP PLANNED (NVS/SD NOT ACCESSED BY UI DEMO)");
            } else if (ctx->action == 2) {
                ctx->backend->model().setNotice("RESTORE PLANNED (NVS/SD NOT ACCESSED BY UI DEMO)");
            }
            ctx->nav->navigateTo(ScreenId::GlobalSettings);
        };

        lv_obj_t* rst_btn = createActionButton(store_card, "RESET DEMO RAM DEFAULTS", 8,   80, 244, 40, nullptr, nullptr, false, true);
        lv_obj_t* bkp_btn = createActionButton(store_card, "EXPORT BACKUP (PLANNED)", 264, 80, 244, 40, nullptr, nullptr);
        lv_obj_t* lod_btn = createActionButton(store_card, "IMPORT BACKUP (PLANNED)", 520, 80, 244, 40, nullptr, nullptr);

        if (rst_btn) attachLvContext<SysActionCtx>(rst_btn, SysActionCtx{&backend, &nav, 0}, act_cb, LV_EVENT_CLICKED);
        if (bkp_btn) attachLvContext<SysActionCtx>(bkp_btn, SysActionCtx{&backend, &nav, 1}, act_cb, LV_EVENT_CLICKED);
        if (lod_btn) attachLvContext<SysActionCtx>(lod_btn, SysActionCtx{&backend, &nav, 2}, act_cb, LV_EVENT_CLICKED);

        return true;
    }

    void refresh(UiDemoBackend&) override {}
};

} // namespace

ScreenView* createSettingsScreen() {
    return new SettingsScreen();
}

} // namespace smk::p4::ui
