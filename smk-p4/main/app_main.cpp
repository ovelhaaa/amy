#include "app_config.h"
#include "gate0_diagnostics.h"
#include "display_test.h"
#include "p4_link.h"
#include "performance_ui.h"
#include "remote_performance_backend.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main() {
    if (!smk::p4::reportHardware()) {
        ESP_LOGE("SMK_P4_GATE0", "Hardware query failed; Gate 0 remains incomplete");
        return;
    }
#if defined(CONFIG_SMK_LINK_ENABLED)
    if (!smk::p4::beginDisplayTest(false)) return;
    static smk::p4::P4Link link(esp_random());
    link.begin();
    static smk::p4::RemotePerformanceBackend backend(link);
    static smk::p4::PerformanceUi ui(backend);
    if (!ui.begin()) {
        ESP_LOGE("SMK_P4", "Performance UI initialization failed");
        return;
    }
#else
    if (!smk::p4::beginDisplayTest()) return;
#endif
    TickType_t last_report = xTaskGetTickCount();
    for (;;) {
#if defined(CONFIG_SMK_LINK_ENABLED)
        link.service(static_cast<uint32_t>(esp_timer_get_time()/1000));
#endif
        smk::p4::serviceDisplayTest();
#if defined(CONFIG_SMK_LINK_ENABLED)
        ui.service(static_cast<uint32_t>(esp_timer_get_time()/1000));
#endif
        if (xTaskGetTickCount()-last_report >= pdMS_TO_TICKS(smk::p4::config::kDiagnosticsIntervalMs)) {
            last_report = xTaskGetTickCount();
            smk::p4::reportMemory();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
