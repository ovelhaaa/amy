#include "local_performance_backend.h"
#include "amy_runtime.h"
#include "user_preset_nvs_v2.h"
#include "user_preset_fx_nvs.h"
#include "fake_nvs.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>

extern "C" {
void probe_realtime(bool);
uint32_t probe_calls();
size_t probe_bytes();
}

using namespace smk::p4;

static void test_scenario_1_juno_strings_reboot_recall() {
    fake_nvs::blobs.clear();
    fake_nvs::free_entries = 700;

    // 1. Initial boot: configure Juno Strings + Chorus JunoII + Reverb
    {
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

        // Load patch 4 (Strings)
        assert(backend.setPatch(4, 0));
        runtime.apply(0);
        runtime.loadPendingPatch();
        assert(runtime.render());
        backend.publish(runtime.snapshot(), false, 0, 0);

        // Modify synth: Cutoff 1800Hz, Resonance 0.6
        assert(backend.editParameter(edit::Cutoff, 1800.0f));
        assert(backend.editParameter(edit::Resonance, 0.60f));

        // Configure FX: Chorus JunoII (mode 3), Reverb size 0.85, mix 0.40
        assert(backend.setFxParameter("fx.chorus.mode", 3.0f, 10));
        assert(backend.setFxParameter("fx.reverb.size", 85.0f, 10));
        assert(backend.setFxParameter("fx.reverb.mix", 40.0f, 10));

        backend.service(20);
        runtime.apply(20);
        assert(runtime.render());
        backend.publish(runtime.snapshot(), false, 0, 0);

        // Save As to Slot 1 ("JunoStrFX")
        assert(backend.editAction(2, 1, "JunoStrFX"));
        // Wait for quiet barrier and save completion
        for (uint32_t t = 30; t <= 200; t += 25) {
            backend.service(t);
            runtime.apply(t);
            runtime.loadPendingPatch();
            assert(runtime.render());
            backend.publish(runtime.snapshot(), false, 0, 0);
            if (!backend.busy() && !backend.editPending()) break;
        }

        assert(std::strcmp(backend.editNotice(), "SAVED") == 0);
        assert(!backend.unsaved());
    }

    // 2. Reboot simulation: fresh runtime and backend loading from persisted fake_nvs
    {
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

        // Recall Slot 1
        assert(backend.editAction(3, 1));
        for (uint32_t t = 10; t <= 200; t += 25) {
            backend.service(t);
            runtime.apply(t);
            runtime.loadPendingPatch();
            assert(runtime.render());
            backend.publish(runtime.snapshot(), false, 0, 0);
            if (!backend.busy() && !backend.editPending()) break;
        }

        assert(backend.state().patch_id == 4);
        assert(std::strcmp(backend.state().patch_name, "JunoStrFX") == 0);

        const auto* edit = backend.soundEdit();
        assert(edit != nullptr);
        assert(std::fabs(edit->applied[edit::Cutoff] - 1800.0f) < 20.0f);

        const auto* fx = backend.fxState();
        assert(fx != nullptr);
        assert(fx->config.chorus_mode == 3); // JunoII
        assert(std::fabs(fx->config.reverb_size - 0.85f) < 0.02f);
        assert(std::fabs(fx->config.reverb_mix - 0.40f) < 0.02f);
    }
    std::puts("  PASS: Scenario 1 - Juno Strings + Chorus II + Reverb reboot & exact recall");
}

static void test_scenario_2_dx7_piano_delay_reboot_recall() {
    // 1. Session 1: configure DX7 piano with Delay 420ms + Feedback 60%
    {
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

        // Load patch 128 (DX7 Electric Piano)
        assert(backend.setPatch(128, 0));
        runtime.apply(0);
        runtime.loadPendingPatch();
        assert(runtime.render());
        backend.publish(runtime.snapshot(), false, 0, 0);

        // Configure Delay: 420 ms, feedback 60%, mix 50%
        assert(backend.setFxParameter("fx.delay.time_ms", 420.0f, 10));
        assert(backend.setFxParameter("fx.delay.feedback", 60.0f, 10));
        assert(backend.setFxParameter("fx.delay.mix", 50.0f, 10));

        backend.service(20);
        runtime.apply(20);
        assert(runtime.render());
        backend.publish(runtime.snapshot(), false, 0, 0);

        // Save As to Slot 2 ("DX7Delay")
        assert(backend.editAction(2, 2, "DX7Delay"));
        for (uint32_t t = 30; t <= 200; t += 25) {
            backend.service(t);
            runtime.apply(t);
            runtime.loadPendingPatch();
            assert(runtime.render());
            backend.publish(runtime.snapshot(), false, 0, 0);
            if (!backend.busy() && !backend.editPending()) break;
        }

        assert(std::strcmp(backend.editNotice(), "SAVED") == 0);
    }

    // 2. Reboot simulation & Recall Slot 2
    {
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

        assert(backend.editAction(3, 2));
        for (uint32_t t = 10; t <= 200; t += 25) {
            backend.service(t);
            runtime.apply(t);
            runtime.loadPendingPatch();
            assert(runtime.render());
            backend.publish(runtime.snapshot(), false, 0, 0);
            if (!backend.busy() && !backend.editPending()) break;
        }

        assert(backend.state().patch_id == 128);
        assert(std::strcmp(backend.state().patch_name, "DX7Delay") == 0);

        const auto* fx = backend.fxState();
        assert(fx != nullptr);
        assert(std::fabs(fx->config.delay_time_ms - 420.0f) < 1.0f);
        assert(std::fabs(fx->config.delay_feedback - 0.60f) < 0.02f);
        assert(std::fabs(fx->config.delay_mix - 0.50f) < 0.02f);
    }
    std::puts("  PASS: Scenario 2 - DX7 Piano + Delay 420ms + Feedback 60% reboot & recall");
}

static void test_scenario_3_interrupted_write_rollback() {
    // 1. Initial valid save at Slot 3 (Revision 1: chorus mode 1)
    {
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

        assert(backend.setPatch(8, 0));
        runtime.apply(0);
        runtime.loadPendingPatch();
        assert(runtime.render());
        backend.publish(runtime.snapshot(), false, 0, 0);

        assert(backend.setFxParameter("fx.chorus.mode", 1.0f, 10));
        backend.service(20);
        runtime.apply(20);
        assert(runtime.render());
        backend.publish(runtime.snapshot(), false, 0, 0);

        assert(backend.editAction(2, 3, "Rev1"));
        for (uint32_t t = 30; t <= 200; t += 25) {
            backend.service(t);
            runtime.apply(t);
            runtime.loadPendingPatch();
            assert(runtime.render());
            backend.publish(runtime.snapshot(), false, 0, 0);
            if (!backend.busy() && !backend.editPending()) break;
        }
        assert(std::strcmp(backend.editNotice(), "SAVED") == 0);
    }

    // 2. Interrupted save at Slot 3 (Fault injected during FX write)
    {
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

        // Recall rev 1 first
        assert(backend.editAction(3, 3));
        for (uint32_t t = 10; t <= 200; t += 25) {
            backend.service(t);
            runtime.apply(t);
            runtime.loadPendingPatch();
            assert(runtime.render());
            backend.publish(runtime.snapshot(), false, 0, 0);
            if (!backend.busy() && !backend.editPending()) break;
        }
        assert(backend.fxState()->config.chorus_mode == 1);

        // Try to update to chorus mode 5, but inject fault during FX write
        assert(backend.setFxParameter("fx.chorus.mode", 5.0f, 130));
        backend.service(140);
        runtime.apply(140);
        assert(runtime.render());
        backend.publish(runtime.snapshot(), false, 0, 0);

        backend.transactionCoordinator().setFaultInjection(preset::FaultInjection::DuringFxWrite);
        assert(backend.editAction(2, 3, "Rev2_Corrupt"));

        for (uint32_t t = 150; t <= 300; t += 25) {
            backend.service(t);
            runtime.apply(t);
            assert(runtime.render());
            backend.publish(runtime.snapshot(), false, 0, 0);
            if (!backend.busy() && !backend.editPending()) break;
        }

        // Save MUST fail with transaction incomplete
        assert(backend.editError());
        assert(std::strstr(backend.editNotice(), "SAVE FAILED") != nullptr);
        backend.transactionCoordinator().setFaultInjection(preset::FaultInjection::None);
    }

    // 3. Reboot: verify Slot 3 recovered Revision 1 cleanly
    {
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

        assert(backend.editAction(3, 3));
        for (uint32_t t = 10; t <= 200; t += 25) {
            backend.service(t);
            runtime.apply(t);
            runtime.loadPendingPatch();
            assert(runtime.render());
            backend.publish(runtime.snapshot(), false, 0, 0);
            if (!backend.busy() && !backend.editPending()) break;
        }

        assert(std::strcmp(backend.state().patch_name, "Rev1") == 0);
        assert(backend.fxState()->config.chorus_mode == 1); // Cleanly preserved Rev 1!
    }
    std::puts("  PASS: Scenario 3 - Interrupted write cleanly rolls back to previous committed pair");
}

static void test_scenario_4_burst_overload_zero_heap() {
    amy::Runtime runtime;
    assert(runtime.begin());

    LocalAmyPerformanceBackend backend(runtime.commands());
    backend.publish(runtime.snapshot(), false, 0, 0);

    // Warm up render
    assert(runtime.render());

    const auto before_calls = probe_calls();
    probe_realtime(true);

    // Burst 60 rapid parameter modifications while rendering audio
    for (int i = 0; i < 60; ++i) {
        backend.setFxParameter("fx.chorus.depth", static_cast<float>(i % 100), static_cast<uint32_t>(i));
        backend.setFxParameter("fx.delay.feedback", static_cast<float>(i % 90), static_cast<uint32_t>(i));
        backend.service(static_cast<uint32_t>(i));
        runtime.apply(static_cast<uint32_t>(i));
        assert(runtime.render());
    }

    probe_realtime(false);
    assert(probe_calls() == before_calls && "Zero heap allocation allowed during live FX parameter changes");
    std::puts("  PASS: Scenario 4 - 60 burst FX updates with zero heap allocations");
}

static void test_scenario_5_rapid_patch_switch_with_reverb_tail() {
    amy::Runtime runtime;
    assert(runtime.begin());

    LocalAmyPerformanceBackend backend(runtime.commands());
    backend.publish(runtime.snapshot(), false, 0, 0);

    // Turn on long reverb directly on AMY bus 0 to test DSP stability during patch switching
    smk::p4::fx::FxConfig fx_cfg{};
    fx_cfg.reverb_size = 0.90f;
    fx_cfg.reverb_mix = 0.60f;
    smk::p4::fx::applyBusFx(fx_cfg);

    // Trigger note
    backend.noteOn(60, 120, 20);
    runtime.apply(20);
    for (int i = 0; i < 5; ++i) assert(runtime.render());

    // Release note -> reverb tail begins decaying
    backend.noteOff(60);
    runtime.apply(30);

    // Rapidly change patches 10 times while the reverb is ringing out
    uint16_t patches[] = {4, 8, 128, 135, 4, 128, 8, 135, 4, 128};
    for (unsigned i = 0; i < 10; ++i) {
        assert(backend.setPatch(patches[i], 40 + i * 10));
        backend.service(40 + i * 10);
        runtime.apply(40 + i * 10);
        runtime.loadPendingPatch();
        assert(runtime.render());
        backend.publish(runtime.snapshot(), false, 0, 0);
    }

    std::puts("  PASS: Scenario 5 - Rapid patch switching during active reverb decay");
}

static void test_scenario_6_panic_clears_pending_fx() {
    amy::Runtime runtime;
    assert(runtime.begin());

    LocalAmyPerformanceBackend backend(runtime.commands());
    backend.publish(runtime.snapshot(), false, 0, 0);

    // Push an FX parameter
    assert(backend.setFxParameter("fx.chorus.mode", 4.0f, 10));

    // Immediate panic
    backend.panic();

    // Render audio: panic must clear queue and advance generation, rejecting stale parameter
    backend.service(20);
    runtime.apply(20);
    assert(runtime.render());
    auto snap = runtime.snapshot();

    // Mode must NOT have been applied to snapshot
    assert(snap.fx.config.chorus_mode == 0);
    std::puts("  PASS: Scenario 6 - Panic immediately purges pending FX requests");
}

static void test_scenario_7_neutral_audio_bit_exact() {
    std::vector<int16_t> dry_pcm;
    {
        amy::Runtime r;
        assert(r.begin());
        LocalAmyPerformanceBackend b(r.commands());
        b.noteOn(60, 100, 0);
        r.apply(0);
        for (int blk = 0; blk < 5; ++blk) {
            const int16_t* pcm = r.render();
            dry_pcm.insert(dry_pcm.end(), pcm, pcm + 512);
        }
        r.end();
    }

    std::vector<int16_t> fx_bypassed_pcm;
    {
        amy::Runtime r;
        assert(r.begin());
        LocalAmyPerformanceBackend b(r.commands());
        smk::p4::fx::FxConfig bypass_cfg{};
        bypass_cfg.bypass_all = true;
        smk::p4::fx::applyBusFx(bypass_cfg);
        b.noteOn(60, 100, 0);
        r.apply(0);
        for (int blk = 0; blk < 5; ++blk) {
            const int16_t* pcm = r.render();
            fx_bypassed_pcm.insert(fx_bypassed_pcm.end(), pcm, pcm + 512);
        }
        r.end();
    }

    assert(dry_pcm.size() == fx_bypassed_pcm.size());
    for (size_t i = 0; i < dry_pcm.size(); ++i) {
        assert(dry_pcm[i] == fx_bypassed_pcm[i]);
    }

    std::puts("  PASS: Scenario 7 - Neutral/bypassed FX output is bit-exact to baseline");
}

int main() {
    std::puts("=== GATE I: End-to-End Reboot, Recovery & Resilience Tests ===");
    test_scenario_1_juno_strings_reboot_recall();
    test_scenario_2_dx7_piano_delay_reboot_recall();
    test_scenario_3_interrupted_write_rollback();
    test_scenario_4_burst_overload_zero_heap();
    test_scenario_5_rapid_patch_switch_with_reverb_tail();
    test_scenario_6_panic_clears_pending_fx();
    test_scenario_7_neutral_audio_bit_exact();
    std::puts("ALL GATE I TESTS PASSED.");
    return 0;
}
