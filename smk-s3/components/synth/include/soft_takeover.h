#pragma once
#include "patch_types.h"
#include "screens/parameter_screen.h" // For TakeoverStatus enum definition
#include <array>
#include <cmath>

namespace smk {

enum class TakeoverMode : uint8_t {
    Pickup = 0,        // Default: wait until physical knob crosses saved value
    Jump = 1,          // Instant jump to physical knob value
    Relative = 2,      // Relative offset tracking
    ScaledCatchUp = 3  // Scaled catch-up speed proportional to distance
};

class SoftTakeover {
public:
    static constexpr float kTolerance = 3.5f; // Tolerance window for 0..127 scale
    static constexpr size_t kMaxTrackedParams = 64;

    struct ParamState {
        float          saved_val = 0.0f;
        float          last_physical_val = 0.0f;
        TakeoverStatus status = TakeoverStatus::Captured;
        bool           initialized = false;
    };

    SoftTakeover();

    void setMode(TakeoverMode mode) { mode_ = mode; }
    TakeoverMode mode() const { return mode_; }

    /**
     * @brief Process a physical knob movement for a parameter.
     * @param param_id Identifier for the parameter
     * @param physical_val Current physical position of the knob (0.0 .. 127.0)
     * @param saved_val Target/saved value in the patch (0.0 .. 127.0)
     * @param out_effective_val Output parameter that receives the effective value to apply
     * @return TakeoverStatus Captured, TakeoverLower, or TakeoverHigher
     */
    TakeoverStatus update(uint8_t param_id, float physical_val, float saved_val, float& out_effective_val);

    /**
     * @brief Reset capture state for a single parameter (e.g., after loading a new patch)
     */
    void reset(uint8_t param_id, float new_saved_val);

    /**
     * @brief Reset capture state for all parameters
     */
    void resetAll();

private:
    TakeoverMode mode_ = TakeoverMode::Pickup;
    std::array<ParamState, kMaxTrackedParams> states_;
};

} // namespace smk
