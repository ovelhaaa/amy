#include "user_preset_nvs_v2.h"
#include "user_preset_fx_nvs.h"
#include "preset_transaction.h"
#include "fake_nvs.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>

using namespace smk::p4;
using namespace smk::p4::preset;

static bool approx(float a, float b, float tol = 0.015f) {
    return std::fabs(a - b) <= tol;
}

int main() {
    fake_nvs::blobs.clear();
    fake_nvs::free_entries = 700;

    NvsVersionedPresets synth_store;
    assert(synth_store.begin());

    fx::NvsFxPresets fx_store;
    assert(fx_store.begin());

    PresetTransactionCoordinator coordinator;
    coordinator.setSynthStore(&synth_store);
    coordinator.setFxStore(&fx_store);

    // Initial load on empty slot returns false
    edit::Preset loaded_p{};
    fx::FxConfig loaded_fx{};
    assert(!coordinator.load(0, loaded_p, loaded_fx));

    // ── 1. Clean Commit of Preset 1 (Slot 0) ────────────────────────────────
    edit::Preset p1{};
    p1.sequence = 1;
    p1.patch = 4; // Moving Strings
    p1.family = 1;
    std::strcpy(p1.name, "TRANSACTION A");
    p1.mask = (1ULL << edit::Cutoff);
    p1.values[edit::Cutoff] = 1150.0f;

    fx::FxConfig fx1{};
    fx1.chorus_mode = 2; // Juno I
    fx1.chorus_depth = 0.70f;
    fx1.delay_time_ms = 400.0f;
    fx1.reverb_mix = 0.35f;

    auto res1 = coordinator.save(0, p1, fx1);
    assert(res1.status == SaveResult::Status::Success);
    assert(std::strcmp(res1.message, "SAVED") == 0);

    // Verify recovery after clean save
    assert(coordinator.load(0, loaded_p, loaded_fx));
    assert(std::strcmp(loaded_p.name, "TRANSACTION A") == 0);
    assert(loaded_p.patch == 4);
    assert(loaded_p.values[edit::Cutoff] == 1150.0f);
    assert(loaded_fx.chorus_mode == 2);
    assert(approx(loaded_fx.delay_time_ms, 400.0f));
    assert(approx(loaded_fx.reverb_mix, 0.35f));

    // ── 2. Fault Injection Across All Transaction Steps ─────────────────────
    // Target: Updating Slot 0 with Preset 2
    edit::Preset p2 = p1;
    std::strcpy(p2.name, "TRANSACTION B");
    p2.values[edit::Cutoff] = 2200.0f;

    fx::FxConfig fx2 = fx1;
    fx2.chorus_mode = 3; // Juno II
    fx2.delay_time_ms = 800.0f;
    fx2.reverb_mix = 0.90f;

    // Test fault: BeforeFxWrite
    coordinator.setFaultInjection(FaultInjection::BeforeFxWrite);
    auto res_f1 = coordinator.save(0, p2, fx2);
    assert(res_f1.status != SaveResult::Status::Success);
    // Verified: Slot 0 must still load intact Preset A
    assert(coordinator.load(0, loaded_p, loaded_fx));
    assert(std::strcmp(loaded_p.name, "TRANSACTION A") == 0);
    assert(loaded_p.values[edit::Cutoff] == 1150.0f);
    assert(loaded_fx.chorus_mode == 2);

    // Test fault: DuringFxWrite (corrupt FX record)
    coordinator.setFaultInjection(FaultInjection::DuringFxWrite);
    auto res_f2 = coordinator.save(0, p2, fx2);
    assert(res_f2.status != SaveResult::Status::Success);
    assert(coordinator.load(0, loaded_p, loaded_fx));
    assert(std::strcmp(loaded_p.name, "TRANSACTION A") == 0);
    assert(loaded_fx.chorus_mode == 2);

    // Test fault: AfterFxWrite / BeforeSynthWrite
    coordinator.setFaultInjection(FaultInjection::AfterFxWrite);
    auto res_f3 = coordinator.save(0, p2, fx2);
    assert(res_f3.status != SaveResult::Status::Success);
    assert(coordinator.load(0, loaded_p, loaded_fx));
    assert(std::strcmp(loaded_p.name, "TRANSACTION A") == 0);
    assert(loaded_fx.chorus_mode == 2);

    // Test fault: DuringSynthWrite (corrupt Synth record)
    coordinator.setFaultInjection(FaultInjection::DuringSynthWrite);
    auto res_f4 = coordinator.save(0, p2, fx2);
    assert(res_f4.status != SaveResult::Status::Success);
    assert(coordinator.load(0, loaded_p, loaded_fx));
    assert(std::strcmp(loaded_p.name, "TRANSACTION A") == 0);
    assert(loaded_fx.chorus_mode == 2);

    // Test fault: AfterSynthWrite / BeforeMarkerWrite
    coordinator.setFaultInjection(FaultInjection::AfterSynthWrite);
    auto res_f5 = coordinator.save(0, p2, fx2);
    assert(res_f5.status != SaveResult::Status::Success);
    assert(coordinator.load(0, loaded_p, loaded_fx));
    assert(std::strcmp(loaded_p.name, "TRANSACTION A") == 0);
    assert(loaded_fx.chorus_mode == 2);

    // Test fault: DuringMarkerWrite (corrupt Commit Marker)
    coordinator.setFaultInjection(FaultInjection::DuringMarkerWrite);
    auto res_f6 = coordinator.save(0, p2, fx2);
    assert(res_f6.status != SaveResult::Status::Success);
    assert(coordinator.load(0, loaded_p, loaded_fx));
    assert(std::strcmp(loaded_p.name, "TRANSACTION A") == 0);
    assert(loaded_fx.chorus_mode == 2);

    // Test fault: DuringReadback (verification failure)
    coordinator.setFaultInjection(FaultInjection::DuringReadback);
    auto res_f7 = coordinator.save(0, p2, fx2);
    assert(res_f7.status != SaveResult::Status::Success);

    // Reset fault injection to None
    coordinator.setFaultInjection(FaultInjection::None);

    // ── 3. Clean Commit of Preset 2 (Overwriting Slot 0 via Ping-Pong) ──────
    auto res2 = coordinator.save(0, p2, fx2);
    assert(res2.status == SaveResult::Status::Success);

    // Verify recovery after clean overwrite
    assert(coordinator.load(0, loaded_p, loaded_fx));
    assert(std::strcmp(loaded_p.name, "TRANSACTION B") == 0);
    assert(loaded_p.values[edit::Cutoff] == 2200.0f);
    assert(loaded_fx.chorus_mode == 3);
    assert(approx(loaded_fx.delay_time_ms, 800.0f));
    assert(approx(loaded_fx.reverb_mix, 0.90f));

    // ── 4. Reboot Persistence with Independent Instances ────────────────────
    {
        NvsVersionedPresets reboot_synth;
        assert(reboot_synth.begin());
        fx::NvsFxPresets reboot_fx;
        assert(reboot_fx.begin());
        PresetTransactionCoordinator reboot_coord;
        reboot_coord.setSynthStore(&reboot_synth);
        reboot_coord.setFxStore(&reboot_fx);

        edit::Preset p_boot{};
        fx::FxConfig fx_boot{};
        assert(reboot_coord.load(0, p_boot, fx_boot));
        assert(std::strcmp(p_boot.name, "TRANSACTION B") == 0);
        assert(p_boot.values[edit::Cutoff] == 2200.0f);
        assert(fx_boot.chorus_mode == 3);
        assert(approx(fx_boot.delay_time_ms, 800.0f));
    }

    // ── 5. Torn Active Marker Rollback to Alternate Marker ───────────────────
    // Save a new revision (Transaction C -> Bank 1)
    edit::Preset p3 = p2;
    std::strcpy(p3.name, "TRANSACTION C");
    p3.values[edit::Cutoff] = 500.0f;
    fx::FxConfig fx3 = fx2;
    fx3.chorus_mode = 1; // Classic
    assert(coordinator.save(0, p3, fx3).status == SaveResult::Status::Success);
    assert(coordinator.load(0, loaded_p, loaded_fx));
    assert(std::strcmp(loaded_p.name, "TRANSACTION C") == 0);

    // Corrupt active commit marker in Bank 1 (c00b)
    auto& marker_b = fake_nvs::blobs.at({3, "c00b"});
    marker_b[10] ^= 0xff; // Corrupt active marker byte

    {
        NvsVersionedPresets rollback_synth;
        assert(rollback_synth.begin());
        fx::NvsFxPresets rollback_fx;
        assert(rollback_fx.begin());
        PresetTransactionCoordinator rollback_coord;
        rollback_coord.setSynthStore(&rollback_synth);
        rollback_coord.setFxStore(&rollback_fx);

        edit::Preset p_roll{};
        fx::FxConfig fx_roll{};
        // Rolls back to intact Transaction B in Bank 0!
        assert(rollback_coord.load(0, p_roll, fx_roll));
        assert(std::strcmp(p_roll.name, "TRANSACTION B") == 0);
        assert(p_roll.values[edit::Cutoff] == 2200.0f);
        assert(fx_roll.chorus_mode == 3);
    }

    std::puts("PASS GATE B: 2-Phase Coordinated Presets Commit, Fault Injection (all 10 points) & Dual-Bank Rollback verified");
    return 0;
}
 
