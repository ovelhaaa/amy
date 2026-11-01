#pragma once

#include <atomic>
#include <cstdint>

namespace smk {

// Tick deadlines are always scheduled less than 2^31 ticks ahead. Signed
// subtraction therefore remains valid when the 32-bit musical clock wraps.
constexpr bool tickReached(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}

// Clock edges are state, not a backlog of commands. Keep only the newest edge:
// the consumer can release notes whose deadlines passed, but must not replay
// stale note triggers in a burst after it was busy.
class LatestTickMailbox {
public:
    void publish(uint32_t tick, uint64_t timestamp_us = 0) {
        timestamp_us_.store(timestamp_us, std::memory_order_relaxed);
        latest_.store(tick, std::memory_order_release);
    }

    bool consume(uint32_t& tick, uint32_t& skipped, uint64_t& timestamp_us) {
        uint32_t latest = latest_.load(std::memory_order_acquire);
        const uint32_t previous = consumed_;
        if (latest == previous) return false;
        timestamp_us = timestamp_us_.load(std::memory_order_relaxed);
        const uint32_t confirm = latest_.load(std::memory_order_acquire);
        if (confirm != latest) {
            latest = confirm;
            timestamp_us = timestamp_us_.load(std::memory_order_relaxed);
        }
        skipped = latest - previous - 1;
        consumed_ = latest;
        tick = latest;
        return true;
    }

    void reset(uint32_t tick = 0) {
        timestamp_us_.store(0, std::memory_order_relaxed);
        latest_.store(tick, std::memory_order_release);
        consumed_ = tick;
    }

private:
    std::atomic<uint32_t> latest_{0};
    std::atomic<uint64_t> timestamp_us_{0};
    uint32_t consumed_ = 0; // Single control-task consumer.
};

} // namespace smk
