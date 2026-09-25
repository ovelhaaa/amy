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

    bool begin(const char* base_path = nullptr);
    bool isMounted() const { return mounted_; }

    void setBasePath(const char* path);
    const char* basePath() const { return base_path_; }

    /**
     * @brief Save a patch struct atomically to Flash SPIFFS slot
     *
     * The slot index (0..127) is a user storage location and lives only in the
     * filename. SynthPatch::id is the patch's own identity and is preserved
     * verbatim; it is never overwritten with the slot. See
     * docs/patch_storage_semantics.md.
     *
     * @param slot_id Slot index (0..127), independent of SynthPatch::id
     * @param patch Patch data to save
     * @return True if saved and verified successfully
     */
    bool savePatch(uint8_t slot_id, const SynthPatch& patch);

    /**
     * @brief Load and verify patch struct from Flash SPIFFS slot
     *
     * The returned patch keeps the identity stored in the file. The caller
     * (PatchManager) records the slot separately via applyLoadedPatch(patch,
     * slot_id).
     *
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
    char   base_path_[64] = "/spiffs";
    size_t total_bytes_ = 0;
    size_t used_bytes_ = 0;
};

} // namespace smk
