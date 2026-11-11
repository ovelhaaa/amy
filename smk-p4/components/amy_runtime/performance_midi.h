#pragma once

#include "midi_control_events.h"

namespace smk::p4::performance {

struct MidiSource {
    uint8_t cable = 0, channel = 0, controller = 0;

    bool operator==(const MidiSource& other) const {
        return cable == other.cable && channel == other.channel && controller == other.controller;
    }
};

enum class LearnState : uint8_t { Idle, Armed, Captured, Confirming, Active, Cancelled, Timeout };

class MidiLearn {
public:
    static constexpr uint32_t kTimeoutMs = 10000;

    void arm(uint32_t now_ms, uint32_t generation, uint8_t channel, bool allow_mod_wheel) {
        started_ms_ = now_ms;
        generation_ = generation;
        channel_ = channel;
        allow_mod_wheel_ = allow_mod_wheel;
        state_ = LearnState::Armed;
    }

    void service(uint32_t now_ms, uint32_t generation) {
        if (!pending()) return;
        if (generation != generation_) state_ = LearnState::Cancelled;
        else if (now_ms - started_ms_ >= kTimeoutMs) state_ = LearnState::Timeout;
    }

    bool capture(const amy::MidiControlEvent& event, uint32_t now_ms, uint32_t generation) {
        service(now_ms, generation);
        if (state_ != LearnState::Armed || event.device_generation != generation ||
            event.timestamp_ms - started_ms_ >= kTimeoutMs || event.channel >= 16 ||
            (channel_ < 16 && event.channel != channel_) || event.cable != 0 ||
            event.value > 127 || !amy::observableController(event.controller) ||
            (event.controller == 1 && !allow_mod_wheel_)) return false;
        candidate_ = {event.cable, event.channel, event.controller};
        state_ = LearnState::Captured;
        return true;
    }

    bool confirm(uint32_t now_ms, uint32_t generation) {
        service(now_ms, generation);
        if (state_ != LearnState::Captured) return false;
        state_ = LearnState::Confirming;
        return true;
    }

    void resolve(bool installed, uint32_t now_ms, uint32_t generation) {
        service(now_ms, generation);
        if (state_ == LearnState::Confirming)
            state_ = installed ? LearnState::Active : LearnState::Captured;
    }

    void cancel() {
        if (pending()) state_ = LearnState::Cancelled;
    }
    LearnState state() const { return state_; }
    MidiSource candidate() const { return candidate_; }

private:
    bool pending() const {
        return state_ == LearnState::Armed || state_ == LearnState::Captured ||
            state_ == LearnState::Confirming;
    }

    LearnState state_ = LearnState::Idle;
    MidiSource candidate_{};
    uint32_t started_ms_ = 0, generation_ = 0;
    uint8_t channel_ = 16;
    bool allow_mod_wheel_ = false;
};

enum class PickupState : uint8_t { Waiting, ApproachingFromBelow, ApproachingFromAbove, Captured };

class Pickup {
public:
    static constexpr int kTolerance = 3;

    void reset() { state_ = PickupState::Waiting; }

    // At the capture edge the first output is the target. A sparse CC stream
    // may cross it by many steps, so forwarding that first sample would jump.
    bool update(uint8_t physical, uint8_t target, uint8_t& output) {
        if (physical > 127 || target > 127) return false;
        if (state_ == PickupState::Captured) {
            output = physical;
            return true;
        }
        const int difference = int(physical) - int(target);
        if (state_ == PickupState::Waiting) {
            if (difference == 0) return capture(target, output);
            state_ = difference < 0 ? PickupState::ApproachingFromBelow : PickupState::ApproachingFromAbove;
            extreme_ = physical;
            return false;
        }
        const bool below = state_ == PickupState::ApproachingFromBelow;
        const bool crossing = below ? difference >= 0 : difference <= 0;
        const bool approaching = below ? difference >= -kTolerance && int(physical) - extreme_ >= kTolerance
                                       : difference <= kTolerance && extreme_ - int(physical) >= kTolerance;
        if (crossing || approaching) return capture(target, output);
        if (below ? physical < extreme_ : physical > extreme_) extreme_ = physical;
        return false;
    }

    PickupState state() const { return state_; }

private:
    bool capture(uint8_t target, uint8_t& output) {
        state_ = PickupState::Captured;
        output = target;
        return true;
    }

    PickupState state_ = PickupState::Waiting;
    uint8_t extreme_ = 0;
};

}
