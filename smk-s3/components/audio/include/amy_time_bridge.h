#pragma once
#include <cstdint>
#include <limits>

namespace smk {

// Experimental conversion only. No runtime caller enables this bridge yet:
// rendered frames lead I2S/DMA and cannot be anchored to analog playback by
// sampling esp_timer at an arbitrary render boundary.
class AmyTimeBridge {
public:
    struct Result {
        bool valid = false;
        bool late = false;
        bool beyond_lookahead = false;
        uint64_t frame = 0;
        uint32_t amy_time_ms = 0;
    };
    bool anchor(uint64_t monotonic_us, uint64_t rendered_frame,
                uint32_t sample_rate_hz, uint32_t generation,
                uint32_t max_lookahead_frames, bool experimental_enabled = false) {
        enabled_ = experimental_enabled && sample_rate_hz && max_lookahead_frames;
        anchor_us_ = monotonic_us; anchor_frame_ = rendered_frame;
        rate_ = sample_rate_hz; generation_ = generation;
        max_ahead_ = max_lookahead_frames; last_frame_ = rendered_frame;
        return enabled_;
    }
    void invalidate() { enabled_ = false; }
    Result convert(uint64_t timestamp_us, uint64_t current_frame, uint32_t generation) {
        Result r;
        if (!enabled_ || generation != generation_) return r;
        const bool after = timestamp_us >= anchor_us_;
        const uint64_t delta_us = after ? timestamp_us - anchor_us_ : anchor_us_ - timestamp_us;
        const uint64_t seconds = delta_us / 1000000;
        if (seconds > (UINT64_MAX - rate_) / rate_) return r;
        // Convert from the fixed anchor each time. Remainders never accumulate.
        const uint64_t frames = seconds * rate_ + delta_us % 1000000 * rate_ / 1000000;
        uint64_t target;
        if (after) {
            if (frames > UINT64_MAX - anchor_frame_) return r;
            target = anchor_frame_ + frames;
        } else target = frames > anchor_frame_ ? 0 : anchor_frame_ - frames;
        r.late = target < current_frame;
        if (target < current_frame) target = current_frame;
        if (target < last_frame_) target = last_frame_; // Preserve FIFO order.
        if (target - current_frame > max_ahead_) {
            r.beyond_lookahead = true; return r;
        }
        r.frame = target;
        // AMY's event clock is integer milliseconds, not samples. Avoid
        // overflow before narrowing and skip UINT32_MAX, its unset sentinel.
        const uint64_t seconds_out = target / rate_;
        r.amy_time_ms = static_cast<uint32_t>(seconds_out) * 1000u +
            static_cast<uint32_t>(target % rate_ * 1000u / rate_);
        if (r.amy_time_ms == UINT32_MAX) ++r.amy_time_ms;
        last_frame_ = target;
        r.valid = true;
        return r;
    }
private:
    bool enabled_ = false;
    uint64_t anchor_us_ = 0, anchor_frame_ = 0, last_frame_ = 0;
    uint32_t rate_ = 0, generation_ = 0, max_ahead_ = 0;
};
} // namespace smk
