#include "amy_runtime.h"
#include <cassert>
#include <cstdio>

using namespace smk::p4;

int main() {
    amy::Runtime runtime;
    assert(runtime.begin());

    const uint32_t gen0 = runtime.commands().generation();
    assert(gen0 >= 1);

    // ── 1. Valid Fresh Command Matching Generation ──────────────────────────
    fx::FxRequest req1{};
    req1.config.chorus_mode = 2; // Juno I
    req1.config.chorus_depth = 0.5f;
    req1.generation = gen0;
    req1.request = 101;
    assert(runtime.commands().fx.push(req1));

    runtime.apply(0);
    assert(runtime.snapshot().fx.config.chorus_mode == 2);
    assert(runtime.snapshot().fx.request == 101);
    assert(runtime.snapshot().fx.rejected == 0);
    assert(runtime.snapshot().fx.errors == 0);

    // ── 2. Stale Command Rejection (Past Generation) ────────────────────────
    fx::FxRequest req_stale{};
    req_stale.config.chorus_mode = 4; // Ensemble
    req_stale.generation = gen0 - 1;   // Stale generation!
    req_stale.request = 102;
    assert(runtime.commands().fx.push(req_stale));

    runtime.apply(20);
    // Verified: Stale command must NOT be applied to AMY bus
    assert(runtime.snapshot().fx.config.chorus_mode == 2); // Retains Juno I
    assert(runtime.snapshot().fx.request == 101);          // Request 102 was dropped
    assert(runtime.snapshot().fx.rejected == 1);           // Rejected counter incremented

    // ── 3. Panic Increments Generation and Invalidates Stamped Commands ─────
    runtime.commands().panic();
    const uint32_t gen1 = runtime.commands().generation();
    assert(gen1 > gen0);

    // Submit request stamped with pre-panic generation gen0
    fx::FxRequest req_pre_panic{};
    req_pre_panic.config.delay_mix = 0.8f;
    req_pre_panic.generation = gen0; // obsolete!
    req_pre_panic.request = 103;
    assert(runtime.commands().fx.push(req_pre_panic));

    runtime.apply(40);
    assert(runtime.snapshot().fx.config.delay_mix == 0.0f); // Default 0.0, not updated
    assert(runtime.snapshot().fx.rejected == 2);           // Rejected counter incremented again!

    // ── 4. New Command With Current Post-Panic Generation Accepted ───────────
    fx::FxRequest req_new{};
    req_new.config.delay_mix = 0.4f;
    req_new.generation = gen1; // Current generation!
    req_new.request = 104;
    assert(runtime.commands().fx.push(req_new));

    runtime.apply(60);
    assert(runtime.snapshot().fx.config.delay_mix == 0.4f);
    assert(runtime.snapshot().fx.request == 104);
    assert(runtime.snapshot().fx.rejected == 2);           // No new rejections

    runtime.end();
    std::puts("PASS GATE F: Stale FX generation rejection, Panic invalidation & rejection diagnostic counters verified");
    return 0;
}
