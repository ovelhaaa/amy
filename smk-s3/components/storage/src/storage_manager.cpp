#include "storage_manager.h"
#include "esp_spiffs.h"
#include "esp_rom_crc.h"
#include "esp_log.h"
#include <cstdio>
#include <cstring>

static const char* TAG = "StorageManager";

namespace smk {

StorageManager::StorageManager() {}

StorageManager::~StorageManager() {
    if (mounted_) {
        esp_vfs_spiffs_unregister("storage");
        mounted_ = false;
    }
}

bool StorageManager::begin() {
    ESP_LOGI(TAG, "Mounting SPIFFS storage partition...");

    esp_vfs_spiffs_conf_t conf = {
        .base_path = kMountPath,
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition 'storage'");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return false;
    }

    mounted_ = true;
    updateStorageStats();

    ESP_LOGI(TAG, "SPIFFS Mounted Successfully: Total=%zu KB, Used=%zu KB", 
             total_bytes_ / 1024, used_bytes_ / 1024);

    return true;
}

void StorageManager::getSlotPath(uint8_t slot_id, char* path_out, size_t max_len, const char* ext) const {
    snprintf(path_out, max_len, "%s/patch_%03d%s", kMountPath, slot_id, ext);
}

void StorageManager::updateStorageStats() {
    if (!mounted_) return;
    esp_spiffs_info("storage", &total_bytes_, &used_bytes_);
}

bool StorageManager::patchExists(uint8_t slot_id) const {
    if (!mounted_) return false;
    char path[64];
    getSlotPath(slot_id, path, sizeof(path));
    FILE* f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

bool StorageManager::savePatch(uint8_t slot_id, const SynthPatch& patch) {
    if (!mounted_) return false;
    if (slot_id >= kMaxSlots) return false;

    char tmp_path[64];
    char s3p_path[64];
    getSlotPath(slot_id, tmp_path, sizeof(tmp_path), ".tmp");
    getSlotPath(slot_id, s3p_path, sizeof(s3p_path), ".s3p");

    SynthPatch p_copy = patch;
    p_copy.id = slot_id;
    p_copy.crc32 = calculatePatchCrc32(p_copy);

    PatchHeader header = {};
    header.magic = kPatchMagic;
    header.format_version = kPatchFormatVersion;
    header.data_size = static_cast<uint16_t>(sizeof(SynthPatch));
    header.crc32 = p_copy.crc32;

    FILE* f = fopen(tmp_path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", tmp_path);
        return false;
    }

    size_t hdr_written = fwrite(&header, 1, sizeof(PatchHeader), f);
    size_t payload_written = fwrite(&p_copy, 1, sizeof(SynthPatch), f);
    fclose(f);

    if (hdr_written != sizeof(PatchHeader) || payload_written != sizeof(SynthPatch)) {
        ESP_LOGE(TAG, "Write size mismatch: hdr %zu/%zu, payload %zu/%zu bytes", 
                 hdr_written, sizeof(PatchHeader), payload_written, sizeof(SynthPatch));
        remove(tmp_path);
        return false;
    }

    // Atomic replacement
    remove(s3p_path);
    if (rename(tmp_path, s3p_path) != 0) {
        ESP_LOGE(TAG, "Failed to rename %s to %s", tmp_path, s3p_path);
        return false;
    }

    updateStorageStats();
    ESP_LOGI(TAG, "Saved Patch Slot #%d [%s] (.s3p) to Flash (CRC32: 0x%08X)", 
             slot_id, p_copy.name, p_copy.crc32);

    return true;
}

bool StorageManager::loadPatch(uint8_t slot_id, SynthPatch& patch_out) {
    if (!mounted_) return false;
    if (slot_id >= kMaxSlots) return false;

    char s3p_path[64];
    getSlotPath(slot_id, s3p_path, sizeof(s3p_path), ".s3p");

    FILE* f = fopen(s3p_path, "rb");
    if (!f) {
        // Fallback check for legacy .bin format
        char bin_path[64];
        getSlotPath(slot_id, bin_path, sizeof(bin_path), ".bin");
        f = fopen(bin_path, "rb");
        if (!f) {
            ESP_LOGW(TAG, "Patch slot #%d does not exist on Flash", slot_id);
            return false;
        }
    }

    PatchHeader header = {};
    size_t hdr_read = fread(&header, 1, sizeof(PatchHeader), f);

    if (hdr_read == sizeof(PatchHeader) && header.magic == kPatchMagic) {
        if (header.format_version != kPatchFormatVersion) {
            ESP_LOGE(TAG, "Incompatible patch format version: %u on slot #%d", header.format_version, slot_id);
            fclose(f);
            return false;
        }

        SynthPatch loaded_patch = {};
        size_t read_bytes = fread(&loaded_patch, 1, sizeof(SynthPatch), f);
        fclose(f);

        if (read_bytes != sizeof(SynthPatch)) {
            ESP_LOGE(TAG, "Corrupted patch payload on slot #%d (bytes read %zu != %zu)", 
                     slot_id, read_bytes, sizeof(SynthPatch));
            return false;
        }

        uint32_t computed_crc = calculatePatchCrc32(loaded_patch);
        if (computed_crc != header.crc32 || computed_crc != loaded_patch.crc32) {
            ESP_LOGE(TAG, "Patch CRC32 mismatch on slot #%d! Computed 0x%08X != Header 0x%08X",
                     slot_id, computed_crc, header.crc32);
            return false;
        }

        patch_out = loaded_patch;
        ESP_LOGI(TAG, "Loaded Patch Slot #%d [%s] (.s3p) from Flash (Magic & CRC32 Verified)", slot_id, patch_out.name);
        return true;
    }

    // Legacy binary fallback read without header
    fseek(f, 0, SEEK_SET);
    SynthPatch legacy_patch = {};
    size_t legacy_read = fread(&legacy_patch, 1, sizeof(SynthPatch), f);
    fclose(f);

    if (legacy_read == sizeof(SynthPatch)) {
        uint32_t computed_crc = calculatePatchCrc32(legacy_patch);
        if (computed_crc == legacy_patch.crc32) {
            patch_out = legacy_patch;
            ESP_LOGI(TAG, "Loaded legacy Patch Slot #%d [%s] from Flash", slot_id, patch_out.name);
            return true;
        }
    }

    ESP_LOGE(TAG, "Failed to load patch slot #%d: Invalid format or header", slot_id);
    return false;
}

bool StorageManager::saveProfile(const char* name, const ControllerProfile& profile) {
    if (!mounted_ || !name) return false;

    char path[64];
    snprintf(path, sizeof(path), "%s/%s.s3m", kMountPath, name);

    ControllerProfile prof_copy = profile;
    prof_copy.crc32 = calculateProfileCrc32(prof_copy);

    ProfileHeader header = {};
    header.magic = kProfileMagic;
    header.format_version = kProfileFormatVersion;
    header.data_size = static_cast<uint16_t>(sizeof(ControllerProfile));
    header.crc32 = prof_copy.crc32;

    FILE* f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open profile file for writing: %s", path);
        return false;
    }

    fwrite(&header, 1, sizeof(ProfileHeader), f);
    fwrite(&prof_copy, 1, sizeof(ControllerProfile), f);
    fclose(f);

    updateStorageStats();
    ESP_LOGI(TAG, "Saved Controller Profile [%s] (.s3m) to Flash (CRC32: 0x%08X)", 
             name, prof_copy.crc32);
    return true;
}

bool StorageManager::loadProfile(const char* name, ControllerProfile& profile_out) {
    if (!mounted_ || !name) return false;

    char path[64];
    snprintf(path, sizeof(path), "%s/%s.s3m", kMountPath, name);

    FILE* f = fopen(path, "rb");
    if (!f) {
        ESP_LOGW(TAG, "Profile [%s] does not exist on Flash", name);
        return false;
    }

    ProfileHeader header = {};
    size_t hdr_read = fread(&header, 1, sizeof(ProfileHeader), f);

    if (hdr_read == sizeof(ProfileHeader) && header.magic == kProfileMagic) {
        ControllerProfile loaded = {};
        size_t read_bytes = fread(&loaded, 1, sizeof(ControllerProfile), f);
        fclose(f);

        if (read_bytes == sizeof(ControllerProfile)) {
            uint32_t computed_crc = calculateProfileCrc32(loaded);
            if (computed_crc == header.crc32 && computed_crc == loaded.crc32) {
                profile_out = loaded;
                ESP_LOGI(TAG, "Loaded Controller Profile [%s] (.s3m) from Flash", name);
                return true;
            }
        }
    }
    fclose(f);
    ESP_LOGE(TAG, "Failed to load Controller Profile [%s]", name);
    return false;
}

uint32_t calculateSceneCrc32(const Scene& scene) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(&scene);
    size_t length = offsetof(Scene, crc32);
    return esp_rom_crc32_le(0, data, length);
}

bool StorageManager::saveScene(const char* name, const Scene& scene) {
    if (!mounted_) return false;
    char path[128];
    snprintf(path, sizeof(path), "%s/scene_%s.s3s", kMountPath, name);

    FILE* f = fopen(path, "wb");
    if (!f) return false;

    Scene sc = scene;
    sc.crc32 = calculateSceneCrc32(sc);

    SceneHeader header = {
        .magic = kSceneMagic,
        .format_version = kSceneFormatVersion,
        .data_size = sizeof(Scene),
        .crc32 = sc.crc32
    };

    if (fwrite(&header, sizeof(SceneHeader), 1, f) != 1 ||
        fwrite(&sc, sizeof(Scene), 1, f) != 1) {
        fclose(f);
        return false;
    }

    fclose(f);
    updateStorageStats();
    ESP_LOGI(TAG, "Saved Scene [%s] (.s3s) to Flash", name);
    return true;
}

bool StorageManager::loadScene(const char* name, Scene& scene_out) {
    if (!mounted_) return false;
    char path[128];
    snprintf(path, sizeof(path), "%s/scene_%s.s3s", kMountPath, name);

    FILE* f = fopen(path, "rb");
    if (!f) return false;

    SceneHeader header = {};
    if (fread(&header, sizeof(SceneHeader), 1, f) == 1 && header.magic == kSceneMagic) {
        Scene loaded = {};
        size_t read_bytes = fread(&loaded, 1, sizeof(Scene), f);
        fclose(f);

        if (read_bytes == sizeof(Scene)) {
            uint32_t computed_crc = calculateSceneCrc32(loaded);
            if (computed_crc == header.crc32 && computed_crc == loaded.crc32) {
                scene_out = loaded;
                ESP_LOGI(TAG, "Loaded Scene [%s] (.s3s) from Flash", name);
                return true;
            }
        }
    }
    fclose(f);
    ESP_LOGE(TAG, "Failed to load Scene [%s]", name);
    return false;
}

} // namespace smk
