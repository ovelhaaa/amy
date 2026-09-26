// Run with tests/run_smk_patch_integrity.ps1.
//
// M3.1 real-AMY regression: the requested reverb-freeze state must reach the
// actual DSP reverb tank even though AMY allocates that tank lazily (only once
// reverb level > 0). The engine is started fresh here, so the tank genuinely
// begins absent; every assertion reads amy_global.bus[0]->reverb.rev, not a
// mock. The chain under test is PatchManager + real AmyAdapter + real AMY.
#include "patch_manager.h"
#include "factory_patches.h"
#include "storage_manager.h"
#include "amy_adapter.h"
#include "synth_config.h"
#include "diagnostics.h"
#include "ui_manager.h"
#include "clock_manager.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace smk {
Diagnostics& Diagnostics::instance() { static Diagnostics instance; return instance; }
DiagnosticCounters& Diagnostics::counters() { return counters_; }
struct AmyAdapterTestAccess {
    static bool service(AmyAdapter& adapter) { return adapter.serviceBlock(); }
};

// UI/clock stubs required to link PatchManager. The test never drives the UI.
void ClockManager::setBpm(float) {}
void HomeScreen::setPatchInfo(uint16_t, const char*, const char*) {}
void HomeScreen::setMacroValues(const uint8_t[8]) {}
void HomeScreen::setMacroLabels(const char[8][8]) {}
void HomeScreen::setEngineValues(const uint8_t[8]) {}
void HomeScreen::setHomeKnobBankView(HomeKnobBankView) {}
void HomeScreen::setKnobBankLabel(const char*) {}
void HomeScreen::setActiveVoices(uint8_t, uint8_t) {}
void UIManager::triggerParameterOverlay(const char*, const char*, float, float, const char*, TakeoverStatus) {}
}

extern "C" {
#include "amy.h"
void delay_ms(uint32_t) {}
void amy_update_tasks() {}
int16_t* amy_render_audio() { return amy_simple_fill_buffer(); }
size_t amy_i2s_write(const uint8_t*, size_t nbytes) { return nbytes; }
struct FmAlgorithm { uint8_t ops[MAX_ALGO_OPS]; };
extern const struct FmAlgorithm algorithms[33];
}

static void process(smk::AmyAdapter& adapter, int blocks) {
    for (int b = 0; b < blocks; ++b) {
        assert(smk::AmyAdapterTestAccess::service(adapter));
        adapter.render();
    }
}

// ── Direct views into real AMY DSP state (never a mock) ─────────────────────
static reverb_params_t* tank() {
    return amy_global.bus[0] ? amy_global.bus[0]->reverb.rev : nullptr;
}
static bool tank_materialized() {
    reverb_params_t* r = tank();
    return r != nullptr && r->delay_1 != nullptr;
}
static uint8_t tank_freeze() {
    reverb_params_t* r = tank();
    return r ? r->freeze : 0;
}

int main() {
    smk::AmyAdapter adapter;
    assert(adapter.begin(AMY_SAMPLE_RATE));
    smk::PatchManager pm;
    assert(pm.begin(&adapter, nullptr));
    process(adapter, 4);

    auto make_patch = [](uint8_t id, float mix, float size, uint8_t freeze) {
        smk::SynthPatch p = *smk::FactoryPatches::getPatchById(id);
        p.reverb_mix = mix;
        p.reverb_size = size;
        p.reverb_freeze = freeze;
        return p;
    };

    // ── 0. Fresh engine baseline: dry patch, no tank yet. ───────────────────
    assert(!tank_materialized());
    assert(!adapter.reverbFreeze());
    std::printf("PASS: fresh engine starts with no reverb tank and freeze off\n");

    // ── 1. Freeze requested while the tank is absent must not allocate it. ───
    // This is the exact fresh-boot failure precondition: freeze is applied while
    // reverb.rev == nullptr and would previously be lost.
    assert(pm.applyLoadedPatch(make_patch(0, 0.0f, 0.8f, 1)));
    process(adapter, 6);
    assert(!tank_materialized());
    assert(adapter.reverbFreeze());
    std::printf("PASS: freeze ON with mix=0 stores the request and allocates no tank\n");

    // ── 2. Lazy materialization applies the previously requested freeze. ─────
    // Freeze was queued before any reverb level in an earlier block, so this is
    // the Freeze -> Reverb command order on a tank that did not exist yet.
    adapter.setReverb(0.8f, 0.5f, 0.5f);
    process(adapter, 6);
    assert(tank_materialized());
    assert(tank_freeze() == 1);
    assert(adapter.reverbFreeze());
    std::printf("PASS: lazily materialized tank inherits the previously requested freeze\n");

    // ── 3. Order independence: Reverb -> Freeze (opposite order). ────────────
    adapter.setReverbFreeze(false);
    adapter.setReverb(0.8f, 0.5f, 0.0f);
    process(adapter, 6);
    assert(!adapter.reverbFreeze());
    assert(tank_freeze() == 0);
    adapter.setReverb(0.8f, 0.5f, 0.5f);
    adapter.setReverbFreeze(true);
    process(adapter, 6);
    assert(adapter.reverbFreeze());
    assert(tank_freeze() == 1);
    std::printf("PASS: Reverb -> Freeze order reaches the same frozen tank\n");

    // ── 4. Freeze OFF after materialization disables the live tank. ──────────
    adapter.setReverbFreeze(false);
    process(adapter, 4);
    assert(!adapter.reverbFreeze());
    assert(tank_freeze() == 0);
    std::printf("PASS: freeze OFF after materialization clears the live tank\n");

    // ── 5. Patch A (frozen) -> patch B (unfrozen): no ghost freeze. ─────────
    assert(pm.applyLoadedPatch(make_patch(0, 0.7f, 0.7f, 1)));
    process(adapter, 6);
    assert(tank_freeze() == 1);
    assert(adapter.reverbFreeze());
    assert(pm.applyLoadedPatch(make_patch(1, 0.7f, 0.7f, 0)));
    process(adapter, 6);
    assert(tank_freeze() == 0);
    assert(!adapter.reverbFreeze());
    std::printf("PASS: patch A frozen -> patch B unfrozen leaves no ghost freeze\n");

    // ── 6. Patch A (frozen) -> patch B (dry + unfrozen): no ghost freeze. ───
    assert(pm.applyLoadedPatch(make_patch(0, 0.7f, 0.7f, 1)));
    process(adapter, 6);
    assert(tank_freeze() == 1);
    assert(pm.applyLoadedPatch(make_patch(1, 0.0f, 0.7f, 0)));
    process(adapter, 6);
    assert(!adapter.reverbFreeze());
    if (tank_materialized()) assert(tank_freeze() == 0);
    std::printf("PASS: patch A frozen -> patch B dry leaves no ghost freeze\n");

    // ── 7. Save/reload preserves the requested and actual freeze. ────────────
    {
        const char* dir = "build/smk_patch_integrity/reverb_freeze_storage";
        std::filesystem::create_directories(dir);
        smk::StorageManager storage;
        assert(storage.begin(dir));

        assert(pm.applyLoadedPatch(make_patch(0, 0.5f, 0.6f, 1)));
        process(adapter, 6);
        assert(tank_freeze() == 1);

        const smk::SynthPatch persisted = pm.buildPersistablePatch();
        assert(persisted.reverb_freeze == 1);
        assert(std::fabs(persisted.reverb_mix - 0.5f) < 1e-4f);
        assert(storage.savePatch(7, persisted));

        smk::SynthPatch loaded = {};
        assert(storage.loadPatch(7, loaded));
        assert(loaded.reverb_freeze == 1);
        assert(pm.applyLoadedPatch(loaded, 7));
        process(adapter, 6);
        assert(adapter.reverbFreeze());
        assert(tank_freeze() == 1);
        std::printf("PASS: reverb freeze round-trips through v6 save/load and the live tank\n");
    }

    std::printf("PASS: M3.1 requested reverb freeze is reconciled onto the real DSP tank\n");
    return 0;
}
