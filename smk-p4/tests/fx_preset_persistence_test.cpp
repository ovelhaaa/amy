#include "user_preset_fx_nvs.h"
#include "user_preset_nvs_v2.h"
#include "local_performance_backend.h"
#include "fake_nvs.h"
#include <cassert>
#include <cstdio>
#include <cmath>
#include <cstring>

using namespace smk::p4;
using namespace smk::p4::fx;

static bool approx(float a, float b, float tol = 0.015f) {
    return std::fabs(a - b) <= tol;
}

int main() {
    fake_nvs::blobs.clear();
    fake_nvs::free_entries = 700;
    fake_nvs::writes = 0;

    // ── 1. NvsFxPresets Basic Lifecycle & Dual-Bank Ping-Pong ───────────────
    NvsFxPresets store;
    assert(store.begin());

    FxConfig cfg1{};
    cfg1.chorus_mode = 2; // Juno I
    cfg1.chorus_depth = 0.65f;
    cfg1.delay_time_ms = 450.0f;
    cfg1.delay_feedback = 0.50f;
    cfg1.delay_mix = 0.30f;
    cfg1.reverb_size = 0.80f;
    cfg1.reverb_mix = 0.40f;
    cfg1.master_tone = 0.25f;

    // Initial load on empty slot returns false
    FxConfig out{};
    assert(!store.loadFx(0, out));

    // Save slot 0 (Bank A)
    assert(store.saveFx(0, cfg1));
    assert(fake_nvs::blobs.count({3, "f00a"}) == 1);
    assert(store.loadFx(0, out));
    assert(out.chorus_mode == 2);
    assert(approx(out.chorus_depth, 0.65f));
    assert(approx(out.delay_time_ms, 450.0f));
    assert(approx(out.delay_mix, 0.30f));
    assert(approx(out.reverb_mix, 0.40f));
    assert(approx(out.master_tone, 0.25f));

    // Save slot 0 second time (Ping-Pong -> Bank B)
    FxConfig cfg2 = cfg1;
    cfg2.chorus_mode = 4; // Ensemble
    cfg2.delay_time_ms = 720.0f;
    assert(store.saveFx(0, cfg2));
    assert(fake_nvs::blobs.count({3, "f00b"}) == 1);
    assert(store.loadFx(0, out));
    assert(out.chorus_mode == 4);
    assert(approx(out.delay_time_ms, 720.0f));

    // Save slot 0 third time (Ping-Pong back -> Bank A)
    cfg1.reverb_mix = 0.85f;
    assert(store.saveFx(0, cfg1));
    assert(store.loadFx(0, out));
    assert(approx(out.reverb_mix, 0.85f));

    // ── 2. Reboot Simulation ────────────────────────────────────────────────
    NvsFxPresets reboot;
    assert(reboot.begin());
    assert(reboot.loadFx(0, out));
    assert(approx(out.reverb_mix, 0.85f));

    // ── 3. Dual-Bank CRC Verification & Rollback ────────────────────────────
    // Corrupt the latest record in Bank A
    auto& bank_a = fake_nvs::blobs.at({3, "f00a"});
    bank_a[10] ^= 0xff; // Corrupt payload byte
    // Should safely roll back to Bank B (which had cfg2 with delay_time_ms = 720)
    assert(reboot.loadFx(0, out));
    assert(out.chorus_mode == 4);
    assert(approx(out.delay_time_ms, 720.0f));

    // Corrupt Bank B as well -> both corrupt -> loadFx returns false (safe rejection)
    auto& bank_b = fake_nvs::blobs.at({3, "f00b"});
    bank_b[31] ^= 0xff; // Corrupt CRC byte
    assert(!reboot.loadFx(0, out));

    // ── 4. Torn-Write Protection ────────────────────────────────────────────
    // Repair Bank B with valid record
    assert(store.saveFx(0, cfg2));
    assert(store.loadFx(0, out) && out.chorus_mode == 4);

    fake_nvs::torn = true;
    assert(!store.saveFx(0, cfg1)); // Torn write fails
    fake_nvs::torn = false;
    // Previous valid record intact
    assert(store.loadFx(0, out) && out.chorus_mode == 4);

    // ── 5. Error & Out-of-Space Handling ────────────────────────────────────
    fake_nvs::free_entries = 120; // Below GC barrier
    assert(!store.saveFx(0, cfg1));
    fake_nvs::free_entries = 700;

    fake_nvs::commit_error = true;
    assert(!store.saveFx(0, cfg1));
    fake_nvs::commit_error = false;
    assert(store.loadFx(0, out));

    // ── 6. Full 16-Slot Saturation ──────────────────────────────────────────
    for (unsigned slot = 0; slot < 16; ++slot) {
        FxConfig c{};
        c.chorus_mode = static_cast<uint8_t>(slot % 6);
        c.delay_time_ms = 100.0f + slot * 50.0f;
        c.reverb_mix = slot * 0.05f;
        assert(store.saveFx(slot, c));
    }
    for (unsigned slot = 0; slot < 16; ++slot) {
        assert(store.loadFx(slot, out));
        assert(out.chorus_mode == (slot % 6));
        assert(approx(out.delay_time_ms, 100.0f + slot * 50.0f));
        assert(approx(out.reverb_mix, slot * 0.05f));
    }

    // ── 7. End-to-End LocalAmyPerformanceBackend Integration ────────────────
    amy::Runtime runtime;
    assert(runtime.begin());

    NvsVersionedPresets synth_store;
    assert(synth_store.begin());

    LocalAmyPerformanceBackend backend(runtime.commands());
    backend.presetStore(&synth_store);
    backend.fxPresetStore(&store);

    backend.publish(runtime.snapshot(), false, 0, 0);

    // Select factory patch 4 (Moving Strings)
    assert(backend.setPatch(4, 0));
    runtime.apply(0);
    runtime.loadPendingPatch();
    assert(runtime.render());
    backend.publish(runtime.snapshot(), false, 0, 0);

    // Edit synthesis parameter: Filter Cutoff
    assert(backend.editParameter(edit::Cutoff, 1200.0f));
    // Edit FX parameters: Chorus Juno I, Delay 550ms, Reverb Mix 45%
    assert(backend.setFxParameter("fx.chorus.mode", 2.0f, 0));
    assert(backend.setFxParameter("fx.delay.time_ms", 550.0f, 0));
    assert(backend.setFxParameter("fx.reverb.mix", 45.0f, 0));

    // Service backend and apply to runtime
    backend.service(25);
    runtime.apply(25);
    assert(runtime.render());
    backend.publish(runtime.snapshot(), false, 0, 0);

    // Save as User Preset slot 5: "FX STRINGS"
    assert(backend.editAction(2, 5, "FX STRINGS"));
    runtime.apply(50);
    assert(runtime.snapshot().storage_quiet);
    backend.publish(runtime.snapshot(), false, 0, 0); // sets quiet_ = true
    backend.service(75); // performs save of both synthesis & FX
    runtime.apply(75); // clears storage_quiet
    backend.publish(runtime.snapshot(), false, 0, 0); // sets quiet_ = false
    assert(!backend.busy());

    // Verify preset name in slot 5
    char name[25];
    uint16_t origin = 0;
    assert(backend.userPreset(5, name, sizeof(name), origin));
    assert(std::strcmp(name, "FX STRINGS") == 0 && origin == 4);

    // Switch to patch 135 (DX7 Piano) - FX defaults to neutral dry
    assert(backend.setPatch(135, 100));
    runtime.apply(100);
    runtime.loadPendingPatch();
    assert(runtime.render());
    backend.publish(runtime.snapshot(), false, 0, 0);
    assert(backend.state().patch_id == 135);

    // Recall slot 5 ("FX STRINGS")
    assert(backend.editAction(3, 5)); // Recall
    backend.service(125);
    runtime.apply(125);
    runtime.loadPendingPatch();
    assert(runtime.render());
    backend.publish(runtime.snapshot(), false, 0, 0);
    backend.service(150);
    runtime.apply(150);
    assert(runtime.render());
    backend.publish(runtime.snapshot(), false, 0, 0);

    // Check that patch 4 is active, cutoff is 1200, AND FX are fully restored!
    assert(backend.state().patch_id == 4);
    assert(runtime.snapshot().edit.applied[edit::Cutoff] == 1200.0f);
    assert(runtime.snapshot().fx.config.chorus_mode == 2);
    assert(approx(runtime.snapshot().fx.config.delay_time_ms, 550.0f));
    assert(approx(runtime.snapshot().fx.config.reverb_mix, 0.45f));

    // ── 8. Backward Compatibility: v1/v2 Preset without FX Record ───────────
    // Save a synthesis-only preset in slot 7 directly in synth_store
    edit::Preset legacy_p{};
    legacy_p.sequence = 1;
    legacy_p.patch = 8; // Juno Organ I
    legacy_p.family = 1;
    std::strcpy(legacy_p.name, "LEGACY ORGAN");
    legacy_p.mask = (1u << edit::Cutoff);
    legacy_p.values[edit::Cutoff] = 800.0f;
    assert(synth_store.save(7, legacy_p));
    // Ensure sound012fx has no record for slot 7
    fake_nvs::blobs.erase({3, "f07a"});
    fake_nvs::blobs.erase({3, "f07b"});

    // Recall slot 7
    assert(backend.editAction(3, 7));
    backend.service(200);
    runtime.apply(200);
    runtime.loadPendingPatch();
    assert(runtime.render());
    backend.publish(runtime.snapshot(), false, 0, 0);
    backend.service(225);
    runtime.apply(225);
    assert(runtime.render());
    backend.publish(runtime.snapshot(), false, 0, 0);

    // Confirms patch 8, cutoff 800, and neutral dry FX fallback
    assert(backend.state().patch_id == 8);
    assert(runtime.snapshot().edit.applied[edit::Cutoff] == 800.0f);
    assert(runtime.snapshot().fx.config.chorus_mode == 0); // Off
    assert(runtime.snapshot().fx.config.delay_mix == 0.0f); // 0%
    assert(runtime.snapshot().fx.config.reverb_mix == 0.0f); // 0%

    runtime.end();
    std::puts("PASS FX Preset Persistence: dual-bank ping-pong, 16 slots, reboot, torn/CRC rollback, out-of-space, end-to-end backend recall, legacy fallback");
    return 0;
}
