#pragma once
#include <cstdint>
#include "patch_types.h"
#include "pad_bank.h"

namespace smk {

constexpr uint32_t kSceneMagic = 0x53335331; // "S3S1"
constexpr uint16_t kSceneFormatVersion = 1;

struct SceneHeader {
    uint32_t magic;          // 0x53335331
    uint16_t format_version; // Version 1
    uint16_t data_size;      // Payload data size
    uint32_t crc32;          // Checksum
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
    uint32_t    crc32;
};

uint32_t calculateSceneCrc32(const Scene& scene);

} // namespace smk
