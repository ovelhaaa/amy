#include "user_preset_nvs_v2.h"
#include "user_preset_fx_nvs.h"
#include "local_performance_backend.h"
#include "fake_nvs.h"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace smk::p4;

int main() {
    fake_nvs::blobs.clear();
    fake_nvs::free_entries = 700;

    amy::Runtime runtime;
    assert(runtime.begin());

    NvsVersionedPresets synth_store;
    assert(synth_store.begin());

    fx::NvsFxPresets fx_store;
    assert(fx_store.begin());

    LocalAmyPerformanceBackend backend(runtime.commands());
    backend.presetStore(&synth_store);
    backend.fxPresetStore(&fx_store);
    backend.publish(runtime.snapshot(), false, 0, 0);

    // ── 1. Initial State: Factory Original ──────────────────────────────────
    assert(backend.setPatch(4, 0));
    runtime.apply(0);
    runtime.loadPendingPatch();
    assert(runtime.render());
    backend.publish(runtime.snapshot(), false, 0, 0);

    assert(!backend.unsaved());
    assert(std::strcmp(backend.editNotice(), "FACTORY ORIGINAL") == 0);

    // ── 2. Modify Synth Only: EDITED / UNSAVED ──────────────────────────────
    assert(backend.editParameter(edit::Cutoff, 1500.0f));
    backend.service(25);
    runtime.apply(25);
    backend.publish(runtime.snapshot(), false, 0, 0);

    assert(backend.unsaved());
    assert(std::strcmp(backend.editNotice(), "EDITED / UNSAVED") == 0);

    // Revert restores Factory Original
    assert(backend.editAction(1)); // Revert
    backend.service(50);
    runtime.apply(50);
    backend.publish(runtime.snapshot(), false, 0, 0);

    assert(!backend.unsaved());
    assert(std::strcmp(backend.editNotice(), "FACTORY ORIGINAL") == 0);

    // ── 3. Modify FX Only: FX EDITED / UNSAVED & Block Patch Change ─────────
    assert(backend.setFxParameter("fx.chorus.mode", 2.0f, 60));
    backend.service(75);
    runtime.apply(75);
    backend.publish(runtime.snapshot(), false, 0, 0);

    // Verified: FX modifications participate in unsaved state
    assert(backend.unsaved());
    assert(std::strcmp(backend.editNotice(), "FX EDITED / UNSAVED") == 0);

    // Verified: Switching patch while FX is unsaved must be BLOCKED
    assert(!backend.setPatch(8, 80));
    assert(backend.state().patch_id == 4); // Still patch 4

    // ── 4. Compare with FX: Baseline vs Edited ──────────────────────────────
    // Turn Compare ON
    assert(backend.editAction(0)); // Compare toggle
    backend.service(90);
    runtime.apply(90);
    backend.publish(runtime.snapshot(), false, 0, 0);

    assert(std::strcmp(backend.editNotice(), "ORIGINAL / EDITS RETAINED") == 0);
    // Owner should receive original neutral chorus mode (0 = Off)
    assert(runtime.snapshot().fx.config.chorus_mode == 0);

    // Turn Compare OFF
    assert(backend.editAction(0));
    backend.service(120);
    runtime.apply(120);
    backend.publish(runtime.snapshot(), false, 0, 0);

    assert(backend.unsaved());
    assert(std::strcmp(backend.editNotice(), "FX EDITED / UNSAVED") == 0);
    // Owner should receive edited chorus mode (2 = Juno I)
    assert(runtime.snapshot().fx.config.chorus_mode == 2);

    // ── 5. Revert Clears FX Unsaved State & Unblocks Patch Change ────────────
    assert(backend.editAction(1)); // Revert
    backend.service(150);
    runtime.apply(150);
    backend.publish(runtime.snapshot(), false, 0, 0);

    assert(!backend.unsaved());
    assert(std::strcmp(backend.editNotice(), "FACTORY ORIGINAL") == 0);
    assert(runtime.snapshot().fx.config.chorus_mode == 0);

    // Now patch change is unblocked!
    assert(backend.setPatch(8, 160));
    runtime.apply(160);
    runtime.loadPendingPatch();
    assert(runtime.render());
    backend.publish(runtime.snapshot(), false, 0, 0);
    assert(backend.state().patch_id == 8);

    // ── 6. Save State and Subsequent Incremental FX Editing ──────────────────
    assert(backend.setFxParameter("fx.delay.mix", 40.0f, 180));
    backend.service(190);
    runtime.apply(190);
    backend.publish(runtime.snapshot(), false, 0, 0);

    // Save as Slot 0: "MY DELAY ORGAN"
    assert(backend.editAction(2, 0, "MY DELAY ORGAN"));
    runtime.apply(200);
    backend.publish(runtime.snapshot(), false, 0, 0);
    backend.service(210);
    runtime.apply(210);
    backend.publish(runtime.snapshot(), false, 0, 0);

    assert(!backend.unsaved());
    assert(std::strcmp(backend.editNotice(), "SAVED") == 0);

    // Modify FX beyond saved state
    assert(backend.setFxParameter("fx.reverb.mix", 50.0f, 240));
    backend.service(250);
    runtime.apply(250);
    backend.publish(runtime.snapshot(), false, 0, 0);

    assert(backend.unsaved());
    assert(std::strcmp(backend.editNotice(), "FX EDITED / UNSAVED") == 0);
    assert(!backend.setPatch(4, 260)); // Blocked

    runtime.end();
    std::puts("PASS GATE D/E: FX Unsaved State, FX EDITED / UNSAVED notification, patch protection & Compare/Revert verified");
    return 0;
}
 
