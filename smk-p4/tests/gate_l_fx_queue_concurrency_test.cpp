#include "amy_runtime.h"
#include <cassert>
#include <cstdio>
#include <thread>
#include <atomic>
#include <chrono>

using namespace smk::p4;

int main() {
    std::puts("=== GATE C: SPSC FX Queue Real Concurrency Test ===");

    for (int run = 1; run <= 3; ++run) {
        printf("--- Running concurrency pass %d (Lifecycle & Restart) ---\n", run);
        amy::Runtime runtime;
        assert(runtime.begin());

        std::atomic<bool> producer_done{false};
        std::atomic<uint32_t> total_pushed{0};
        std::atomic<uint32_t> total_full_drops{0};
        std::atomic<uint32_t> panics_count{0};
        std::atomic<uint32_t> retired{0};
        const bool drain_before_panic=run==3;
        const auto baseline=runtime.snapshot().fx;

        constexpr int kTotalRequests = 30000;

        // Producer Thread (simulating Control Task / UI)
        std::thread producer([&]() {
            uint32_t req_id = 0;
            for (int i = 0; i < kTotalRequests; ++i) {
                // Every 600 requests, simulate a Panic
                if ((i % 600) == 599) {
                    // A producer may legally see an empty queue at every Panic.
                    if(drain_before_panic)
                        while(retired.load(std::memory_order_acquire)!=total_pushed.load())
                            std::this_thread::yield();
                    runtime.commands().panic();
                    panics_count.fetch_add(1, std::memory_order_relaxed);
                }

                fx::FxRequest req{};
                req.config.chorus_mode = static_cast<uint8_t>(1 + (i % 5));
                req.config.chorus_depth = 0.1f + 0.00002f * (i % 10000);
                req.config.delay_time_ms = 100.0f + static_cast<float>(i % 500);
                req.config.delay_mix = 0.25f;
                req.generation = runtime.commands().generation();
                req.request = ++req_id;

                if (runtime.commands().fx.push(req)) {
                    total_pushed.fetch_add(1, std::memory_order_relaxed);
                    // Establish one known applied command before stressing
                    // the scheduler; a slow consumer may otherwise retire
                    // a full queue of only stale commands after the last Panic.
                    if(i==0)
                        while(retired.load(std::memory_order_acquire)==0)
                            std::this_thread::yield();
                } else {
                    total_full_drops.fetch_add(1, std::memory_order_relaxed);
                }

                // Micro-yield occasionally to interleave execution
                if ((i % 50) == 0) {
                    std::this_thread::yield();
                }
            }
            producer_done.store(true, std::memory_order_release);
        });

        // Consumer Thread (simulating Audio Task)
        uint32_t consumer_applied = 0;
        uint32_t last_applied_request = 0;
        uint32_t now_ms = 0;
        const auto publishRetired=[&](const fx::FxState& snapshot) {
            retired.store(snapshot.revision-baseline.revision+snapshot.rejected-baseline.rejected,
                          std::memory_order_release);
        };

        while (!producer_done.load(std::memory_order_acquire)) {
            runtime.apply(now_ms);
            now_ms += 1;
            auto snap = runtime.snapshot();
            publishRetired(snap.fx);
            if (snap.fx.request > last_applied_request) {
                last_applied_request = snap.fx.request;
                ++consumer_applied;
            }
            if(run==2) std::this_thread::sleep_for(std::chrono::microseconds(1));
            else std::this_thread::yield();
        }

        producer.join();

        // Drain any remaining items after producer finishes
        for (int i = 0; i < 50; ++i) {
            runtime.apply(now_ms);
            now_ms += 1;
            auto snap = runtime.snapshot();
            if (snap.fx.request > last_applied_request) {
                last_applied_request = snap.fx.request;
                ++consumer_applied;
            }
        }

        auto final_snap = runtime.snapshot();
        printf("  Pass %d stats:\n", run);
        printf("    Total Pushed:       %u\n", total_pushed.load());
        printf("    Queue Full Drops:   %u\n", total_full_drops.load());
        printf("    Panics Injected:    %u\n", panics_count.load());
        printf("    Stale Rejections:   %u\n", final_snap.fx.rejected);
        printf("    Consumer Applied:   %u\n", consumer_applied);
        printf("    Errors (Corrupt):   %u\n", final_snap.fx.errors);

        // Invariants verification
        assert(final_snap.fx.errors == 0); // ZERO corrupted slots read
        assert(total_pushed.load()+total_full_drops.load()==kTotalRequests);
        const auto applied=final_snap.fx.revision-baseline.revision;
        const auto stale=final_snap.fx.rejected-baseline.rejected;
        assert(applied+stale==total_pushed.load());
        if(drain_before_panic) assert(stale==0);
        assert(consumer_applied > 0);
        assert(fx::valid(final_snap.fx.config));

        // An explicitly queued old-generation request must be rejected without
        // changing the last applied request/configuration.
        fx::FxRequest stale_request{};
        stale_request.request=kTotalRequests+1;
        stale_request.config=final_snap.fx.config;
        stale_request.generation=runtime.commands().generation();
        assert(runtime.commands().fx.push(stale_request));
        runtime.commands().panic();
        runtime.apply(now_ms);
        assert(runtime.snapshot().fx.rejected==final_snap.fx.rejected+1);
        assert(runtime.snapshot().fx.request==final_snap.fx.request);

        runtime.end();
        printf("  PASS: Pass %d completed with zero corruption, no races, and clean shutdown\n", run);
    }

    std::puts("ALL GATE C CONCURRENCY TESTS PASSED.");
    return 0;
}
