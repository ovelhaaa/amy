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

    // ── 1. Clean Save of Initial Preset ─────────────────────────────────────
    assert(backend.setPatch(4, 0));
    runtime.apply(0);
    runtime.loadPendingPatch();
    assert(runtime.render());
    backend.publish(runtime.snapshot(), false, 0, 0);

    assert(backend.editParameter(edit::Cutoff, 1200.0f));
    assert(backend.setFxParameter("fx.chorus.mode", 2.0f, 10));
    backend.service(25);
    runtime.apply(25);
    backend.publish(runtime.snapshot(), false, 0, 0);

    assert(backend.editAction(2, 0, "PRESET ONE"));
    runtime.apply(50);
    assert(runtime.snapshot().storage_quiet);
    backend.publish(runtime.snapshot(), false, 0, 0);

    backend.service(75);
    assert(!backend.editError());
    assert(std::strcmp(backend.editNotice(), "SAVED") == 0);

    runtime.apply(75);
    backend.publish(runtime.snapshot(), false, 0, 0);
    assert(!backend.busy());
    assert(std::strcmp(backend.state().patch_name, "PRESET ONE") == 0);

    // ── 2. Injected FX Storage Failure During Save ──────────────────────────
    // Modify parameters
    assert(backend.editParameter(edit::Cutoff, 2400.0f));
    assert(backend.setFxParameter("fx.reverb.mix", 75.0f, 100));
    backend.service(125);
    runtime.apply(125);
    backend.publish(runtime.snapshot(), false, 0, 0);

    // Incur fault during FX write
    backend.transactionCoordinator().setFaultInjection(preset::FaultInjection::DuringFxWrite);

    assert(backend.editAction(2, 0, "FAILED PRESET"));
    runtime.apply(150);
    backend.publish(runtime.snapshot(), false, 0, 0);

    backend.service(175);
    // Verified: Save must fail cleanly and report error
    assert(backend.editError());
    assert(std::strstr(backend.editNotice(), "SAVE FAILED") != nullptr);
    assert(std::strcmp(backend.editNotice(), "SAVED") != 0);

    runtime.apply(175);
    backend.publish(runtime.snapshot(), false, 0, 0);
    assert(!backend.busy());

    // Verified: Patch name must NOT be overwritten with the unpersisted name
    assert(std::strcmp(backend.state().patch_name, "FAILED PRESET") != 0);
    assert(std::strcmp(backend.state().patch_name, "PRESET ONE") == 0);

    // Verified: Storage still holds the intact original PRESET ONE
    edit::Preset verify_p{};
    fx::FxConfig verify_fx{};
    assert(backend.transactionCoordinator().load(0, verify_p, verify_fx));
    assert(std::strcmp(verify_p.name, "PRESET ONE") == 0);
    assert(verify_p.values[edit::Cutoff] == 1200.0f);
    assert(verify_fx.chorus_mode == 2);

    backend.transactionCoordinator().setFaultInjection(preset::FaultInjection::None);

    // ── 3. Quiet Barrier Timeout Handling ───────────────────────────────────
    assert(backend.editAction(2, 0, "TIMEOUT PRESET"));
    // We intentionally DO NOT update snapshot with storage_quiet = true
    // Simulate elapsed time exceeding quiet barrier timeout (2000 ms)
    backend.service(2500);

    assert(backend.editError());
    assert(std::strcmp(backend.editNotice(), "SAVE QUIET BARRIER TIMEOUT") == 0);
    assert(!backend.busy());
    assert(!runtime.commands().storage_requested.load()); // Audio barrier cleared

    runtime.end();
    std::puts("PASS GATE C: Save As success/failure semantics, unpersisted name rejection & barrier timeout verified");
    return 0;
}
