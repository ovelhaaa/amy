#include "nvs_storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "NvsStorage";
static const char* NVS_NAMESPACE = "smk_sys";

namespace smk {

NvsStorage::NvsStorage() {}

NvsStorage::~NvsStorage() {}

bool NvsStorage::begin() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated/new version found. Erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS flash: %s", esp_err_to_name(err));
        return false;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "NVS Storage initialized successfully");
    return true;
}

bool NvsStorage::saveConfig(const SystemConfig& config) {
    if (!initialized_) return false;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(err));
        return false;
    }

    uint32_t bpm_bits;
    memcpy(&bpm_bits, &config.global_bpm, sizeof(float));

    nvs_set_u8(handle, "patch_id", config.active_patch_id);
    nvs_set_u32(handle, "bpm_bits", bpm_bits);
    nvs_set_u8(handle, "volume", config.master_volume);
    nvs_set_u8(handle, "channel", config.midi_channel);

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "System Config Saved to NVS (PatchID:%d, BPM:%.1f)", 
                 config.active_patch_id, config.global_bpm);
        return true;
    }

    ESP_LOGE(TAG, "Failed to commit NVS config: %s", esp_err_to_name(err));
    return false;
}

bool NvsStorage::loadConfig(SystemConfig& config_out) {
    if (!initialized_) return false;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No saved NVS config found. Using default system config.");
        return false;
    }

    uint8_t patch_id = 0;
    uint32_t bpm_bits = 0;
    uint8_t volume = 100;
    uint8_t channel = 0;

    nvs_get_u8(handle, "patch_id", &patch_id);
    if (nvs_get_u32(handle, "bpm_bits", &bpm_bits) == ESP_OK) {
        memcpy(&config_out.global_bpm, &bpm_bits, sizeof(float));
    }
    nvs_get_u8(handle, "volume", &volume);
    nvs_get_u8(handle, "channel", &channel);

    nvs_close(handle);

    config_out.active_patch_id = patch_id;
    config_out.master_volume = volume;
    config_out.midi_channel = channel;

    ESP_LOGI(TAG, "Loaded System Config from NVS (PatchID:%d, BPM:%.1f)", 
             config_out.active_patch_id, config_out.global_bpm);
    return true;
}

} // namespace smk
