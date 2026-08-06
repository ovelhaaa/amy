#include "soft_takeover.h"
#include <algorithm>

namespace smk {

SoftTakeover::SoftTakeover() {
    resetAll();
}

TakeoverStatus SoftTakeover::update(uint8_t param_id, float physical_val, float saved_val, float& out_effective_val) {
    if (param_id >= kMaxTrackedParams) {
        out_effective_val = physical_val;
        return TakeoverStatus::Captured;
    }

    auto& state = states_[param_id];

    // Direct Jump Mode: immediate capture
    if (mode_ == TakeoverMode::Jump) {
        state.saved_val = physical_val;
        state.last_physical_val = physical_val;
        state.status = TakeoverStatus::Captured;
        state.initialized = true;
        out_effective_val = physical_val;
        return TakeoverStatus::Captured;
    }

    if (!state.initialized) {
        state.saved_val = saved_val;
        state.last_physical_val = physical_val;
        state.initialized = true;

        if (std::abs(physical_val - saved_val) <= kTolerance) {
            state.status = TakeoverStatus::Captured;
        } else if (physical_val < saved_val) {
            state.status = TakeoverStatus::ApproachingFromBelow;
        } else {
            state.status = TakeoverStatus::ApproachingFromAbove;
        }
    }

    // Relative Mode: apply physical delta directly to saved value
    if (mode_ == TakeoverMode::Relative) {
        float delta = physical_val - state.last_physical_val;
        state.saved_val = std::clamp(state.saved_val + delta, 0.0f, 127.0f);
        state.last_physical_val = physical_val;
        state.status = TakeoverStatus::Captured;
        out_effective_val = state.saved_val;
        return TakeoverStatus::Captured;
    }

    // Scaled Catch Up Mode: catch up faster as physical knob moves closer
    if (mode_ == TakeoverMode::ScaledCatchUp && state.status != TakeoverStatus::Captured) {
        float delta = physical_val - state.last_physical_val;
        float distance = std::abs(state.saved_val - physical_val);
        float scale = 1.0f - std::clamp(distance / 127.0f, 0.0f, 0.8f);
        state.saved_val = std::clamp(state.saved_val + delta * scale, 0.0f, 127.0f);
        state.last_physical_val = physical_val;
        if (std::abs(physical_val - state.saved_val) <= kTolerance) {
            state.status = TakeoverStatus::Captured;
        }
        out_effective_val = state.saved_val;
        return state.status;
    }

    // Default Pickup Mode logic
    if (state.saved_val != saved_val && state.status == TakeoverStatus::Captured) {
        state.saved_val = saved_val;
    }

    if (state.status == TakeoverStatus::Captured) {
        state.saved_val = physical_val;
        state.last_physical_val = physical_val;
        out_effective_val = physical_val;
        return TakeoverStatus::Captured;
    }

    // Check if physical knob crossed or entered the tolerance window of the saved value
    bool crossed = false;
    if (state.status == TakeoverStatus::ApproachingFromBelow && physical_val >= (saved_val - kTolerance)) {
        crossed = true;
    } else if (state.status == TakeoverStatus::ApproachingFromAbove && physical_val <= (saved_val + kTolerance)) {
        crossed = true;
    }

    if (crossed || std::abs(physical_val - saved_val) <= kTolerance) {
        state.status = TakeoverStatus::Captured;
        state.saved_val = physical_val;
        state.last_physical_val = physical_val;
        out_effective_val = physical_val;
        return TakeoverStatus::Captured;
    }

    // Still not captured: output saved value to prevent sudden jumps
    state.last_physical_val = physical_val;
    out_effective_val = state.saved_val;
    return state.status;
}

void SoftTakeover::reset(uint8_t param_id, float new_saved_val) {
    if (param_id < kMaxTrackedParams) {
        states_[param_id].saved_val = new_saved_val;
        states_[param_id].status = TakeoverStatus::ApproachingFromBelow;
        states_[param_id].initialized = false;
    }
}

void SoftTakeover::resetAll() {
    for (auto& s : states_) {
        s.saved_val = 0.0f;
        s.last_physical_val = 0.0f;
        s.status = TakeoverStatus::ApproachingFromBelow;
        s.initialized = false;
    }
}

} // namespace smk
