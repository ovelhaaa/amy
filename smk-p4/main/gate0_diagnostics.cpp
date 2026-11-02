#include "gate0_diagnostics.h"
#include "app_config.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_psram.h"
#include "sdkconfig.h"
#include <cinttypes>

namespace smk::p4 {
static const char* TAG = "SMK_P4_GATE0";

bool reportHardware() {
    esp_chip_info_t chip{};
    esp_chip_info(&chip);
#if defined(CONFIG_SMKP4_STANDALONE)
    ESP_LOGI(TAG, "Standalone local AMY/PCM5102A; I2S requires confirmed wiring");
#elif defined(CONFIG_SMK_LINK_ENABLED)
    ESP_LOGI(TAG, "Rehearsal Performance controller; synthesis/audio remain on S3");
#else
    ESP_LOGI(TAG, "Gate 0/1 display diagnostic; UART disabled");
#endif
    ESP_LOGI(TAG, "IDF=%s model=%d revision=%u.%u cores=%u reset_reason=%d",
             esp_get_idf_version(), static_cast<int>(chip.model),
             static_cast<unsigned>(chip.revision / 100),
             static_cast<unsigned>(chip.revision % 100),
             static_cast<unsigned>(chip.cores), static_cast<int>(esp_reset_reason()));
    ESP_LOGI(TAG, "Image revision range=%d..%d (hundredths)",
             CONFIG_ESP_REV_MIN_FULL, CONFIG_ESP_REV_MAX_FULL);
    uint32_t flash_size_bytes = 0;
    const esp_err_t result = esp_flash_get_size(nullptr, &flash_size_bytes);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Flash size query failed: %s", esp_err_to_name(result));
        return false;
    }
    ESP_LOGI(TAG, "Detected Flash=%" PRIu32 " bytes", flash_size_bytes);
    ESP_LOGI(TAG, "Physical initialized PSRAM=%zu bytes", esp_psram_get_size());
    ESP_LOGI(TAG, "Profile: JC4880P443C_I_W from user's P4SDM; chip queried independently");
    ESP_LOGI(TAG, "Display target=%ux%u; P4 audio driver: NOT INITIALIZED",
             static_cast<unsigned>(config::kIntendedDisplayWidth),
             static_cast<unsigned>(config::kIntendedDisplayHeight));
    return true;
}

void reportMemory() {
    constexpr uint32_t internal_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    constexpr uint32_t psram_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    ESP_LOGI(TAG, "heap internal free=%zu min=%zu largest=%zu; PSRAM total=%zu free=%zu min=%zu largest=%zu",
             heap_caps_get_free_size(internal_caps), heap_caps_get_minimum_free_size(internal_caps),
             heap_caps_get_largest_free_block(internal_caps), heap_caps_get_total_size(psram_caps),
             heap_caps_get_free_size(psram_caps), heap_caps_get_minimum_free_size(psram_caps),
             heap_caps_get_largest_free_block(psram_caps));
}
}
