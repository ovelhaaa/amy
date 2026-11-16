#include "amy_runtime.h"
#include "local_performance_backend.h"
#include "performance_lock.h"
#include "fake_nvs.h"
#include <cassert>
#include <cmath>
#include <cstdio>

extern "C" void probe_realtime(bool);

namespace {
using namespace smk::p4;

static amy::Runtime runtime;
static uint32_t now = 0;

void testPatchLoadPreservation() {
    assert(runtime.begin());

    LocalAmyPerformanceBackend backend(runtime.commands());
    backend.publish(runtime.snapshot(), false, 0, 0);

    // Initial patch: Juno patch 4
    assert(backend.setPatch(4, now, perf_lock::None));
    runtime.apply(now);
    runtime.loadPendingPatch();
    backend.publish(runtime.snapshot(), false, 0, 0);
    assert(runtime.snapshot().patch_id == 4);

    // Configure performance settings:
    // 1. FX
    fx::FxConfig fx_cfg{};
    fx_cfg.delay_mode = fx::DelayMode::Sync;
    fx_cfg.delay_division = tempo::Division::DottedEighth;
    fx_cfg.delay_feedback = 0.70f;
    fx_cfg.delay_mix = 0.45f;
    fx_cfg.chorus_mode = static_cast<uint8_t>(fx::ChorusMode::Classic);
    fx_cfg.chorus_depth = 0.6f;
    fx_cfg.reverb_size = 0.5f;
    fx_cfg.eq_high_db = 3.0f;
    assert(runtime.commands().fx.push(fx::FxRequest{fx_cfg, 0, 1, 0, false}));

    // 2. Harmony
    harmony::Config harm_cfg{};
    harm_cfg.scale = 2; // Minor
    harm_cfg.root = 0;  // C
    harm_cfg.chord = 2; // Min7
    harm_cfg.quant = 1;
    assert(backend.setHarmony(harm_cfg, now));

    // 3. Arpeggiator
    amy::Command c_arp{amy::CommandType::Arp, uint16_t(arp::Action::Enable), 1, 0, runtime.commands().nextToken(), now};
    c_arp.version = arp::kProtocolVersion;
    assert(runtime.commands().push(c_arp));
    c_arp.id = uint16_t(arp::Action::Rate);
    c_arp.value = static_cast<uint8_t>(arp::Rate::Eighth); // 1/8 (default is Sixteenth)
    assert(runtime.commands().push(c_arp));

    // Process settings
    now += 10;
    runtime.apply(now);
    backend.publish(runtime.snapshot(), false, 0, 0);

    assert(runtime.snapshot().fx.config.delay_mode == fx::DelayMode::Sync);
    assert(runtime.snapshot().harmony.scale == 2);
    assert(runtime.snapshot().arp.config.enabled);

    // TEST A: Load DX7 patch 128 with KEEP FX + KEEP HARMONY + KEEP ARP
    now += 20;
    assert(backend.setPatch(128, now, perf_lock::kDefaultLockMask));
    runtime.apply(now);
    assert(runtime.snapshot().patch_state == amy::PatchState::Requested);
    runtime.loadPendingPatch();
    backend.publish(runtime.snapshot(), false, 0, 0);

    assert(runtime.snapshot().patch_state == amy::PatchState::Ready);
    assert(runtime.snapshot().patch_id == 128);
    // FX preserved
    assert(runtime.snapshot().fx.config.delay_mode == fx::DelayMode::Sync);
    assert(runtime.snapshot().fx.config.delay_division == tempo::Division::DottedEighth);
    assert(std::fabs(runtime.snapshot().fx.config.delay_feedback - 0.70f) < 1e-4f);
    assert(runtime.snapshot().fx.dirty);
    // Harmony preserved
    assert(runtime.snapshot().harmony.scale == 2);
    assert(runtime.snapshot().harmony.chord == 2);
    // Arp preserved
    assert(runtime.snapshot().arp.config.enabled);
    assert(runtime.snapshot().arp.config.rate == arp::Rate::Eighth);
    // No active notes
    assert(runtime.snapshot().active_notes == 0);

    // TEST B: Load DX7 patch 135 with NO LOCKS (clean load / discard unsaved preserved FX)
    now += 30;
    assert(backend.unsaved()); // Correctly identified as unsaved in 0.15.2a
    assert(backend.discardEditsAndSetPatch(135, now, perf_lock::None));
    runtime.apply(now);
    runtime.loadPendingPatch();
    backend.publish(runtime.snapshot(), false, 0, 0);

    assert(runtime.snapshot().patch_id == 135);
    // FX reset to default
    assert(runtime.snapshot().fx.config.delay_mode == fx::DelayMode::Free);
    assert(std::fabs(runtime.snapshot().fx.config.delay_mix - 0.0f) < 1e-4f);
    assert(!runtime.snapshot().fx.dirty);
    // Harmony reset to default
    assert(!runtime.snapshot().harmony.scale_on);
    assert(!runtime.snapshot().harmony.chord_on);
    assert(runtime.snapshot().harmony.scale == 1);
    assert(runtime.snapshot().harmony.chord == 1);
    // Arp disabled and reset to default
    assert(!runtime.snapshot().arp.config.enabled);
    assert(runtime.snapshot().arp.config.rate == arp::Rate::Sixteenth);

    // TEST C: Macro cross-family validation
    // Configure custom macro route targeting DX7-specific OpAttack0
    performance::RouteConfiguration custom_routes{};
    custom_routes.custom = true;
    custom_routes.routes = performance::kFactoryRoutes;
    custom_routes.routes[0][0] = performance::route(edit::OpAttack0, 0, 2000, 150.0f, performance::Fm);
    assert(backend.setPerformanceRoutes(custom_routes, 0));
    now += 40;
    backend.service(now);
    runtime.apply(now);
    backend.publish(runtime.snapshot(), false, 0, 0);
    assert(runtime.snapshot().edit.performance_routes.custom);

    // Load another DX7 patch (128) with KEEP MACROS: valid for FM, preserved!
    now += 50;
    assert(backend.patchHasUnsavedEdits());
    assert(backend.discardEditsAndSetPatch(128, now, perf_lock::Macros));
    runtime.apply(now);
    runtime.loadPendingPatch();
    backend.publish(runtime.snapshot(), false, 0, 0);
    assert(runtime.snapshot().patch_id == 128);
    assert(runtime.snapshot().edit.performance_routes.custom);

    // Load Juno patch 0 with KEEP MACROS: OpAttack0 is NOT in Juno's available mask,
    // so performance::valid fails and custom route is safely discarded for default routes!
    now += 60;
    assert(backend.discardEditsAndSetPatch(0, now, perf_lock::Macros));
    runtime.apply(now);
    runtime.loadPendingPatch();
    backend.publish(runtime.snapshot(), false, 0, 0);
    assert(runtime.snapshot().patch_id == 0);
    assert(!runtime.snapshot().edit.performance_routes.custom);

    runtime.end();
    std::printf("PASS patch_load_0152_test: selective preservation of FX, Harmony, ARP and Macro validation validated\n");
}
} // namespace

int main() {
    testPatchLoadPreservation();
    return 0;
}
