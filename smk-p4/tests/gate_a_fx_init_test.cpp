#include "user_preset_nvs.h"
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

    // ── 1. Production Firmware Boot Path Simulation ─────────────────────────
    amy::Runtime runtime;
    assert(runtime.begin());

    static NvsUserPresets user_presets;
    const bool presets_ready = user_presets.begin();
    assert(presets_ready);

    static fx::NvsFxPresets fx_presets;
    const bool fx_ready = fx_presets.begin();
    assert(fx_ready);

    static LocalAmyPerformanceBackend backend(runtime.commands());
    if (presets_ready) backend.presetStore(&user_presets);
    if (fx_ready) backend.fxPresetStore(&fx_presets);

    backend.publish(runtime.snapshot(), false, 0, 0);

    // Verify both storage engines registered and ready
    assert(backend.presetStorageReady());
    assert(backend.fxStorageReady());

    // ── 2. Degraded Mode Simulation (FX Storage Unavailable) ────────────────
    LocalAmyPerformanceBackend degraded_backend(runtime.commands());
    // Only synth store is registered; FX store is unavailable (null)
    degraded_backend.presetStore(&user_presets);
    degraded_backend.publish(runtime.snapshot(), false, 0, 0);

    assert(degraded_backend.presetStorageReady() == false);
    assert(degraded_backend.fxStorageReady() == false);

    // Factory audio & patch selection continue normally
    assert(degraded_backend.setPatch(4, 0));
    runtime.apply(0);
    runtime.loadPendingPatch();
    assert(runtime.render());
    degraded_backend.publish(runtime.snapshot(), false, 0, 0);
    assert(degraded_backend.state().patch_id == 4);

    // Live FX controls still work in real time on AMY Bus 0
    assert(degraded_backend.setFxParameter("fx.chorus.mode", 3.0f, 0));
    degraded_backend.service(20);
    runtime.apply(20);
    assert(runtime.render());
    degraded_backend.publish(runtime.snapshot(), false, 0, 0);
    assert(runtime.snapshot().fx.config.chorus_mode == 3);

    // Attempting Save As must be rejected explicitly
    assert(!degraded_backend.editAction(2, 0, "MY PRESET"));
    assert(degraded_backend.editError());
    assert(std::strcmp(degraded_backend.editNotice(), "SAVE FAILED: FX STORAGE UNAVAILABLE") == 0);

    runtime.end();
    std::puts("PASS GATE A: FX NVS firmware wiring, full readiness and graceful degraded mode verified");
    return 0;
}
