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

static edit::Preset makeValidPreset(uint32_t seq, const char* name, float cutoff = 1000.0f) {
    edit::Preset p{};
    p.sequence = seq;
    p.patch = 4; // Moving Strings (family 1)
    p.family = 1;
    for (unsigned m = 0; m < 3; ++m) p.macros[m] = 64;
    std::snprintf(p.name, sizeof(p.name), "%s", name);
    p.mask = (1ULL << edit::Cutoff);
    p.values[edit::Cutoff] = cutoff;
    return p;
}

// Helpers to create real binary records
static void writeRawV1(unsigned slot, unsigned bank, const edit::Preset& p) {
    uint8_t bytes[edit::kRecordBytes];
    assert(edit::encode(p, bytes));
    char key[12];
    std::snprintf(key, sizeof(key), "s%02u%c", slot, bank ? 'b' : 'a');
    fake_nvs::blobs[{1, key}].assign(bytes, bytes + sizeof(bytes));
}

static void writeRawV2(unsigned slot, unsigned bank, const edit::Preset& p) {
    uint8_t bytes[edit::kV2RecordBytes];
    assert(edit::encodeV2(p, bytes));
    char key[12];
    std::snprintf(key, sizeof(key), "v%02u%c", slot, bank ? 'b' : 'a');
    fake_nvs::blobs[{2, key}].assign(bytes, bytes + sizeof(bytes));
}

static void writeRawFx(unsigned slot, unsigned bank, const fx::FxConfig& cfg, uint16_t seq) {
    uint8_t bytes[fx::kFxRecordBytes];
    assert(fx::encodeRecord(cfg, seq, bytes));
    char key[12];
    std::snprintf(key, sizeof(key), "f%02u%c", slot, bank ? 'b' : 'a');
    fake_nvs::blobs[{3, key}].assign(bytes, bytes + sizeof(bytes));
}

int main() {
    std::puts("=== GATE A: First Transaction Over Legacy Presets Tests ===");

    const FaultInjection all_faults[] = {
        FaultInjection::BeforeFxWrite,
        FaultInjection::DuringFxWrite,
        FaultInjection::AfterFxWrite,
        FaultInjection::BeforeSynthWrite,
        FaultInjection::DuringSynthWrite,
        FaultInjection::AfterSynthWrite,
        FaultInjection::BeforeMarkerWrite,
        FaultInjection::DuringMarkerWrite
    };

    // ── Scenario 1: Only V1 present ─────────────────────────────────────────
    for (auto fault : all_faults) {
        fake_nvs::blobs.clear();
        fake_nvs::free_entries = 700;

        edit::Preset legacy_v1 = makeValidPreset(5, "LEGACY V1", 800.0f);
        writeRawV1(0, 0, legacy_v1);

        {
            NvsVersionedPresets synth_store;
            assert(synth_store.begin());
            fx::NvsFxPresets fx_store;
            assert(fx_store.begin());
            PresetTransactionCoordinator coord;
            coord.setSynthStore(&synth_store);
            coord.setFxStore(&fx_store);
            coord.setFaultInjection(fault);

            edit::Preset new_p = makeValidPreset(1, "MODERN OVERWRITE", 2000.0f);
            fx::FxConfig new_fx{};
            new_fx.chorus_mode = 2;

            auto res = coord.save(0, new_p, new_fx);
            assert(res.status == SaveResult::Status::FailedBeforeCommit);
        }

        // Reboot: recall must return legacy V1 with neutral FX
        {
            NvsVersionedPresets reboot_synth;
            assert(reboot_synth.begin());
            fx::NvsFxPresets reboot_fx;
            assert(reboot_fx.begin());
            PresetTransactionCoordinator reboot_coord;
            reboot_coord.setSynthStore(&reboot_synth);
            reboot_coord.setFxStore(&reboot_fx);

            edit::Preset rec_p{};
            fx::FxConfig rec_fx{};
            assert(reboot_coord.load(0, rec_p, rec_fx));
            assert(std::strcmp(rec_p.name, "LEGACY V1") == 0);
            assert(rec_p.patch == 4);
            assert(rec_p.values[edit::Cutoff] == 800.0f);
            assert(rec_fx.chorus_mode == 0); // Neutral default dry FX
        }
    }
    std::puts("  PASS: Scenario 1 - Only V1 present, rollback verified across all fault points");

    // ── Scenario 2: Only V2 present ─────────────────────────────────────────
    for (auto fault : all_faults) {
        fake_nvs::blobs.clear();
        fake_nvs::free_entries = 700;

        edit::Preset legacy_v2 = makeValidPreset(12, "LEGACY V2", 1500.0f);
        writeRawV2(0, 0, legacy_v2);

        {
            NvsVersionedPresets synth_store;
            assert(synth_store.begin());
            fx::NvsFxPresets fx_store;
            assert(fx_store.begin());
            PresetTransactionCoordinator coord;
            coord.setSynthStore(&synth_store);
            coord.setFxStore(&fx_store);
            coord.setFaultInjection(fault);

            edit::Preset new_p = makeValidPreset(1, "MODERN OVERWRITE", 2400.0f);
            fx::FxConfig new_fx{};
            new_fx.delay_time_ms = 500.0f;

            auto res = coord.save(0, new_p, new_fx);
            assert(res.status == SaveResult::Status::FailedBeforeCommit);
        }

        {
            NvsVersionedPresets reboot_synth;
            assert(reboot_synth.begin());
            fx::NvsFxPresets reboot_fx;
            assert(reboot_fx.begin());
            PresetTransactionCoordinator reboot_coord;
            reboot_coord.setSynthStore(&reboot_synth);
            reboot_coord.setFxStore(&reboot_fx);

            edit::Preset rec_p{};
            fx::FxConfig rec_fx{};
            assert(reboot_coord.load(0, rec_p, rec_fx));
            assert(std::strcmp(rec_p.name, "LEGACY V2") == 0);
            assert(rec_p.values[edit::Cutoff] == 1500.0f);
            assert(rec_fx.delay_time_ms == 350.0f); // Default neutral delay
        }
    }
    std::puts("  PASS: Scenario 2 - Only V2 present, rollback verified across all fault points");

    // ── Scenario 3: Both V1 and V2 present (V2 newer) ───────────────────────
    for (auto fault : all_faults) {
        fake_nvs::blobs.clear();
        fake_nvs::free_entries = 700;

        edit::Preset legacy_v1 = makeValidPreset(10, "OLD V1", 700.0f);
        writeRawV1(0, 0, legacy_v1);

        edit::Preset legacy_v2 = makeValidPreset(20, "NEWER V2", 1600.0f);
        writeRawV2(0, 0, legacy_v2);

        {
            NvsVersionedPresets synth_store;
            assert(synth_store.begin());
            fx::NvsFxPresets fx_store;
            assert(fx_store.begin());
            PresetTransactionCoordinator coord;
            coord.setSynthStore(&synth_store);
            coord.setFxStore(&fx_store);
            coord.setFaultInjection(fault);

            edit::Preset new_p = makeValidPreset(1, "INTERRUPTED NEW", 2100.0f);
            fx::FxConfig new_fx{};

            auto res = coord.save(0, new_p, new_fx);
            assert(res.status == SaveResult::Status::FailedBeforeCommit);
        }

        {
            NvsVersionedPresets reboot_synth;
            assert(reboot_synth.begin());
            fx::NvsFxPresets reboot_fx;
            assert(reboot_fx.begin());
            PresetTransactionCoordinator reboot_coord;
            reboot_coord.setSynthStore(&reboot_synth);
            reboot_coord.setFxStore(&reboot_fx);

            edit::Preset rec_p{};
            fx::FxConfig rec_fx{};
            assert(reboot_coord.load(0, rec_p, rec_fx));
            // Recovers NEWER V2
            assert(std::strcmp(rec_p.name, "NEWER V2") == 0);
            assert(rec_p.values[edit::Cutoff] == 1600.0f);
        }
    }
    std::puts("  PASS: Scenario 3 - V1 and V2 present, newer V2 rollback verified");

    // ── Scenario 4: 0.12 FX present with V2 Synth ───────────────────────────
    for (auto fault : all_faults) {
        fake_nvs::blobs.clear();
        fake_nvs::free_entries = 700;

        edit::Preset legacy_v2 = makeValidPreset(5, "SYNTH V2", 1100.0f);
        writeRawV2(0, 0, legacy_v2);

        fx::FxConfig legacy_fx{};
        legacy_fx.chorus_mode = 3; // Juno II
        legacy_fx.delay_time_ms = 440.0f;
        legacy_fx.delay_mix = 0.50f;
        writeRawFx(0, 0, legacy_fx, 7);

        {
            NvsVersionedPresets synth_store;
            assert(synth_store.begin());
            fx::NvsFxPresets fx_store;
            assert(fx_store.begin());
            PresetTransactionCoordinator coord;
            coord.setSynthStore(&synth_store);
            coord.setFxStore(&fx_store);
            coord.setFaultInjection(fault);

            edit::Preset new_p = makeValidPreset(1, "NEW SYNTH", 1900.0f);
            fx::FxConfig new_fx = legacy_fx;
            new_fx.chorus_mode = 1;

            auto res = coord.save(0, new_p, new_fx);
            assert(res.status == SaveResult::Status::FailedBeforeCommit);
        }

        {
            NvsVersionedPresets reboot_synth;
            assert(reboot_synth.begin());
            fx::NvsFxPresets reboot_fx;
            assert(reboot_fx.begin());
            PresetTransactionCoordinator reboot_coord;
            reboot_coord.setSynthStore(&reboot_synth);
            reboot_coord.setFxStore(&reboot_fx);

            edit::Preset rec_p{};
            fx::FxConfig rec_fx{};
            assert(reboot_coord.load(0, rec_p, rec_fx));
            assert(std::strcmp(rec_p.name, "SYNTH V2") == 0);
            assert(rec_fx.chorus_mode == 3);
            assert(approx(rec_fx.delay_time_ms, 440.0f));
            assert(approx(rec_fx.delay_mix, 0.50f));
        }
    }
    std::puts("  PASS: Scenario 4 - 0.12 FX present with V2 Synth, rollback verified");

    // ── Scenario 5: Bank A is latest ────────────────────────────────────────
    {
        fake_nvs::blobs.clear();
        fake_nvs::free_entries = 700;

        edit::Preset synth_a = makeValidPreset(30, "BANK A LATEST", 1400.0f);
        writeRawV2(0, 0, synth_a);

        edit::Preset synth_b = makeValidPreset(20, "BANK B OLD", 1100.0f);
        writeRawV2(0, 1, synth_b);

        NvsVersionedPresets synth_store;
        assert(synth_store.begin());
        fx::NvsFxPresets fx_store;
        assert(fx_store.begin());
        PresetTransactionCoordinator coord;
        coord.setSynthStore(&synth_store);
        coord.setFxStore(&fx_store);
        coord.setFaultInjection(FaultInjection::DuringSynthWrite);

        edit::Preset new_p = makeValidPreset(1, "INTERRUPT", 2200.0f);
        fx::FxConfig new_fx{};

        assert(coord.save(0, new_p, new_fx).status == SaveResult::Status::FailedBeforeCommit);

        // Active Bank A must survive untouched
        edit::Preset rec_p{};
        fx::FxConfig rec_fx{};
        assert(coord.load(0, rec_p, rec_fx));
        assert(std::strcmp(rec_p.name, "BANK A LATEST") == 0);
        assert(rec_p.sequence == 30);
    }
    std::puts("  PASS: Scenario 5 - Bank A is latest, candidate targeted Bank B and preserved A");

    // ── Scenario 6: Bank B is latest ────────────────────────────────────────
    {
        fake_nvs::blobs.clear();
        fake_nvs::free_entries = 700;

        edit::Preset synth_a = makeValidPreset(15, "BANK A OLD", 900.0f);
        writeRawV2(0, 0, synth_a);

        edit::Preset synth_b = makeValidPreset(45, "BANK B LATEST", 1700.0f);
        writeRawV2(0, 1, synth_b);

        NvsVersionedPresets synth_store;
        assert(synth_store.begin());
        fx::NvsFxPresets fx_store;
        assert(fx_store.begin());
        PresetTransactionCoordinator coord;
        coord.setSynthStore(&synth_store);
        coord.setFxStore(&fx_store);
        coord.setFaultInjection(FaultInjection::DuringSynthWrite);

        edit::Preset new_p = makeValidPreset(1, "INTERRUPT", 2300.0f);
        fx::FxConfig new_fx{};

        assert(coord.save(0, new_p, new_fx).status == SaveResult::Status::FailedBeforeCommit);

        // Active Bank B must survive untouched
        edit::Preset rec_p{};
        fx::FxConfig rec_fx{};
        assert(coord.load(0, rec_p, rec_fx));
        assert(std::strcmp(rec_p.name, "BANK B LATEST") == 0);
        assert(rec_p.sequence == 45);
    }
    std::puts("  PASS: Scenario 6 - Bank B is latest, candidate targeted Bank A and preserved B");

    // ── Scenario 7: One bank corrupt ────────────────────────────────────────
    {
        fake_nvs::blobs.clear();
        fake_nvs::free_entries = 700;

        // Bank A corrupt
        uint8_t garbage[edit::kV2RecordBytes] = { 0xde, 0xad, 0xbe, 0xef };
        fake_nvs::blobs[{2, "v00a"}].assign(garbage, garbage + sizeof(garbage));

        // Bank B valid
        edit::Preset synth_b = makeValidPreset(50, "VALID B", 1300.0f);
        writeRawV2(0, 1, synth_b);

        NvsVersionedPresets synth_store;
        assert(synth_store.begin());
        fx::NvsFxPresets fx_store;
        assert(fx_store.begin());
        PresetTransactionCoordinator coord;
        coord.setSynthStore(&synth_store);
        coord.setFxStore(&fx_store);
        coord.setFaultInjection(FaultInjection::DuringSynthWrite);

        edit::Preset new_p = makeValidPreset(1, "INTERRUPT", 2000.0f);
        fx::FxConfig new_fx{};

        assert(coord.save(0, new_p, new_fx).status == SaveResult::Status::FailedBeforeCommit);

        edit::Preset rec_p{};
        fx::FxConfig rec_fx{};
        assert(coord.load(0, rec_p, rec_fx));
        assert(std::strcmp(rec_p.name, "VALID B") == 0);
    }
    std::puts("  PASS: Scenario 7 - Corrupt bank ignored, valid bank preserved");

    // ── Scenario 8: Clean first transaction commit over legacy preset ───────
    {
        fake_nvs::blobs.clear();
        fake_nvs::free_entries = 700;

        edit::Preset legacy_p = makeValidPreset(8, "LEGACY CLEAN", 1100.0f);
        writeRawV2(0, 0, legacy_p);

        NvsVersionedPresets synth_store;
        assert(synth_store.begin());
        fx::NvsFxPresets fx_store;
        assert(fx_store.begin());
        PresetTransactionCoordinator coord;
        coord.setSynthStore(&synth_store);
        coord.setFxStore(&fx_store);

        edit::Preset new_p = makeValidPreset(1, "COMMITTED 0121", 2500.0f);
        fx::FxConfig new_fx{};
        new_fx.chorus_mode = 2;

        auto res = coord.save(0, new_p, new_fx);
        assert(res.status == SaveResult::Status::Success);
        assert(std::strcmp(res.message, "SAVED") == 0);

        // Verification after clean commit
        edit::Preset rec_p{};
        fx::FxConfig rec_fx{};
        assert(coord.load(0, rec_p, rec_fx));
        assert(std::strcmp(rec_p.name, "COMMITTED 0121") == 0);
        assert(rec_p.values[edit::Cutoff] == 2500.0f);
        assert(rec_fx.chorus_mode == 2);
    }
    std::puts("  PASS: Scenario 8 - Clean first transaction committed and verified");

    // ── Scenario 9: Empty slot failure ──────────────────────────────────────
    {
        fake_nvs::blobs.clear();
        fake_nvs::free_entries = 700;

        NvsVersionedPresets synth_store;
        assert(synth_store.begin());
        fx::NvsFxPresets fx_store;
        assert(fx_store.begin());
        PresetTransactionCoordinator coord;
        coord.setSynthStore(&synth_store);
        coord.setFxStore(&fx_store);
        coord.setFaultInjection(FaultInjection::DuringFxWrite);

        edit::Preset new_p = makeValidPreset(1, "FRESH SLOT", 1200.0f);
        fx::FxConfig new_fx{};

        assert(coord.save(0, new_p, new_fx).status == SaveResult::Status::FailedBeforeCommit);

        // On reboot: slot must still be empty!
        edit::Preset rec_p{};
        fx::FxConfig rec_fx{};
        assert(!coord.load(0, rec_p, rec_fx));
    }
    std::puts("  PASS: Scenario 9 - Empty slot interrupted save remains cleanly empty");

    // ── Scenario 10: All 16 slots populated and migrated ────────────────────
    {
        fake_nvs::blobs.clear();
        fake_nvs::free_entries = 700;

        // Populate all 16 slots with legacy presets
        for (unsigned s = 0; s < 16; ++s) {
            char name[32];
            std::snprintf(name, sizeof(name), "LEGACY_%02u", s);
            edit::Preset p = makeValidPreset(s + 1, name, 800.0f + 50.0f * s);
            writeRawV2(s, s % 2, p);
        }

        NvsVersionedPresets synth_store;
        assert(synth_store.begin());
        fx::NvsFxPresets fx_store;
        assert(fx_store.begin());
        PresetTransactionCoordinator coord;
        coord.setSynthStore(&synth_store);
        coord.setFxStore(&fx_store);

        // Migrate all 16 slots one by one to 0.12.1
        for (unsigned s = 0; s < 16; ++s) {
            char name[32];
            std::snprintf(name, sizeof(name), "MODERN_%02u", s);
            edit::Preset new_p = makeValidPreset(1, name, 1500.0f + 50.0f * s);
            fx::FxConfig new_fx{};
            new_fx.chorus_mode = 1 + (s % 5);

            auto res = coord.save(s, new_p, new_fx);
            assert(res.status == SaveResult::Status::Success);
        }

        // Reboot: verify all 16 slots recovered accurately
        NvsVersionedPresets reboot_synth;
        assert(reboot_synth.begin());
        fx::NvsFxPresets reboot_fx;
        assert(reboot_fx.begin());
        PresetTransactionCoordinator reboot_coord;
        reboot_coord.setSynthStore(&reboot_synth);
        reboot_coord.setFxStore(&reboot_fx);

        for (unsigned s = 0; s < 16; ++s) {
            edit::Preset rec_p{};
            fx::FxConfig rec_fx{};
            assert(reboot_coord.load(s, rec_p, rec_fx));
            char expected[32];
            std::snprintf(expected, sizeof(expected), "MODERN_%02u", s);
            assert(std::strcmp(rec_p.name, expected) == 0);
            assert(rec_fx.chorus_mode == 1 + (s % 5));
        }
    }
    std::puts("  PASS: Scenario 10 - All 16 slots migrated and verified after reboot");

    std::puts("ALL GATE A LEGACY MIGRATION TESTS PASSED.");
    return 0;
}
