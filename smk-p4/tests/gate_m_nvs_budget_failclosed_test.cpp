#include "user_preset_nvs_v2.h"
#include "user_preset_fx_nvs.h"
#include "preset_transaction.h"
#include "fake_nvs.h"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace smk::p4;
using namespace smk::p4::preset;

static edit::Preset makePreset(uint32_t seq, const char* name) {
    edit::Preset p{};
    p.sequence = seq;
    p.patch = 4;
    p.family = 1;
    for (unsigned m = 0; m < 3; ++m) p.macros[m] = 64;
    std::snprintf(p.name, sizeof(p.name), "%s", name);
    p.mask = (1ULL << edit::Cutoff);
    p.values[edit::Cutoff] = 1200.0f;
    return p;
}

int main() {
    std::puts("=== GATE D: NVS Budget Fail-Closed & Hardware Error Tests ===");

    // ── 1. Sufficient Space ─────────────────────────────────────────────────
    fake_nvs::blobs.clear();
    fake_nvs::free_entries = 700;
    fake_nvs::stats_error = false;
    fake_nvs::read_error = false;
    fake_nvs::commit_error = false;
    fake_nvs::torn = false;

    fx::NvsFxPresets fx_store;
    assert(fx_store.begin());
    assert(fx_store.checkNvsBudget(4));
    assert(fx_store.checkNvsBudget(12));
    std::puts("  PASS: Sufficient NVS space approves budget check");

    // ── 2. Exact Boundary & NVS Full (gc_entries = 126) ─────────────────────
    // For required_entries = 4: threshold is 126 + 4 = 130
    fake_nvs::free_entries = 130; // exactly at boundary
    assert(fx_store.checkNvsBudget(4));

    fake_nvs::free_entries = 129; // 1 below boundary -> FAIL
    assert(!fx_store.checkNvsBudget(4));
    assert(fx_store.error() == ESP_ERR_NVS_NOT_ENOUGH_SPACE);

    // Completely full (0 entries)
    fake_nvs::free_entries = 0;
    assert(!fx_store.checkNvsBudget(4));
    assert(fx_store.error() == ESP_ERR_NVS_NOT_ENOUGH_SPACE);
    std::puts("  PASS: Exact boundary and full NVS rejected fail-closed");

    // ── 3. nvs_get_stats failure must FAIL CLOSED ────────────────────────────
    fake_nvs::free_entries = 700; // plenty of space
    fake_nvs::stats_error = true;  // but query fails!
    assert(!fx_store.checkNvsBudget(4));
    assert(fx_store.error() == ESP_FAIL);

    // Save must not even attempt to write when stats query fails
    fake_nvs::writes = 0;
    fx::FxConfig cfg{};
    assert(!fx_store.saveFx(0, cfg));
    assert(fake_nvs::writes == 0); // ZERO writes attempted!
    std::puts("  PASS: nvs_get_stats error strictly aborts before write");

    fake_nvs::stats_error = false;

    // ── 4. Setup initial valid committed preset ─────────────────────────────
    NvsVersionedPresets synth_store;
    assert(synth_store.begin());
    PresetTransactionCoordinator coord;
    coord.setSynthStore(&synth_store);
    coord.setFxStore(&fx_store);

    edit::Preset p_base = makePreset(1, "COMMITTED_BASE");
    fx::FxConfig fx_base{};
    fx_base.chorus_mode = 2;
    assert(coord.save(0, p_base, fx_base).status == SaveResult::Status::Success);

    // ── 5. Error in nvs_set_blob (simulated via torn / write failure) ────────
    fake_nvs::torn = true;
    edit::Preset p_candidate = makePreset(2, "TORN_WRITE");
    fx::FxConfig fx_candidate{};
    auto res_torn = coord.save(0, p_candidate, fx_candidate);
    assert(res_torn.status == SaveResult::Status::FailedBeforeCommit);
    fake_nvs::torn = false;

    // Reboot: recovery returns COMMITTED_BASE
    {
        PresetTransactionCoordinator rec_coord;
        rec_coord.setSynthStore(&synth_store);
        rec_coord.setFxStore(&fx_store);
        edit::Preset rec_p{};
        fx::FxConfig rec_fx{};
        assert(rec_coord.load(0, rec_p, rec_fx));
        assert(std::strcmp(rec_p.name, "COMMITTED_BASE") == 0);
        assert(rec_fx.chorus_mode == 2);
    }
    std::puts("  PASS: nvs_set_blob write error preserves committed baseline");

    // ── 6. Error in nvs_commit ──────────────────────────────────────────────
    fake_nvs::commit_error = true;
    auto res_commit_err = coord.save(0, p_candidate, fx_candidate);
    assert(res_commit_err.status == SaveResult::Status::FailedBeforeCommit);
    fake_nvs::commit_error = false;

    {
        PresetTransactionCoordinator rec_coord;
        rec_coord.setSynthStore(&synth_store);
        rec_coord.setFxStore(&fx_store);
        edit::Preset rec_p{};
        fx::FxConfig rec_fx{};
        assert(rec_coord.load(0, rec_p, rec_fx));
        assert(std::strcmp(rec_p.name, "COMMITTED_BASE") == 0);
    }
    std::puts("  PASS: nvs_commit error preserves committed baseline");

    // ── 7. Readback Hardware I/O Error ──────────────────────────────────────
    fake_nvs::read_error = true;
    // When reading hardware fails, checkNvsBudget or inspectLegacySlot fails closed
    {
        PresetTransactionCoordinator rec_coord;
        rec_coord.setSynthStore(&synth_store);
        rec_coord.setFxStore(&fx_store);
        edit::Preset rec_p{};
        fx::FxConfig rec_fx{};
        assert(!rec_coord.load(0, rec_p, rec_fx)); // Fails closed, does not corrupt
    }
    fake_nvs::read_error = false;

    // Normal read recovers baseline
    {
        PresetTransactionCoordinator rec_coord;
        rec_coord.setSynthStore(&synth_store);
        rec_coord.setFxStore(&fx_store);
        edit::Preset rec_p{};
        fx::FxConfig rec_fx{};
        assert(rec_coord.load(0, rec_p, rec_fx));
        assert(std::strcmp(rec_p.name, "COMMITTED_BASE") == 0);
    }
    std::puts("  PASS: Readback error handled cleanly without corrupting persisted state");

    std::puts("ALL GATE D NVS BUDGET FAIL-CLOSED TESTS PASSED.");
    return 0;
}
