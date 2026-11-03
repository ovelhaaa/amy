#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>

namespace smk::p4::ui {

namespace {

class DiagnosticsScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::Diagnostics; }

    bool build(lv_obj_t* parent, UiDemoBackend& backend, UiNavigationManager&) override {
        const auto& d = backend.model().diagnostics();

        // 1. Explicit UI DEMO Telemetry Honesty Banner (Section 5 Tela 14 requirement)
        lv_obj_t* banner = lv_obj_create(parent);
        applyPanelStyle(banner, true);
        lv_obj_set_pos(banner, 8, 4);
        lv_obj_set_size(banner, 784, 34);
        lv_obj_set_style_pad_all(banner, 4, 0);
        createLabel(banner,
                    "DIAGNOSTICS  |  UI DEMO BACKEND ACTIVE: HARDWARE TELEMETRY IS NOT FABRICATED (REAL LVGL POOL SHOWN)",
                    6, 2, 768, &lv_font_montserrat_14, palette::kWarning);

        // 2. 6 Structured Diagnostic Cards (3x2 grid, y = 42..386)
        // Card 1: Firmware & UI Runtime
        lv_obj_t* c1 = lv_obj_create(parent);
        applyPanelStyle(c1, false);
        lv_obj_set_pos(c1, 8, 42);
        lv_obj_set_size(c1, 256, 166);
        createLabel(c1, "1. FIRMWARE & UI", 6, 4, 170, &lv_font_montserrat_14, palette::kAccentPrimary);
        createStatusBadge(c1, FeatureStatus::Demo, 178, 4);
        createLabel(c1, d.firmware_version, 6, 28, 240, &lv_font_montserrat_14, palette::kTextPrimary);
        char c1_b1[64], c1_b2[64], c1_b3[64];
        std::snprintf(c1_b1, sizeof(c1_b1), "UPTIME: %lu ms", static_cast<unsigned long>(d.uptime_ms));
        std::snprintf(c1_b2, sizeof(c1_b2), "NAV SWITCHES: %lu", static_cast<unsigned long>(d.screen_switches));
        std::snprintf(c1_b3, sizeof(c1_b3), "DEMO PANICS: %lu", static_cast<unsigned long>(d.demo_panic_count));
        createLabel(c1, c1_b1, 6, 56, 240, &lv_font_montserrat_14, palette::kTextSecondary);
        createLabel(c1, c1_b2, 6, 82, 240, &lv_font_montserrat_14, palette::kTextSecondary);
        createLabel(c1, c1_b3, 6, 108, 240, &lv_font_montserrat_14, palette::kTextSecondary);
        createLabel(c1, "PARAMS REGISTERED: 86", 6, 134, 240, &lv_font_montserrat_14, palette::kActive);

        // Card 2: LVGL 9.3.0 Memory Pool (Real measurement from lv_mem_monitor)
        lv_obj_t* c2 = lv_obj_create(parent);
        applyPanelStyle(c2, false);
        lv_obj_set_pos(c2, 272, 42);
        lv_obj_set_size(c2, 256, 166);
        createLabel(c2, "2. LVGL 9.3.0 HEAP", 6, 4, 170, &lv_font_montserrat_14, palette::kAccentPrimary);
        createStatusBadge(c2, FeatureStatus::Available, 178, 4);
        char c2_b1[64], c2_b2[64], c2_b3[64], c2_b4[64];
        std::snprintf(c2_b1, sizeof(c2_b1), "FREE: %lu bytes", static_cast<unsigned long>(d.lvgl_free_bytes));
        std::snprintf(c2_b2, sizeof(c2_b2), "LARGEST: %lu bytes", static_cast<unsigned long>(d.lvgl_largest_block));
        std::snprintf(c2_b3, sizeof(c2_b3), "PEAK USED: %lu bytes", static_cast<unsigned long>(d.lvgl_peak_used));
        std::snprintf(c2_b4, sizeof(c2_b4), "FRAGMENTATION: %u%%", unsigned(d.lvgl_frag_pct));
        createLabel(c2, c2_b1, 6, 30, 240, &lv_font_montserrat_14, palette::kTextPrimary);
        createLabel(c2, c2_b2, 6, 56, 240, &lv_font_montserrat_14, palette::kTextPrimary);
        createLabel(c2, c2_b3, 6, 82, 240, &lv_font_montserrat_14, palette::kTextSecondary);
        createLabel(c2, c2_b4, 6, 108, 240, &lv_font_montserrat_14, palette::kActive);
        createLabel(c2, "SINGLE ACTIVE SCREEN TREE", 6, 134, 240, &lv_font_montserrat_14, palette::kTextSecondary);

        // Card 3: AMY Engine & DSP Budget
        lv_obj_t* c3 = lv_obj_create(parent);
        applyPanelStyle(c3, false);
        lv_obj_set_pos(c3, 536, 42);
        lv_obj_set_size(c3, 256, 166);
        createLabel(c3, "3. AMY SYNTHESIS", 6, 4, 170, &lv_font_montserrat_14, palette::kAccentPrimary);
        createStatusBadge(c3, FeatureStatus::Demo, 178, 4);
        createLabel(c3, d.amy_status, 6, 30, 240, &lv_font_montserrat_14, palette::kWarning);
        char c3_b1[64], c3_b2[64];
        std::snprintf(c3_b1, sizeof(c3_b1), "DEMO VOICES: %u / %u", d.active_voices, d.max_voices);
        std::snprintf(c3_b2, sizeof(c3_b2), "BLOCK BUDGET: %lu us", static_cast<unsigned long>(d.budget_us));
        createLabel(c3, c3_b1, 6, 56, 240, &lv_font_montserrat_14, palette::kTextPrimary);
        createLabel(c3, c3_b2, 6, 82, 240, &lv_font_montserrat_14, palette::kTextSecondary);
        createLabel(c3, "DSP RENDER: UNMEASURED (DEMO)", 6, 108, 240, &lv_font_montserrat_14, palette::kTextSecondary);
        createLabel(c3, "DEADLINE MISSES: -- (DEMO)", 6, 134, 240, &lv_font_montserrat_14, palette::kTextSecondary);

        // Card 4: I2S & PCM5102A Output
        lv_obj_t* c4 = lv_obj_create(parent);
        applyPanelStyle(c4, false);
        lv_obj_set_pos(c4, 8, 216);
        lv_obj_set_size(c4, 256, 168);
        createLabel(c4, "4. I2S & PCM5102A", 6, 4, 170, &lv_font_montserrat_14, palette::kAccentPrimary);
        createStatusBadge(c4, FeatureStatus::Demo, 178, 4);
        createLabel(c4, d.i2s_status, 6, 30, 240, &lv_font_montserrat_14, palette::kWarning);
        createLabel(c4, "CANDIDATE: BCK=50 WS=51 DIN=52", 6, 56, 240, &lv_font_montserrat_14, palette::kTextPrimary);
        createLabel(c4, "FORMAT: 48kHz 16-BIT STEREO", 6, 82, 240, &lv_font_montserrat_14, palette::kTextSecondary);
        createLabel(c4, "DMA UNDERRUNS: UNMEASURED", 6, 108, 240, &lv_font_montserrat_14, palette::kTextSecondary);
        createLabel(c4, "WIRING GATE: OFF IN DEMO", 6, 134, 240, &lv_font_montserrat_14, palette::kTextSecondary);

        // Card 5: USB-MIDI Host
        lv_obj_t* c5 = lv_obj_create(parent);
        applyPanelStyle(c5, false);
        lv_obj_set_pos(c5, 272, 216);
        lv_obj_set_size(c5, 256, 168);
        createLabel(c5, "5. USB-MIDI HOST", 6, 4, 170, &lv_font_montserrat_14, palette::kAccentPrimary);
        createStatusBadge(c5, FeatureStatus::Demo, 178, 4);
        createLabel(c5, d.usb_status, 6, 30, 240, &lv_font_montserrat_14, palette::kWarning);
        createLabel(c5, "PORT: P4 HS/FS (OPT-IN GATE)", 6, 56, 240, &lv_font_montserrat_14, palette::kTextPrimary);
        createLabel(c5, "PARSE ERRORS: -- (DEMO)", 6, 82, 240, &lv_font_montserrat_14, palette::kTextSecondary);
        createLabel(c5, "QUEUE DROPS: -- (DEMO)", 6, 108, 240, &lv_font_montserrat_14, palette::kTextSecondary);
        createLabel(c5, "HOT-PLUG DRAIN: READY IN FW", 6, 134, 240, &lv_font_montserrat_14, palette::kActive);

        // Card 6: ESP32-P4 Memory & Display
        lv_obj_t* c6 = lv_obj_create(parent);
        applyPanelStyle(c6, false);
        lv_obj_set_pos(c6, 536, 216);
        lv_obj_set_size(c6, 256, 168);
        createLabel(c6, "6. SYSTEM & BOARD", 6, 4, 170, &lv_font_montserrat_14, palette::kAccentPrimary);
        createStatusBadge(c6, FeatureStatus::Demo, 178, 4);
        createLabel(c6, "BOARD: GUITION JC-ESP32P4-M3", 6, 30, 240, &lv_font_montserrat_14, palette::kTextPrimary);
        createLabel(c6, "DISPLAY: 800x480 HORIZONTAL", 6, 56, 240, &lv_font_montserrat_14, palette::kTextPrimary);
        createLabel(c6, "INT RAM / PSRAM: TARGET ONLY", 6, 82, 240, &lv_font_montserrat_14, palette::kTextSecondary);
        createLabel(c6, "NVS WRITES: 0 (STRICT RAM)", 6, 108, 240, &lv_font_montserrat_14, palette::kActive);
        createLabel(c6, backend.model().lastNotice(), 6, 134, 240, &lv_font_montserrat_14, palette::kAccentPrimary);

        return true;
    }

    void refresh(UiDemoBackend&) override {}
};

} // namespace

ScreenView* createDiagnosticsScreen() {
    return new DiagnosticsScreen();
}

} // namespace smk::p4::ui
