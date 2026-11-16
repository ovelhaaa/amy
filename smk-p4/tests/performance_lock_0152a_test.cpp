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

// 2. Individual & Combined Locks Verification
void testLockCombinations() {
    std::printf("--- TEST 2: Lock Combinations ---\n");
    LocalAmyPerformanceBackend backend(runtime.commands());
    backend.publish(runtime.snapshot(), false, 0, 0);

    // Initial setup on patch 4
    assert(backend.setPatch(4, now, perf_lock::None));
    advance(20, backend);

    // Configure all 4 domains
    // 1. FX
    assert(backend.setFxParameter("fx.delay.mode", 1.0f, now)); // Sync
    assert(backend.setFxParameter("fx.delay.division", float(int(tempo::Division::DottedEighth)), now));
    assert(backend.setFxParameter("fx.reverb.mix", 40.0f, now));

    // 2. Harmony
    harmony::Config harm{};
    harm.scale_on = 1; harm.scale = 3; harm.chord_on = 1; harm.chord = 2; harm.root = 5;
    assert(backend.setHarmony(harm, now));

    // 3. Arp
    amy::Command c_arp{amy::CommandType::Arp, uint16_t(arp::Action::Enable), 1, 0, runtime.commands().nextToken(), now};
    c_arp.version = arp::kProtocolVersion;
    assert(runtime.commands().push(c_arp));
    c_arp.id = uint16_t(arp::Action::Rate);
    c_arp.value = static_cast<uint8_t>(arp::Rate::Eighth);
    assert(runtime.commands().push(c_arp));

    // 4. Macros
    performance::RouteConfiguration custom_routes{};
    custom_routes.custom = true;
    custom_routes.routes = performance::kFactoryRoutes;
    custom_routes.routes[0][0] = performance::route(edit::Cutoff, 200, 4000, 500.0f, performance::Juno);
    assert(backend.setPerformanceRoutes(custom_routes, 0));
    advance(20, backend);

    // Verify initial states
    assert(runtime.snapshot().fx.config.delay_mode == fx::DelayMode::Sync);
    assert(runtime.snapshot().harmony.scale == 3);
    assert(runtime.snapshot().arp.config.enabled);
    assert(runtime.snapshot().edit.performance_routes.custom);

    // TEST 2.1: Keep FX alone (0x01) -> FX preserved, others reset
    assert(backend.discardEditsAndSetPatch(0, now, perf_lock::Fx));
    advance(20, backend);
    assert(runtime.snapshot().patch_id == 0);
    assert(runtime.snapshot().fx.config.delay_mode == fx::DelayMode::Sync);
    assert(!runtime.snapshot().harmony.scale_on);
    assert(!runtime.snapshot().arp.config.enabled);
    assert(!runtime.snapshot().edit.performance_routes.custom);

    // Re-arm harmony and arp
    assert(backend.setHarmony(harm, now));
    c_arp.id = uint16_t(arp::Action::Enable); c_arp.value = 1;
    assert(runtime.commands().push(c_arp));
    advance(20, backend);

    // TEST 2.2: Keep Harmony alone (0x02) -> Harmony preserved, others reset
    assert(backend.discardEditsAndSetPatch(4, now, perf_lock::Harmony));
    advance(20, backend);
    assert(runtime.snapshot().patch_id == 4);
    assert(runtime.snapshot().fx.config.delay_mode == fx::DelayMode::Free); // FX reset
    assert(runtime.snapshot().harmony.scale == 3); // Harmony preserved
    assert(!runtime.snapshot().arp.config.enabled); // Arp reset

    // Re-arm arp
    c_arp.id = uint16_t(arp::Action::Enable); c_arp.value = 1;
    assert(runtime.commands().push(c_arp));
    advance(20, backend);

    // TEST 2.3: Keep Arp alone (0x04) -> Arp preserved, others reset
    assert(backend.setPatch(0, now, perf_lock::Arp));
    advance(20, backend);
    assert(runtime.snapshot().patch_id == 0);
    assert(runtime.snapshot().arp.config.enabled);
    assert(!runtime.snapshot().harmony.scale_on);

    // TEST 2.4: Combinations - FX + Harmony + Arp (0x07)
    assert(backend.setFxParameter("fx.delay.mode", 1.0f, now));
    assert(backend.setHarmony(harm, now));
    advance(20, backend);

    assert(backend.discardEditsAndSetPatch(128, now, perf_lock::Fx | perf_lock::Harmony | perf_lock::Arp));
    advance(20, backend);
    assert(runtime.snapshot().patch_id == 128);
    assert(runtime.snapshot().fx.config.delay_mode == fx::DelayMode::Sync);
    assert(runtime.snapshot().harmony.scale == 3);
    assert(runtime.snapshot().arp.config.enabled);

    std::printf("  PASS: Individual and combined lock configurations validated\n");
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
    assert(backend.noteOn(60, 90, now));
    advance(10, backend);
    backend.noteOff(60);
    advance(10, backend);

    // Switch patch with Keep Arp
    assert(backend.setPatch(4, now, perf_lock::Arp));
    advance(20, backend);

    // Arpeggiator remains enabled, but held/sounding notes are cleared!
    assert(runtime.snapshot().arp.config.enabled);
    assert(runtime.snapshot().active_notes == 0);
    assert(!runtime.snapshot().arp.sounding);
    assert(runtime.snapshot().arp.held_count == 0);

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
    std::printf("--- TEST 5: Delay & Audio Continuity ---\n");
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

    std::printf("  PASS: Delay bounds, division changes, feedback stability verified\n");
}

} // namespace

int main() {
    assert(runtime.begin());
    testFxBaselineAndDirtyState();
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
