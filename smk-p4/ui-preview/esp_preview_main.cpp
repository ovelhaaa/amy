// Isolated ESP-IDF Preview Entry Point for SMK-P4 UI Blueprint 1.0
// Reuses existing display_test(false) LVGL + ST7701S + GT911 initialization
// without duplicating display drivers or touching smk-p4/main/ protected files.

#include "ui_blueprint.h"
#include "display_test.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
constexpr const char* kTag = "UI_BLUEPRINT_PREVIEW";
smk::p4::ui::UiBlueprint g_blueprint;
} // namespace

extern "C" void app_main(void) {
    ESP_LOGI(kTag, "Starting SMK-P4 UI Blueprint 1.0 isolated preview (800x480)");
    if (!smk::p4::beginDisplayTest(false)) {
        ESP_LOGE(kTag, "Display initialization failed");
        return;
    }

    if (!g_blueprint.begin(lv_screen_active())) {
        ESP_LOGE(kTag, "UiBlueprint initialization failed");
        return;
    }

    ESP_LOGI(kTag, "UiBlueprint 1.0 mounted on active 800x480 screen (UI DEMO backend)");
    while (true) {
        const uint32_t now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
        g_blueprint.service(now_ms);
        smk::p4::serviceDisplayTest();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
