#include "amy_runtime.h"
#include "local_performance_backend.h"
#include "performance_lock.h"
#include "user_preset_nvs.h"
#include "fake_nvs.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

extern "C" void probe_realtime(bool);

namespace {
using namespace smk::p4;

static amy::Runtime runtime;
static uint32_t now = 1000;

void advance(uint32_t ms, LocalAmyPerformanceBackend& backend) {
    now += ms;
    backend.service(now);
    runtime.apply(now);
    runtime.loadPendingPatch();
    runtime.render();
    backend.publish(runtime.snapshot(), false, 0, 0);
}

// 1. FX Baseline & Dirty State Verification
void testFxBaselineAndDirtyState() {
    std::printf("--- TEST 1: FX Baseline & Dirty State ---\n");
    NvsUserPresets store;
    assert(store.begin());
    fx::NvsFxPresets fx_store;
    assert(fx_store.begin());
    LocalAmyPerformanceBackend backend(runtime.commands());
    backend.presetStore(&store);
    backend.fxPresetStore(&fx_store);
    backend.publish(runtime.snapshot(), false, 0, 0);

    // 1.1 Load Patch 4 (Juno) clean
    assert(backend.setPatch(4, now, perf_lock::None));
    advance(20, backend);
    assert(runtime.snapshot().patch_id == 4);
    assert(!backend.patchHasUnsavedEdits());
    assert(std::strcmp(backend.editNotice(), "FACTORY ORIGINAL") == 0);

    // 1.2 Modify FX: delay, chorus, reverb
    assert(backend.setFxParameter("fx.delay.time_ms", 480.0f, now));
    assert(backend.setFxParameter("fx.delay.feedback", 55.0f, now));
    assert(backend.setFxParameter("fx.delay.mix", 40.0f, now));
    assert(backend.setFxParameter("fx.chorus.mode", 1.0f, now)); // Classic
    assert(backend.setFxParameter("fx.chorus.depth", 70.0f, now));
    assert(backend.setFxParameter("fx.reverb.size", 80.0f, now));
    assert(backend.setFxParameter("fx.reverb.mix", 50.0f, now));
    advance(20, backend);

    // Verify FX modified and marked unsaved
    assert(backend.patchHasUnsavedEdits());
    assert(backend.fxUnsaved());
    assert(std::strstr(backend.editNotice(), "UNSAVED") != nullptr);

    // 1.3 Keep FX active, load Patch 128 (DX7)
    assert(backend.discardEditsAndSetPatch(128, now, perf_lock::Fx));
    advance(20, backend);
    assert(runtime.snapshot().patch_id == 128);

    // 1.4 Verify FX remain active
    const auto& s_fx = runtime.snapshot().fx.config;
    assert(std::lround(s_fx.delay_time_ms) == 480);
    assert(std::lround(s_fx.delay_feedback * 100.0f) == 55);
    assert(std::lround(s_fx.delay_mix * 100.0f) == 40);
    assert(s_fx.chorus_mode == 1);
    assert(std::lround(s_fx.chorus_depth * 100.0f) == 70);
    assert(std::lround(s_fx.reverb_size * 100.0f) == 80);
    assert(std::lround(s_fx.reverb_mix * 100.0f) == 50);

    // 1.5 Verify that UI indicates UNSAVED (effective FX differs from Patch 128 baseline)
    assert(backend.patchHasUnsavedEdits());
    assert(backend.fxUnsaved());
    assert(std::strstr(backend.editNotice(), "UNSAVED") != nullptr);

    // 1.6 Compare & Revert without losing baseline references
    // Compare toggles to factory baseline of Patch 128 (reverb mix 0%, delay mix 0%)
    assert(backend.editAction(0)); // Compare ON
    advance(20, backend);
    assert(runtime.snapshot().edit.compare);
    assert(std::lround(runtime.snapshot().fx.config.reverb_mix * 100.0f) == 0);
    assert(std::lround(runtime.snapshot().fx.config.delay_mix * 100.0f) == 0);

    assert(backend.editAction(0)); // Compare OFF
    advance(20, backend);
    assert(!runtime.snapshot().edit.compare);
    // Exiting compare restored preserved FX
    assert(std::lround(runtime.snapshot().fx.config.reverb_mix * 100.0f) == 50);

    // Revert restores factory baseline of Patch 128
    assert(backend.editAction(1)); // Revert
    advance(20, backend);
    assert(!backend.patchHasUnsavedEdits());
    assert(!backend.fxUnsaved());
    assert(std::strcmp(backend.editNotice(), "FACTORY ORIGINAL") == 0);
    assert(std::lround(runtime.snapshot().fx.config.reverb_mix * 100.0f) == 0);

    // 1.7 Save As and confirm state passes to CLEAN
    // Edit FX again
    assert(backend.setFxParameter("fx.reverb.mix", 35.0f, now));
    advance(20, backend);
    assert(backend.patchHasUnsavedEdits());

    // Save As slot 0
    assert(backend.editAction(2, 0, "TEST PRESET 0"));
    for (unsigned i = 0; i < 4; ++i) advance(20, backend);
    assert(!backend.patchHasUnsavedEdits());
    assert(!backend.fxUnsaved());
    assert(std::strstr(backend.editNotice(), "SAVED") != nullptr);

    // 1.8 Verify no false dirty when preserved FX is already equivalent to baseline
    // Reset to clean patch 4 with Keep FX off
    assert(backend.discardEditsAndSetPatch(4, now, perf_lock::None));
    advance(20, backend);
    assert(!backend.patchHasUnsavedEdits());

    // Switch to patch 135 WITH Keep FX (since current FX is neutral, no false dirty!)
    assert(backend.setPatch(135, now, perf_lock::Fx));
    advance(20, backend);
    assert(runtime.snapshot().patch_id == 135);
    assert(!backend.patchHasUnsavedEdits());
    assert(!backend.fxUnsaved());
    assert(std::strcmp(backend.editNotice(), "FACTORY ORIGINAL") == 0);

    std::printf("  PASS: FX baseline separation, dirty state, Compare/Revert, Save As and no false dirty\n");
}

void testSamePatchReload(bool keep_fx) {
    NvsUserPresets store;
    fx::NvsFxPresets fx_store;
    assert(store.begin() && fx_store.begin());
    LocalAmyPerformanceBackend backend(runtime.commands());
    backend.presetStore(&store);
    backend.fxPresetStore(&fx_store);
    backend.publish(runtime.snapshot(), false, 0, 0);
    assert(backend.discardEditsAndSetPatch(4, now, perf_lock::None));
    advance(20, backend);
    assert(backend.setFxParameter("fx.reverb.mix", 40.0f, now));
    advance(20, backend);
    const auto previous = runtime.snapshot();
    assert(backend.discardEditsAndSetPatch(4, now, keep_fx ? perf_lock::Fx : perf_lock::None));
    assert(backend.fxUnsaved()); // The queued request cannot change the reference.
    advance(20, backend);
    assert(runtime.snapshot().patch_id == previous.patch_id);
    assert(runtime.snapshot().patch_changes == previous.patch_changes + 1);
    backend.publish(previous, false, 0, 0);
    assert(backend.patchRevision() == previous.patch_changes + 1);
    assert(backend.fxUnsaved() == keep_fx);
    assert(backend.patchHasUnsavedEdits() == keep_fx);

    assert(backend.editAction(0));
    advance(20, backend);
    assert(runtime.snapshot().edit.compare && runtime.snapshot().fx.config.reverb_mix == 0.0f);
    assert(backend.editAction(0));
    advance(20, backend);
    assert(!runtime.snapshot().edit.compare);
    const float expected = keep_fx ? 0.4f : 0.0f;
    assert(std::fabs(runtime.snapshot().fx.config.reverb_mix - expected) < 1e-4f);

    assert(backend.editAction(2, keep_fx ? 1 : 2, "SAME PATCH"));
    for (unsigned i = 0; i < 4; ++i) advance(20, backend);
    assert(!backend.patchHasUnsavedEdits());
    assert(backend.setFxParameter("fx.reverb.mix", keep_fx ? 0.0f : 75.0f, now));
    advance(20, backend);
    assert(backend.fxUnsaved());
    assert(backend.editAction(1));
    advance(20, backend);
    assert(std::fabs(runtime.snapshot().fx.config.reverb_mix - expected) < 1e-4f);
    assert(!backend.patchHasUnsavedEdits());
    assert(backend.editAction(3, keep_fx ? 1 : 2));
    for (unsigned i = 0; i < 6; ++i) advance(20, backend);
    assert(backend.presetOperationStatus().state == PresetOperationState::Confirmed);
    assert(!backend.patchHasUnsavedEdits());
    assert(std::fabs(runtime.snapshot().fx.config.reverb_mix - expected) < 1e-4f);

    assert(backend.setFxParameter("fx.reverb.mix", 65.0f, now));
    advance(20, backend);
    assert(backend.discardEditsAndSetPatch(4, now, perf_lock::None));
    runtime.apply(now);
    assert(runtime.snapshot().patch_state == amy::PatchState::Requested);
    runtime.commands().panic();
    advance(20, backend);
    assert(backend.patchError());
    assert(backend.fxUnsaved());
    assert(std::fabs(backend.fxState()->config.reverb_mix - 0.65f) < 1e-4f);
    std::printf("PASS same-patch reload, Keep FX=%d, Compare/Revert, Save As, Recall V4, stale and failed load\n", keep_fx);
}

void testLockCombinations() {
    for (uint8_t mask = 0; mask < 16; ++mask) {
        LocalAmyPerformanceBackend backend(runtime.commands());
        backend.publish(runtime.snapshot(), false, 0, 0);
        assert(backend.discardEditsAndSetPatch(4, now, perf_lock::None));
        advance(20, backend);
        assert(!backend.patchHasUnsavedEdits());

        assert(backend.setFxParameter("fx.delay.mode", 1, now));
        assert(backend.setFxParameter("fx.reverb.mix", 40, now));
        harmony::Config harm{};
        harm.scale_on = harm.chord_on = true;
        harm.scale = 3; harm.chord = 2; harm.root = 5;
        assert(backend.setHarmony(harm, now));
        const auto arp_command = [&](arp::Action action, uint8_t value) {
            amy::Command cmd{amy::CommandType::Arp, uint16_t(action), value, 0,
                             runtime.commands().nextToken(), now};
            cmd.version = arp::kProtocolVersion;
            assert(runtime.commands().push(cmd));
        };
        arp_command(arp::Action::Bpm, 137);
        arp_command(arp::Action::Enable, 1);
        arp_command(arp::Action::Rate, uint8_t(arp::Rate::Eighth));
        arp_command(arp::Action::Latch, 1);
        arp_command(arp::Action::Mode, uint8_t(arp::Mode::Down));
        arp_command(arp::Action::Octaves, 3);
        arp_command(arp::Action::Gate, 72);
        arp_command(arp::Action::Swing, 60);
        arp_command(arp::Action::Restart, 1);
        performance::RouteConfiguration routes{performance::kFactoryRoutes, true};
        routes.routes[0][0] = performance::route(performance::ReverbSize, 0, 1, 0.25f);
        assert(backend.setPerformanceRoutes(routes, 0));
        advance(25, backend);
        assert(backend.setMacro(0, 100, now));
        advance(25, backend);
        assert(backend.noteOn(60, 100, now));
        const uint8_t usb_note[]{0x09, 0x90, 64, 90};
        assert(runtime.usbInput().packet(usb_note, now));
        advance(25, backend);
        const auto before = runtime.snapshot();
        assert(before.active_notes && before.arp.held_count && before.arp.tempo.bpm == 137);
        assert(before.fx.config.delay_mode == fx::DelayMode::Sync);
        assert(harmony::pack(before.harmony) == harmony::pack(harm));
        assert(before.edit.performance_routes == routes && before.edit.performance_macros[0] == 100);

        assert(backend.discardEditsAndSetPatch(128, now, mask));
        advance(25, backend);
        const auto& after = runtime.snapshot();
        assert(after.patch_id == 128 && before.edit.family != after.edit.family);
        assert(after.preserve_mask == mask);
        assert(fx::fxEquivalent(after.fx.config, mask & perf_lock::Fx ? before.fx.config : fx::FxConfig{}));
        assert(harmony::pack(after.harmony) == harmony::pack(mask & perf_lock::Harmony ? harm : harmony::Config{}));
        const auto& arp_cfg = after.arp.config;
        const auto expected_arp = mask & perf_lock::Arp ? before.arp.config : arp::Config{};
        assert(arp_cfg.enabled == expected_arp.enabled && arp_cfg.rate == expected_arp.rate &&
               arp_cfg.latch == expected_arp.latch && arp_cfg.mode == expected_arp.mode &&
               arp_cfg.octaves == expected_arp.octaves && arp_cfg.gate == expected_arp.gate &&
               arp_cfg.swing == expected_arp.swing && arp_cfg.restart == expected_arp.restart);
        assert(after.edit.performance_routes == (mask & perf_lock::Macros ? routes : performance::RouteConfiguration{}));
        assert(after.edit.performance_macros == (mask & perf_lock::Macros ? before.edit.performance_macros : performance::kNeutral));
        assert(after.arp.tempo.bpm == before.arp.tempo.bpm);
        assert(!after.active_notes && !after.arp.held_count && !after.arp.sounding);
        assert(backend.fxUnsaved() == bool(mask & perf_lock::Fx));
        assert(backend.synthUnsaved() == bool(mask & perf_lock::Macros));
        assert(backend.patchHasUnsavedEdits() == bool(mask & (perf_lock::Fx | perf_lock::Macros)));
        std::printf("PASS lock mask 0x%02x\n", unsigned(mask));
    }
}

// 3. Musical Integrity & Stuck-Note Prevention
void testMusicalIntegrity() {
    std::printf("--- TEST 3: Musical Integrity & Stuck Notes ---\n");
    LocalAmyPerformanceBackend backend(runtime.commands());
    backend.publish(runtime.snapshot(), false, 0, 0);

    // 3.1 Held touch notes and sustain active during patch switch
    assert(backend.discardEditsAndSetPatch(4, now, perf_lock::None));
    advance(20, backend);

    // Start 4 notes
    assert(backend.noteOn(60, 100, now));
    assert(backend.noteOn(64, 100, now));
    assert(backend.noteOn(67, 100, now));
    assert(backend.noteOn(71, 100, now));
    advance(10, backend);
    assert(runtime.snapshot().active_notes == 4);

    // Switch patch while notes are sounding
    assert(backend.setPatch(128, now, perf_lock::kDefaultLockMask));
    advance(20, backend);

    // Rule 1: No stuck notes across patch boundary!
    assert(runtime.snapshot().patch_id == 128);
    assert(runtime.snapshot().active_notes == 0);

    // 3.2 Arpeggiator with Latched notes across patch boundary
    amy::Command c_arp{amy::CommandType::Arp, uint16_t(arp::Action::Enable), 1, 0, runtime.commands().nextToken(), now};
    c_arp.version = arp::kProtocolVersion;
    assert(runtime.commands().push(c_arp));
    c_arp.id = uint16_t(arp::Action::Latch); c_arp.value = 1;
    assert(runtime.commands().push(c_arp));
    advance(10, backend);

    // Send note to arpeggiator
    const uint8_t usb_on[]{0x09, 0x90, 60, 90};
    const uint8_t usb_off[]{0x08, 0x80, 60, 0};
    assert(runtime.usbInput().packet(usb_on, now));
    advance(10, backend);
    assert(runtime.usbInput().packet(usb_off, now));
    advance(10, backend);

    assert(runtime.snapshot().arp.held_count == 0 && runtime.snapshot().arp.length > 0);

    // Switch patch with Keep Arp
    assert(backend.setPatch(4, now, perf_lock::Arp));
    advance(20, backend);

    // Arpeggiator remains enabled, but held/sounding notes are cleared!
    assert(runtime.snapshot().arp.config.enabled);
    assert(runtime.snapshot().active_notes == 0);
    assert(!runtime.snapshot().arp.sounding);
    assert(runtime.snapshot().arp.held_count == 0 && runtime.snapshot().arp.length == 0);

    // 3.3 Global BPM consistency
    const auto initial_bpm = runtime.snapshot().arp.tempo.bpm;
    assert(backend.setPatch(128, now, perf_lock::None));
    advance(20, backend);
    assert(runtime.snapshot().arp.tempo.bpm == initial_bpm);

    // 3.4 Panic during patch loading
    assert(backend.setPatch(4, now, perf_lock::None));
    backend.panic();
    advance(20, backend);
    assert(runtime.snapshot().active_notes == 0);

    std::printf("  PASS: Musical integrity, stuck-note prevention and panic verified\n");
}

// 4. Concurrent Requests & Transactional Loading
void testConcurrentPatchRequests() {
    std::printf("--- TEST 4: Concurrent Patch Requests ---\n");
    LocalAmyPerformanceBackend backend(runtime.commands());
    backend.publish(runtime.snapshot(), false, 0, 0);

    assert(backend.setPatch(4, now, perf_lock::None));
    advance(20, backend);
    assert(runtime.snapshot().patch_id == 4);

    // 4.1 First patch request accepted
    assert(backend.setPatch(128, now, perf_lock::None));
    assert(backend.busy());

    // 4.2 Second rapid patch request must be rejected while first is pending
    assert(!backend.setPatch(135, now, perf_lock::None));

    // Complete the first load
    advance(20, backend);
    assert(!backend.busy());
    assert(runtime.snapshot().patch_id == 128);

    // 4.3 Rapid PREV/NEXT simulation
    assert(backend.setPatch(4, now, perf_lock::None));
    assert(!backend.setPatch(135, now, perf_lock::None)); // rejected
    advance(20, backend);
    assert(runtime.snapshot().patch_id == 4);

    std::printf("  PASS: Concurrent requests rejected deterministically; no race conditions\n");
}

// 5. Delay & Continuous Audio Tests
void testDelayContinuity() {
    std::printf("--- TEST 5: Delay Parameter Bounds ---\n");
    LocalAmyPerformanceBackend backend(runtime.commands());
    backend.publish(runtime.snapshot(), false, 0, 0);

    assert(backend.setPatch(4, now, perf_lock::None));
    advance(20, backend);

    // Set Sync delay
    assert(backend.setFxParameter("fx.delay.mode", 1.0f, now));
    assert(backend.setFxParameter("fx.delay.division", float(int(tempo::Division::Sixteenth)), now));
    assert(backend.setFxParameter("fx.delay.feedback", 95.0f, now)); // Safe upper limit
    assert(backend.setFxParameter("fx.delay.mix", 50.0f, now));
    advance(20, backend);

    // Verify feedback is capped at safe limit 0.95
    assert(runtime.snapshot().fx.config.delay_feedback <= 0.95f);

    // Rapid division change
    for (int div = 0; div < int(tempo::Division::Count); ++div) {
        assert(backend.setFxParameter("fx.delay.division", float(div), now));
        advance(5, backend);
        assert(runtime.snapshot().fx.applied.delay_time_ms >= 10.0f);
        assert(runtime.snapshot().fx.applied.delay_time_ms <= 1000.0f);
    }

    // Switch patch with Keep FX
    assert(backend.discardEditsAndSetPatch(128, now, perf_lock::Fx));
    advance(20, backend);
    assert(runtime.snapshot().fx.config.delay_mode == fx::DelayMode::Sync);
    assert(runtime.snapshot().fx.config.delay_feedback <= 0.95f);

    // Switch patch without Keep FX
    assert(backend.discardEditsAndSetPatch(4, now, perf_lock::None));
    advance(20, backend);
    assert(runtime.snapshot().fx.config.delay_mode == fx::DelayMode::Free);

    std::printf("  PASS: Delay bounds, division changes and feedback cap verified\n");
}

} // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    assert(runtime.begin());
    testFxBaselineAndDirtyState();
    testSamePatchReload(false);
    testSamePatchReload(true);
    testLockCombinations();
    testMusicalIntegrity();
    testConcurrentPatchRequests();
    testDelayContinuity();
    runtime.end();
    std::puts("\n===========================================================");
    std::puts("=== ALL 0.15.2a PERFORMANCE LOCK TESTS PASSED (PASS)     ===");
    std::puts("===========================================================");
    return 0;
}
