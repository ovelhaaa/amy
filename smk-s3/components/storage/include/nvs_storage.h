#pragma once
#include <cstdint>

namespace smk {

struct SystemConfig {
    uint8_t active_patch_id = 0;
    float   global_bpm = 120.0f;
    uint8_t master_volume = 100;
    uint8_t midi_channel = 0;
    uint8_t velocity_curve = 0; // 0: Linear, 1: Soft, 2: Hard, 3: SCurve, 4: Fixed
    uint8_t swing_percent = 50;  // 50%..75%
    uint8_t soft_limiter = 1;   // 1: Enabled, 0: Disabled
};

class NvsStorage {
public:
    NvsStorage();
    ~NvsStorage();

    bool begin();

    /**
     * @brief Save global system configuration to NVS
     */
    bool saveConfig(const SystemConfig& config);

    /**
     * @brief Load global system configuration from NVS
     */
    bool loadConfig(SystemConfig& config_out);

private:
    bool initialized_ = false;
};

} // namespace smk
