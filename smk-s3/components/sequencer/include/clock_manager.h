#pragma once
#include <cstdint>
#include <atomic>
#include <mutex>
#include "clock_timing.h"
#include "esp_timer.h"

namespace smk {

enum class ClockSource : uint8_t {
    Internal = 0,
    UsbMidi
};

class ClockManager {
public:
    static constexpr uint32_t kPpqn = 24; // 24 pulses per quarter note (standard MIDI clock)

    ClockManager();
    ~ClockManager();

    bool begin();
    void setBpm(float bpm);
    float bpm() const { return bpm_.load(std::memory_order_relaxed); }

    void setSwing(uint8_t swing_percent);
    uint8_t swing() const { return swing_percent_.load(std::memory_order_relaxed); }

    void setClockSource(ClockSource source);
    ClockSource clockSource() const { return clock_source_.load(std::memory_order_relaxed); }

    void start();
    void stop();
    bool isRunning() const { return running_.load(std::memory_order_relaxed); }

    // Single control-task consumer. Skipped edges are reported so musical
    // triggers can be dropped while release deadlines still get serviced.
    bool takePendingTick(uint32_t& tick, uint32_t& skipped, uint64_t& timestamp_us) {
        TemporalGuard guard(timing_lock_);
        return pending_ticks_.consume(tick, skipped, timestamp_us);
    }

    uint64_t tickPeriodUs() {
        TemporalGuard guard(timing_lock_);
        return clockSource() == ClockSource::UsbMidi && external_period_average_us_
            ? external_period_average_us_ : static_cast<uint64_t>(60000000.0 / bpm() / kPpqn);
    }
    bool takePendingTick(TickSnapshot& tick) {
        TemporalGuard guard(timing_lock_);
        return pending_ticks_.consume(tick);
    }
    void onExternalTick(uint64_t timestamp_us = 0);
    void onExternalStart(uint64_t timestamp_us = 0);
    void onExternalStop();

private:
    friend struct ClockManagerTestAccess;
    static void timerCallback(void* arg);
    void updateTimerPeriod();

    // Task-side API calls (including console) serialize timer API operations.
    // Callback takes only timing_lock_; no timer APIs or task mutex under it.
    std::recursive_mutex api_mutex_;
    TemporalLock timing_lock_;
    uint64_t next_tick_us_ = 0;
    std::atomic<float>      bpm_{120.0f};
    std::atomic<uint8_t>    swing_percent_{50}; // 50% = Straight, 67% = Triplet, 75% = Dotted
    std::atomic<ClockSource> clock_source_{ClockSource::Internal};
    std::atomic<bool>       running_{false};
    std::atomic<uint32_t>   tick_count_{0};
    LatestTickMailbox       pending_ticks_;
    std::atomic<uint64_t>   previous_tick_us_{0};
    uint64_t                external_epoch_us_ = 0; // Temporal lock.
    uint64_t                previous_external_tick_us_ = 0; // Control-task owner.
    uint64_t                external_period_average_us_ = 0;
    esp_timer_handle_t      timer_handle_ = nullptr;
};

} // namespace smk
