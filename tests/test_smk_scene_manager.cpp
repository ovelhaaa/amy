#include <cstdio>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <array>
#include <vector>

namespace smk {

constexpr uint32_t kSceneMagic = 0x53335331; // "S3S1"
constexpr uint16_t kSceneFormatVersion = 1;

enum class SceneTransition : uint8_t {
    Immediate    = 0,
    NextBeat     = 1,
    EndOfPattern = 2
};

struct Scene {
    char        name[24];
    uint8_t     patch_id;
    float       bpm;
    uint8_t     knob_bank;
    uint8_t     pad_bank;
    float       macro_values[8];
    bool        arp_enabled;
    uint8_t     arp_mode;
    uint8_t     arp_division;
    uint8_t     arp_octaves;
    bool        arp_latch;
    bool        seq_playing;
    uint8_t     drum_pattern;     // Sequencer Pattern (0..7)
    uint8_t     drum_mutes;       // Bitmask: bit 0=BD, 1=SD, 2=CH, 3=OH
    uint8_t     transition_mode;  // 0=Immediate, 1=NextBeat, 2=EndOfPattern
    uint32_t    crc32;
};

// CRC32 calculation mock
static inline uint32_t calculateSceneCrc32(const Scene& scene) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(&scene);
    size_t length = sizeof(Scene) - sizeof(uint32_t);
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

class SceneManager {
public:
    static constexpr size_t kMaxScenes = 8;

    SceneManager() {
        initFactoryScenes();
    }

    void initFactoryScenes() {
        const char* default_names[kMaxScenes] = {
            "01: INTRO",
            "02: VERSE",
            "03: PRE-CHORUS",
            "04: CHORUS",
            "05: BRIDGE",
            "06: SOLO",
            "07: BREAKDOWN",
            "08: OUTRO"
        };

        for (size_t i = 0; i < kMaxScenes; ++i) {
            auto& sc = scenes_[i];
            memset(&sc, 0, sizeof(Scene));
            snprintf(sc.name, sizeof(sc.name), "%s", default_names[i]);
            sc.patch_id = static_cast<uint8_t>(i + 1);
            sc.bpm = (i == 2 || i == 3) ? 124.0f : ((i == 4) ? 118.0f : ((i == 6) ? 110.0f : 120.0f));
            sc.knob_bank = 0;
            sc.pad_bank = 0;
            for (uint8_t m = 0; m < 8; ++m) sc.macro_values[m] = 50.0f;
            sc.arp_enabled = (i == 0 || i == 2 || i == 4 || i == 6);
            sc.arp_mode = 0;
            sc.arp_division = 0;
            sc.arp_octaves = 1;
            sc.arp_latch = false;
            sc.seq_playing = (i > 0 && i < 7);
            sc.drum_pattern = static_cast<uint8_t>(i % 8);
            sc.drum_mutes = (i == 0 || i == 6) ? 0x01 : 0x00;
            sc.transition_mode = static_cast<uint8_t>(SceneTransition::EndOfPattern);
            sc.crc32 = calculateSceneCrc32(sc);
        }
    }

    bool selectScene(uint8_t index, bool immediate = true) {
        if (index >= kMaxScenes) return false;
        if (immediate) {
            active_scene_index_ = index;
            pending_scene_index_ = -1;
        } else {
            queueScene(index);
        }
        return true;
    }

    void queueScene(uint8_t index) {
        if (index >= kMaxScenes) return;
        pending_scene_index_ = static_cast<int8_t>(index);
    }

    void processPendingTransition() {
        if (pending_scene_index_ >= 0 && pending_scene_index_ < static_cast<int8_t>(kMaxScenes)) {
            uint8_t next_idx = static_cast<uint8_t>(pending_scene_index_);
            pending_scene_index_ = -1;
            selectScene(next_idx, true);
        }
    }

    void captureCurrentAsScene(uint8_t index, uint8_t patch_id, float bpm, uint8_t drum_pattern, uint8_t drum_mutes, const char* name = nullptr) {
        if (index >= kMaxScenes) return;
        auto& sc = scenes_[index];
        if (name) snprintf(sc.name, sizeof(sc.name), "%s", name);
        sc.patch_id = patch_id;
        sc.bpm = bpm;
        sc.drum_pattern = drum_pattern;
        sc.drum_mutes = drum_mutes;
        sc.crc32 = calculateSceneCrc32(sc);
    }

    uint8_t activeSceneIndex() const { return active_scene_index_; }
    int8_t pendingSceneIndex() const { return pending_scene_index_; }
    const Scene& scene(uint8_t index) const { return scenes_[index < kMaxScenes ? index : 0]; }
    const Scene& activeScene() const { return scenes_[active_scene_index_]; }

private:
    std::array<Scene, kMaxScenes> scenes_;
    uint8_t active_scene_index_ = 0;
    int8_t  pending_scene_index_ = -1;
};

} // namespace smk

int main() {
    printf("=== Running SMK-S3 Scene Manager Unit Tests ===\n");

    smk::SceneManager mgr;

    // 1. Verify 8 Factory Scenes initialization
    assert(mgr.activeSceneIndex() == 0);
    assert(strcmp(mgr.activeScene().name, "01: INTRO") == 0);
    assert(mgr.activeScene().patch_id == 1);
    assert(mgr.activeScene().bpm == 120.0f);
    assert(mgr.activeScene().drum_mutes == 0x01); // Kick muted in intro
    assert(strcmp(mgr.scene(3).name, "04: CHORUS") == 0);
    assert(mgr.scene(3).bpm == 124.0f);
    printf("[PASS] Factory scenes initialization verified.\n");

    // 2. Test Immediate Scene Selection
    mgr.selectScene(3, true); // Chorus
    assert(mgr.activeSceneIndex() == 3);
    assert(mgr.pendingSceneIndex() == -1);
    assert(strcmp(mgr.activeScene().name, "04: CHORUS") == 0);
    printf("[PASS] Immediate scene selection verified.\n");

    // 3. Test Quantized Scene Queueing & Execution
    mgr.selectScene(5, false); // Queue Solo (Scene 6)
    assert(mgr.activeSceneIndex() == 3); // Still in Chorus!
    assert(mgr.pendingSceneIndex() == 5); // Queued
    printf("[PASS] Quantized scene queueing verified.\n");

    // Simulate musical boundary (beat/pattern end)
    mgr.processPendingTransition();
    assert(mgr.activeSceneIndex() == 5);
    assert(mgr.pendingSceneIndex() == -1);
    assert(strcmp(mgr.activeScene().name, "06: SOLO") == 0);
    printf("[PASS] Quantized transition execution verified.\n");

    // 4. Test Live State Capture
    mgr.captureCurrentAsScene(7, 12, 132.0f, 4, 0x02, "MY CUSTOM OUTRO");
    assert(mgr.scene(7).patch_id == 12);
    assert(mgr.scene(7).bpm == 132.0f);
    assert(mgr.scene(7).drum_pattern == 4);
    assert(mgr.scene(7).drum_mutes == 0x02);
    assert(strcmp(mgr.scene(7).name, "MY CUSTOM OUTRO") == 0);
    printf("[PASS] Live state capture into Scene #8 verified.\n");

    // 5. Test CRC32 Integrity Calculation
    uint32_t crc1 = mgr.scene(7).crc32;
    uint32_t crc2 = smk::calculateSceneCrc32(mgr.scene(7));
    assert(crc1 == crc2);
    assert(crc1 != 0);
    printf("[PASS] CRC32 scene checksum calculation verified (0x%08X).\n", crc1);

    printf("=== ALL UNIT TESTS PASSED SUCCESSFULLY! ===\n");
    return 0;
}
