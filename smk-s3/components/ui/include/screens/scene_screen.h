#pragma once

#include "screen_base.h"
#include <cstdint>
#include <array>

namespace smk {

class SceneManager;

class SceneScreen : public ScreenBase {
public:
    SceneScreen();
    ~SceneScreen() override = default;

    void update() override;
    void render(DisplayDriver& display) override;
    const char* name() const override { return "Scenes"; }

    void setSceneManager(const SceneManager* scene_mgr) { scene_mgr_ = scene_mgr; }

private:
    const SceneManager* scene_mgr_{nullptr};

    uint8_t active_scene_index_{0};
    int8_t  pending_scene_index_{-1};
    std::array<char[24], 8> scene_names_;
    std::array<uint8_t, 8>  scene_patches_{1, 2, 3, 4, 5, 6, 7, 8};
    std::array<float, 8>    scene_bpms_{120.0f, 120.0f, 124.0f, 124.0f, 118.0f, 126.0f, 110.0f, 115.0f};
    std::array<bool, 8>     scene_arps_{true, false, true, true, true, false, true, false};
    std::array<uint8_t, 8>  scene_patterns_{0, 1, 2, 3, 4, 5, 6, 7};
};

} // namespace smk
