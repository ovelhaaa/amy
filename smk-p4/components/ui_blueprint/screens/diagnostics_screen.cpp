#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>

namespace smk::p4::ui {

namespace {

class DiagnosticsScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::Diagnostics; }

    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager&) override {
        const auto& d = backend.model().diagnostics();
        if(!backend.isDemoMode()) {
            createLabel(parent,"LIVE SNAPSHOTS | UNMEASURED VALUES ARE N/A | TOUCH LEASE 10s",8,4,784,&lv_font_montserrat_14,palette::kAccentPrimary);
            for(unsigned i=0;i<16;++i) live_[i]=createLabel(parent,"",8+(i/8)*396,42+(i%8)*40,388,&lv_font_montserrat_14,palette::kTextPrimary);
            refresh(backend); return true;
        }

        // 1. Explicit UI DEMO Telemetry Honesty Banner (Section 5 Tela 14 requirement)
        lv_obj_t* banner = lv_obj_create(parent);
        applyPanelStyle(banner, true);
        lv_obj_set_pos(banner, 8, 4);
        lv_obj_set_size(banner, 784, 34);
        lv_obj_set_style_pad_all(banner, 4, 0);
        createLabel(banner,
                    "DIAGNOSTICS  |  UI DEMO ACTIVE: HARDWARE TELEMETRY IS NOT FABRICATED (REAL LVGL HEAP)",
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
        char c1_b1[64], c1_b2[64], c1_b3[64], c1_b4[64];
        std::snprintf(c1_b1, sizeof(c1_b1), "UPTIME: %lu ms", static_cast<unsigned long>(d.uptime_ms));
        std::snprintf(c1_b2, sizeof(c1_b2), "NAV SWITCHES: %lu", static_cast<unsigned long>(d.screen_switches));
        std::snprintf(c1_b3, sizeof(c1_b3), "DEMO PANICS: %lu", static_cast<unsigned long>(d.demo_panic_count));
        std::snprintf(c1_b4, sizeof(c1_b4), "PARAMS REGISTERED: %u", static_cast<unsigned>(ParameterRegistry::count()));
        createLabel(c1, c1_b1, 6, 56, 240, &lv_font_montserrat_14, palette::kTextSecondary);
        createLabel(c1, c1_b2, 6, 82, 240, &lv_font_montserrat_14, palette::kTextSecondary);
        createLabel(c1, c1_b3, 6, 108, 240, &lv_font_montserrat_14, palette::kTextSecondary);
        createLabel(c1, c1_b4, 6, 134, 240, &lv_font_montserrat_14, palette::kActive);

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
        createLabel(c3, "DSP RENDER: UNMEASURED", 6, 108, 240, &lv_font_montserrat_14, palette::kTextSecondary);
        createLabel(c3, "DEADLINE MISSES: -- (DEMO)", 6, 134, 240, &lv_font_montserrat_14, palette::kTextSecondary);

        // Card 4: I2S & PCM5102A Output
        lv_obj_t* c4 = lv_obj_create(parent);
        applyPanelStyle(c4, false);
        lv_obj_set_pos(c4, 8, 216);
        lv_obj_set_size(c4, 256, 168);
        createLabel(c4, "4. I2S & PCM5102A", 6, 4, 170, &lv_font_montserrat_14, palette::kAccentPrimary);
        createStatusBadge(c4, FeatureStatus::Demo, 178, 4);
        createLabel(c4, d.i2s_status, 6, 30, 240, &lv_font_montserrat_14, palette::kWarning);
        createLabel(c4, "PINS: BCK=50 WS=51 DIN=52", 6, 56, 240, &lv_font_montserrat_14, palette::kTextPrimary);
        createLabel(c4, "FORMAT: 48kHz 16-BIT STEREO", 6, 82, 240, &lv_font_montserrat_14, palette::kTextSecondary);
        createLabel(c4, "UNDERRUNS: UNMEASURED", 6, 108, 240, &lv_font_montserrat_14, palette::kTextSecondary);
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
        createLabel(c6, backend.statusText(), 6, 134, 240, &lv_font_montserrat_14, palette::kAccentPrimary);

        return true;
    }

    void refresh(IUiBackend& backend) override {
        if(backend.isDemoMode() || !live_[0]) return;
        const auto& d=backend.model().diagnostics();
        lv_label_set_text(live_[0],d.firmware_version);
        lv_label_set_text(live_[1],d.amy_status);
        lv_label_set_text_fmt(live_[2],"I2S: %s | USB: %s",d.i2s_status,d.usb_status);
        lv_label_set_text_fmt(live_[3],"VOICES: %u/%u (logical gates)",d.active_voices,d.max_voices);
        lv_label_set_text_fmt(live_[4],"RENDER MAX: %lu / %lu us",(unsigned long)d.render_max_us,(unsigned long)d.budget_us);
        lv_label_set_text_fmt(live_[5],"RENDER DEADLINES: %lu",(unsigned long)d.deadline_misses);
        lv_label_set_text(live_[6],"DMA UNDERRUNS / METERS: N/A");
        lv_label_set_text_fmt(live_[7],"UPTIME: %lu ms | NAV: %lu",(unsigned long)d.uptime_ms,(unsigned long)d.screen_switches);
        lv_label_set_text_fmt(live_[8],"LVGL FREE: %lu",(unsigned long)d.lvgl_free_bytes);
        lv_label_set_text_fmt(live_[9],"LVGL LARGEST: %lu",(unsigned long)d.lvgl_largest_block);
        lv_label_set_text_fmt(live_[10],"LVGL PEAK: %lu FRAG: %u%%",(unsigned long)d.lvgl_peak_used,d.lvgl_frag_pct);
        lv_label_set_text_fmt(live_[11],"INTERNAL FREE: %lu",(unsigned long)d.free_internal_ram);
        lv_label_set_text_fmt(live_[12],"PSRAM FREE: %lu",(unsigned long)d.free_psram);
        if(!d.memory_valid) {
            lv_label_set_text(live_[8],"LVGL FREE: N/A (WAIT SAMPLE)");
            lv_label_set_text(live_[9],"LVGL LARGEST: N/A");
            lv_label_set_text(live_[10],"LVGL PEAK / FRAG: N/A");
            lv_label_set_text(live_[11],"INTERNAL FREE: N/A");
            lv_label_set_text(live_[12],"PSRAM FREE: N/A");
        }
        lv_label_set_text(live_[13],"MORE METRICS: 5s SERIAL LOG");
        lv_label_set_text_fmt(live_[14],"CMD: %s",commandStateLabel(backend.lastCommandState()));
        lv_label_set_text(live_[15],backend.model().lastNotice());
    }
private:
    lv_obj_t* live_[16]{};
};

} // namespace

ScreenView* createDiagnosticsScreen() {
    return new DiagnosticsScreen();
}

} // namespace smk::p4::ui
