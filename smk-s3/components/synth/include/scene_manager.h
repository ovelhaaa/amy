#pragma once
#include <cstdint>
#include <array>
#include <cstring>
#include "scene_types.h"

namespace smk {

class PatchManager;
class ClockManager;
class Arpeggiator;
class StepSequencer;
class PadManager;
class StorageManager;
class UIManager;

class SceneManager {
public:
    static constexpr size_t kMaxScenes = 8;

    SceneManager();

    bool begin(PatchManager* patch_mgr,
               ClockManager* clock_mgr,
               Arpeggiator* arp,
               StepSequencer* seq,
               PadManager* pad_mgr,
               StorageManager* storage,
               UIManager* ui_mgr);

    /**
     * @brief Select and activate a scene by index (0..7).
     * @param index Scene slot (0..7)
     * @param immediate If true, activates immediately; if false, queues for the next musical boundary.
     */
    bool selectScene(uint8_t index, bool immediate = true);

    /**
     * @brief Queue a scene to be activated on the next musical boundary (beat / pattern end).
     */
    void queueScene(uint8_t index);

    /**
     * @brief Check and execute pending scene transition if queued.
     */
    void processPendingTransition();

    void nextScene();
    void previousScene();

    /**
     * @brief Capture the live synth, clock, sequencer, and pad states into a scene slot.
     */
    void captureCurrentAsScene(uint8_t index, const char* custom_name = nullptr);

    bool saveSceneToFlash(uint8_t index);
    bool loadSceneFromFlash(uint8_t index);
    bool saveAllToFlash();
    bool loadAllFromFlash();

    uint8_t activeSceneIndex() const { return active_scene_index_; }
    int8_t pendingSceneIndex() const { return pending_scene_index_; }

    const Scene& scene(uint8_t index) const;
    const Scene& activeScene() const;

    void setSceneName(uint8_t index, const char* name);

private:
    void initFactoryScenes();
    void applySceneToSystem(const Scene& sc);

    PatchManager*   patch_manager_   = nullptr;
    ClockManager*   clock_manager_   = nullptr;
    Arpeggiator*    arpeggiator_     = nullptr;
    StepSequencer*  sequencer_       = nullptr;
    PadManager*     pad_manager_     = nullptr;
    StorageManager* storage_manager_ = nullptr;
    UIManager*      ui_manager_      = nullptr;

    std::array<Scene, kMaxScenes> scenes_;
    uint8_t active_scene_index_ = 0;
    int8_t  pending_scene_index_ = -1;
};

} // namespace smk
