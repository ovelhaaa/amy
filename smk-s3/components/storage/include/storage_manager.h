#pragma once
#include <cstdint>
#include <cstddef>
#include "patch_types.h"
#include "controller_profile.h"
#include "scene_types.h"

namespace smk {

class StorageManager {
public:
    static constexpr size_t kMaxSlots = 128;
    static constexpr const char* kMountPath = "/spiffs";

    StorageManager();
    ~StorageManager();

    bool begin();
    bool isMounted() const { return mounted_; }

    /**
     * @brief Save a patch struct atomically to Flash SPIFFS slot
     * @param slot_id Slot index (0..127)
     * @param patch Patch data to save
     * @return True if saved and verified successfully
     */
    bool savePatch(uint8_t slot_id, const SynthPatch& patch);

    /**
     * @brief Load and verify patch struct from Flash SPIFFS slot
     * @param slot_id Slot index (0..127)
     * @param patch_out Output patch struct
     * @return True if loaded and CRC32 verified successfully
     */
    bool loadPatch(uint8_t slot_id, SynthPatch& patch_out);

    /**
     * @brief Check if a patch exists at slot_id
     */
    bool patchExists(uint8_t slot_id) const;

    /**
     * @brief Save controller profile (.s3m) to Flash SPIFFS
     */
    bool saveProfile(const char* name, const ControllerProfile& profile);

    /**
     * @brief Load controller profile (.s3m) from Flash SPIFFS
     */
    bool loadProfile(const char* name, ControllerProfile& profile_out);

    /**
     * @brief Save scene (.s3s) to Flash SPIFFS
     */
    bool saveScene(const char* name, const Scene& scene);

    /**
     * @brief Load scene (.s3s) from Flash SPIFFS
     */
    bool loadScene(const char* name, Scene& scene_out);

    size_t totalBytes() const { return total_bytes_; }
    size_t usedBytes() const { return used_bytes_; }

private:
    void getSlotPath(uint8_t slot_id, char* path_out, size_t max_len, const char* ext = ".s3p") const;
    void updateStorageStats();

    bool   mounted_ = false;
    size_t total_bytes_ = 0;
    size_t used_bytes_ = 0;
};

} // namespace smk
