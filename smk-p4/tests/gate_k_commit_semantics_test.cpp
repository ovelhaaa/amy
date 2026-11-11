#include "user_preset_nvs_v2.h"
#include "user_preset_fx_nvs.h"
#include "preset_transaction.h"
#include "local_performance_backend.h"
#include "fake_nvs.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>

using namespace smk::p4;
using namespace smk::p4::preset;

static edit::Preset makePreset(uint32_t seq, const char* name, float cutoff) {
    edit::Preset p{};
    p.sequence = seq;
    p.patch = 4;
    p.family = 1;
    for (unsigned m = 0; m < 3; ++m) p.macros[m] = 64;
    std::snprintf(p.name, sizeof(p.name), "%s", name);
    p.mask = (1ULL << edit::Cutoff);
    p.values[edit::Cutoff] = cutoff;
    return p;
}

int main() {
    std::puts("=== GATE B: Commit Semantics & Post-Marker Failure Tests ===");

    // ── 1. Setup clean baseline at Slot 2 ───────────────────────────────────
    fake_nvs::blobs.clear();
    fake_nvs::free_entries = 700;

    edit::Preset p_orig = makePreset(1, "BASE_REV1", 1000.0f);
    fx::FxConfig fx_orig{};
    fx_orig.chorus_mode = 2; // Juno I

    {
        NvsVersionedPresets synth_store;
        assert(synth_store.begin());
        fx::NvsFxPresets fx_store;
        assert(fx_store.begin());
        PresetTransactionCoordinator coord;
        coord.setSynthStore(&synth_store);
        coord.setFxStore(&fx_store);

        auto res = coord.save(2, p_orig, fx_orig);
        assert(res.status == SaveResult::Status::Success);
        assert(std::strcmp(res.message, "SAVED") == 0);
    }

    // ── 2. Fault Table Injections & Verification Matrix ─────────────────────
    edit::Preset p_new = makePreset(2, "NEW_REV2", 2200.0f);
    fx::FxConfig fx_new{};
    fx_new.chorus_mode = 3; // Juno II

    struct FaultCase {
        FaultInjection fault;
        SaveResult::Status expected_status;
        const char* expected_msg;
        const char* expected_name_after_reboot;
        uint8_t expected_chorus_after_reboot;
    };

    const FaultCase cases[] = {
        { FaultInjection::BeforeFxWrite,     SaveResult::Status::FailedBeforeCommit,   "SAVE FAILED: PRE-COMMIT",            "BASE_REV1", 2 },
        { FaultInjection::DuringFxWrite,     SaveResult::Status::FailedBeforeCommit,   "SAVE FAILED: PRE-COMMIT",            "BASE_REV1", 2 },
        { FaultInjection::AfterFxWrite,      SaveResult::Status::FailedBeforeCommit,   "SAVE FAILED: PRE-COMMIT",            "BASE_REV1", 2 },
        { FaultInjection::BeforeSynthWrite,  SaveResult::Status::FailedBeforeCommit,   "SAVE FAILED: PRE-COMMIT",            "BASE_REV1", 2 },
        { FaultInjection::DuringSynthWrite,  SaveResult::Status::FailedBeforeCommit,   "SAVE FAILED: PRE-COMMIT",            "BASE_REV1", 2 },
        { FaultInjection::AfterSynthWrite,   SaveResult::Status::FailedBeforeCommit,   "SAVE FAILED: PRE-COMMIT",            "BASE_REV1", 2 },
        { FaultInjection::BeforeMarkerWrite, SaveResult::Status::FailedBeforeCommit,   "SAVE FAILED: PRE-COMMIT",            "BASE_REV1", 2 },
        { FaultInjection::DuringMarkerWrite, SaveResult::Status::FailedBeforeCommit,   "SAVE FAILED: PRE-COMMIT",            "BASE_REV1", 2 },
        { FaultInjection::AfterMarkerWrite,  SaveResult::Status::Success,              "SAVED",                              "NEW_REV2",  3 },
        { FaultInjection::DuringReadback,    SaveResult::Status::CommitOutcomeUnknown, "SAVE INDETERMINATE: VERIFY PRESET",  "NEW_REV2",  3 },
    };

    for (const auto& tc : cases) {
        NvsVersionedPresets synth_store;
        assert(synth_store.begin());
        fx::NvsFxPresets fx_store;
        assert(fx_store.begin());
        PresetTransactionCoordinator coord;
        coord.setSynthStore(&synth_store);
        coord.setFxStore(&fx_store);
        coord.setFaultInjection(tc.fault);

        auto res = coord.save(2, p_new, fx_new);
        assert(res.status == tc.expected_status);
        assert(std::strcmp(res.message, tc.expected_msg) == 0);

        // Reboot check
        NvsVersionedPresets reboot_synth;
        assert(reboot_synth.begin());
        fx::NvsFxPresets reboot_fx;
        assert(reboot_fx.begin());
        PresetTransactionCoordinator reboot_coord;
        reboot_coord.setSynthStore(&reboot_synth);
        reboot_coord.setFxStore(&reboot_fx);

        edit::Preset rec_p{};
        fx::FxConfig rec_fx{};
        assert(reboot_coord.load(2, rec_p, rec_fx));
        assert(std::strcmp(rec_p.name, tc.expected_name_after_reboot) == 0);
        assert(rec_fx.chorus_mode == tc.expected_chorus_after_reboot);

        // If the test committed the new revision (AfterMarkerWrite / DuringReadback),
        // restore BASE_REV1 so subsequent tests start from known state
        if (std::strcmp(tc.expected_name_after_reboot, "NEW_REV2") == 0) {
            reboot_coord.setFaultInjection(FaultInjection::None);
            assert(reboot_coord.save(2, p_orig, fx_orig).status == SaveResult::Status::Success);
        }
    }
    std::puts("  PASS: Injected faults matrix verified according to logical commit boundary");

    // ── 3. Indeterminate Save Protection in LocalAmyPerformanceBackend ───────
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

        assert(backend.setPatch(4, 0));
        runtime.apply(0);
        runtime.loadPendingPatch();
        assert(runtime.render());
        backend.publish(runtime.snapshot(), false, 0, 0);

        // Initial save
        assert(backend.editParameter(edit::Cutoff, 1100.0f));
        backend.service(20);
        runtime.apply(20);
        backend.publish(runtime.snapshot(), false, 0, 0);

        assert(backend.editAction(2, 2, "SAFE_PRESET"));
        for (uint32_t t = 30; t <= 200; t += 25) {
            backend.service(t);
            runtime.apply(t);
            backend.publish(runtime.snapshot(), false, 0, 0);
            if (!backend.busy() && !backend.editPending()) break;
        }
        assert(std::strcmp(backend.editNotice(), "SAVED") == 0);
        assert(!backend.unsaved());

        // Modify parameter and attempt save with fault DuringReadback
        assert(backend.editParameter(edit::Cutoff, 2500.0f));
        backend.service(250);
        runtime.apply(250);
        backend.publish(runtime.snapshot(), false, 0, 0);
        assert(backend.unsaved());

        backend.transactionCoordinator().setFaultInjection(FaultInjection::DuringReadback);
        assert(backend.editAction(2, 2, "INDETERMINATE"));
        for (uint32_t t = 260; t <= 400; t += 25) {
            backend.service(t);
            runtime.apply(t);
            backend.publish(runtime.snapshot(), false, 0, 0);
            if (!backend.busy() && !backend.editPending()) break;
        }

        // Verified: notice reports indeterminate result
        assert(std::strcmp(backend.editNotice(), "SAVE INDETERMINATE: VERIFY PRESET") == 0);
        assert(backend.editError());

        // Verified: destructive patch change (action 3 - recall) is strictly blocked!
        assert(backend.unsaved());
        assert(!backend.editAction(3, 2)); // Blocked because unsaved is true
        edit::Preset verified{};
        fx::FxConfig verified_fx{};
        assert(backend.transactionCoordinator().load(2,verified,verified_fx));
        const auto sequence=verified.sequence;
        assert(!backend.editAction(2,2,"MUST NOT OVERWRITE"));
        assert(backend.presetOperationStatus().state==PresetOperationState::Confirmed);
        assert(backend.transactionCoordinator().load(2,verified,verified_fx));
        assert(verified.sequence==sequence);
        assert(std::strcmp(verified.name,"INDETERMINATE")==0);
    }
    std::puts("  PASS: Indeterminate save blocks destructive patch changes in UI/backend");

    std::puts("ALL GATE B COMMIT SEMANTICS TESTS PASSED.");
    return 0;
}
