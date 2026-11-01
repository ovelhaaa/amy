#pragma once

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#else
#include <mutex>
#endif
#include <cstdint>

namespace smk {

// Tick deadlines are always scheduled less than 2^31 ticks ahead. Signed
// subtraction therefore remains valid when the 32-bit musical clock wraps.
constexpr bool tickReached(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}

// One half of a PPQN period is the default trigger lateness budget. It is
// shorter than the minimum one-tick gate; releases are never age-filtered.
struct MusicalTiming {
    uint64_t timestamp_us = 0;
    uint64_t now_us = 0;
    uint64_t period_us = 0;
    uint32_t lateness_numerator = 1, lateness_denominator = 2;
    bool triggerAllowed() const {
        return !period_us || !timestamp_us ||
            (now_us >= timestamp_us && now_us - timestamp_us <
             period_us * lateness_numerator / (lateness_denominator ? lateness_denominator : 1));
    }
    uint64_t releaseDeadline(uint32_t tick, uint32_t deadline_tick) const {
        const uint64_t elapsed = static_cast<uint32_t>(tick - deadline_tick) * period_us;
        return period_us && timestamp_us >= elapsed ? timestamp_us - elapsed : 0;
    }
    uint64_t deadline(uint32_t gate_ticks) const {
        return timestamp_us && period_us ? timestamp_us + period_us * gate_ticks : 0;
    }
};

// Count step boundaries on an extended tick axis (including tick zero).
inline uint64_t stepBoundaries(uint64_t tick, uint32_t ticks_per_step, uint32_t swing_split = 6) {
    if (ticks_per_step != 6) return tick / ticks_per_step + 1;
    return tick / 12 * 2 + (tick % 12 >= swing_split ? 1 : 0) + 1;
}

// Short, fixed-size snapshots only. ESP uses a port critical section, never a
// blocking task mutex. Host uses a mutex so thread tests follow the C++ model.
class TemporalLock {
public:
    void lock() {
#ifdef ESP_PLATFORM
        portENTER_CRITICAL(&mux_);
#else
        mux_.lock();
#endif
    }
    void unlock() {
#ifdef ESP_PLATFORM
        portEXIT_CRITICAL(&mux_);
#else
        mux_.unlock();
#endif
    }
private:
#ifdef ESP_PLATFORM
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
#else
    std::mutex mux_;
#endif
};
class TemporalGuard {
public:
    explicit TemporalGuard(TemporalLock& lock) : lock_(lock) { lock_.lock(); }
    ~TemporalGuard() { lock_.unlock(); }
    TemporalGuard(const TemporalGuard&) = delete;
    TemporalGuard& operator=(const TemporalGuard&) = delete;
private:
    TemporalLock& lock_;
};

struct TickSnapshot {
    uint32_t tick = 0, skipped = 0, generation = 0;
    uint64_t timestamp_us = 0, period_us = 0;
    bool external = false;
};

// Clock edges are state, not a backlog. Tick, time and consumed
// position are copied together; no retry loop or informal non-atomic seqlock.
class LatestTickMailbox {
public:
    void publish(uint32_t tick, uint64_t timestamp_us = 0, uint64_t period_us = 0, bool external = false) {
        TemporalGuard guard(lock_);
        latest_ = tick;
        timestamp_us_ = timestamp_us;
        period_us_ = period_us; external_ = external;
    }
    bool consume(uint32_t& tick, uint32_t& skipped, uint64_t& timestamp_us) {
        TickSnapshot s;
        if (!consume(s)) return false;
        tick = s.tick; skipped = s.skipped; timestamp_us = s.timestamp_us;
        return true;
    }
    bool consume(TickSnapshot& s) {
        TemporalGuard guard(lock_);
        if (latest_ == consumed_) return false;
        s = {latest_, latest_ - consumed_ - 1, generation_, timestamp_us_, period_us_, external_};
        consumed_ = latest_;
        return true;
    }
    void reset(uint32_t tick = 0) {
        TemporalGuard guard(lock_);
        ++generation_;
        latest_ = consumed_ = tick;
        timestamp_us_ = 0;
    }
private:
    TemporalLock lock_;
    uint32_t latest_ = 0, consumed_ = 0;
    uint32_t generation_ = 0;
    uint64_t timestamp_us_ = 0, period_us_ = 0;
    bool external_ = false;
};

} // namespace smk
