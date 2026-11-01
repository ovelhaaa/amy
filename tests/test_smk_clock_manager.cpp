#include "clock_manager.h"
#include "diagnostics.h"
#include <cassert>
#include <thread>
#include <initializer_list>
namespace smk {
Diagnostics& Diagnostics::instance() { static Diagnostics d; return d; }
DiagnosticCounters& Diagnostics::counters() { return counters_; }
struct ClockManagerTestAccess {
    static void fire(ClockManager& c) { ClockManager::timerCallback(&c); }
    static void rollover(ClockManager& c) {
        TemporalGuard guard(c.timing_lock_);
        c.tick_count_ = UINT32_MAX - 1;
        c.pending_ticks_.reset(UINT32_MAX - 1);
    }
};
}
int main() {
    using namespace smk;
    ClockManager c;
    assert(c.begin());
    uint32_t tick, skipped; uint64_t time;
    for (float bpm : {30.f, 60.f, 120.f, 180.f, 300.f}) {
        c.stop(); c.setBpm(bpm); c.start();
        uint64_t period = static_cast<uint64_t>(60000000.0 / bpm / 24);
        ClockManagerTestAccess::fire(c);
        assert(!c.takePendingTick(tick, skipped, time));
        smk_test_time_us += period * 10;
        ClockManagerTestAccess::fire(c);
        assert(c.takePendingTick(tick, skipped, time));
        assert(tick == 10 && skipped == 9 && time == uint64_t(smk_test_time_us.load()));
        c.setBpm(bpm); // No reset/duplicate of musical position.
        smk_test_time_us += period;
        ClockManagerTestAccess::fire(c);
        assert(c.takePendingTick(tick, skipped, time) && tick == 11);
        c.stop(); ClockManagerTestAccess::fire(c);
        assert(!c.takePendingTick(tick, skipped, time));
    }
    c.start(); ClockManagerTestAccess::rollover(c);
    smk_test_time_us += 16666;
    ClockManagerTestAccess::fire(c);
    assert(c.takePendingTick(tick, skipped, time) && tick == 0 && skipped == 1);
    c.setClockSource(ClockSource::UsbMidi);
    ClockManagerTestAccess::fire(c);
    assert(!c.takePendingTick(tick, skipped, time));
    c.onExternalStart(); c.onExternalTick(1); // Queued before this source epoch.
    assert(!c.takePendingTick(tick, skipped, time));
    c.onExternalTick(12345678);
    assert(c.takePendingTick(tick, skipped, time) && tick == 1 && time == 12345678);
    c.onExternalStop(); c.onExternalTick(12345679);
    assert(!c.takePendingTick(tick, skipped, time));
    const uint64_t before_restart = static_cast<uint64_t>(smk_test_time_us.load());
    c.stop(); smk_test_time_us += 1000; c.start(); // Generic Play while externally clocked.
    c.onExternalTick(before_restart);
    assert(!c.takePendingTick(tick, skipped, time));
    c.onExternalTick(static_cast<uint64_t>(smk_test_time_us.load()));
    assert(c.takePendingTick(tick, skipped, time) && tick == 1);
    std::thread callbacks([&] { for (int i=0; i<10000; ++i) ClockManagerTestAccess::fire(c); });
    for (int i=0; i<1000; ++i) {
        c.stop(); c.setClockSource(ClockSource::Internal); c.start();
        c.setBpm(120); c.setClockSource(ClockSource::UsbMidi); c.onExternalStart();
    }
    c.stop(); callbacks.join();
    assert(!c.takePendingTick(tick, skipped, time));
}
