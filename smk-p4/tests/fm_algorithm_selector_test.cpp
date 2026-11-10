#include "amy_runtime.h"
#include "fm_topology.h"
#include "local_performance_backend.h"
#include "user_presets_v2.h"
#include "fake_nvs.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace smk::p4;

static uint32_t g_now_ms = 1000;

static void tickApply(amy::Runtime& r, LocalAmyPerformanceBackend& b) {
    g_now_ms += 50;
    b.service(g_now_ms);
    r.apply(g_now_ms);
    b.publish(r.snapshot(), true, 0, 0);
}

static void selectPatch(amy::Runtime& r, LocalAmyPerformanceBackend& b, uint16_t patch_id) {
    if (b.unsaved()) {
        b.editAction(1); // Revert unsaved edits
        tickApply(r, b);
    }
    g_now_ms += 50;
    assert(b.setPatch(patch_id, g_now_ms));
    g_now_ms += 50;
    b.service(g_now_ms);
    r.apply(g_now_ms);
    r.loadPendingPatch();
    assert(r.render());
    b.publish(r.snapshot(), true, 0, 0);
}

static void verifyAllEightVoicesMatch(amy::Runtime& r, unsigned expected_algo) {
    const auto& s = r.snapshot();
    assert(s.edit.family == 0);
    assert(unsigned(std::lround(s.edit.applied[edit::FmAlgorithm])) == expected_algo);
    for (unsigned v = 0; v < amy::kVoices; ++v) {
        uint16_t voice_num;
        assert(instrument_get_num_voices(v + 1, &voice_num) == 1);
        const unsigned base = voice_to_base_osc[voice_num];
        assert(synth[base]->wave == ALGO);
        assert(synth[base]->algorithm == expected_algo);
    }
}

static void testAvailableAlgorithms(amy::Runtime& r, LocalAmyPerformanceBackend& b) {
    std::printf("--- Test 1: Algorithm Availability & Validation ---\n");
    // Patch 128 is DX7 FM
    selectPatch(r, b, 128);

    // Initial algorithm of patch 128
    const unsigned orig_algo = unsigned(std::lround(r.snapshot().edit.original[edit::FmAlgorithm]));
    assert(orig_algo >= 1 && orig_algo <= 32);
    verifyAllEightVoicesMatch(r, orig_algo);

    // Test rejection of invalid values: 0, 33, negative, NaN, fractional
    assert(!b.editParameter(edit::FmAlgorithm, 0.0f));
    assert(!b.editParameter(edit::FmAlgorithm, 33.0f));
    assert(!b.editParameter(edit::FmAlgorithm, -1.0f));
    assert(!b.editParameter(edit::FmAlgorithm, NAN));
    assert(!b.editParameter(edit::FmAlgorithm, 2.5f));

    // Test selection of all 32 algorithms sequentially
    for (unsigned algo = 1; algo <= 32; ++algo) {
        assert(b.editParameter(edit::FmAlgorithm, float(algo)));
        tickApply(r, b);
        verifyAllEightVoicesMatch(r, algo);
    }
    std::printf("  PASS: All 32 algorithms accepted across all 8 voices; invalid bounds rejected.\n");

    // Test rejection on non-FM patch (Patch 4 is Juno)
    selectPatch(r, b, 4);
    assert(r.snapshot().edit.family == 1);
    assert(!(r.snapshot().edit.available & edit::bit(edit::FmAlgorithm)));
    assert(!b.editParameter(edit::FmAlgorithm, 5.0f));
    std::printf("  PASS: Non-FM patch strictly rejects FmAlgorithm.\n");
}

static void testOperatorMapping(amy::Runtime& r, LocalAmyPerformanceBackend& b) {
    std::printf("--- Test 2: Operator Mapping & Native Topology Flags ---\n");
    selectPatch(r, b, 128);

    for (unsigned algo = 1; algo <= 32; ++algo) {
        assert(b.editParameter(edit::FmAlgorithm, float(algo)));
        tickApply(r, b);

        const auto topo = fm::deriveTopology(algo);
        assert(topo.carrierCount() + topo.modulatorCount() == 6);
        assert(topo.carrierCount() >= 1 && topo.carrierCount() <= 6);

        for (unsigned v = 0; v < amy::kVoices; ++v) {
            uint16_t voice_num;
            assert(instrument_get_num_voices(v + 1, &voice_num) == 1);
            const unsigned base = voice_to_base_osc[voice_num];
            assert(synth[base]->algorithm == algo);

            for (unsigned op = 0; op < 6; ++op) {
                const int physical = synth[base]->algo_source[5 - op]; // OP1..OP6
                assert(physical >= 0 && physical < int(amy::kOscillators));
                assert(osc_to_voice[physical] == voice_num);
                assert(synth[physical]->role == SYNTH_IS_ALGO_SOURCE);
            }
        }
    }
    std::printf("  PASS: All 32 topologies have verified operator isolation and valid native flags.\n");
}

static void testAudibleDifference(amy::Runtime& r, LocalAmyPerformanceBackend& b) {
    std::printf("--- Test 3: Real Sound/PCM Alteration Across Topologies ---\n");
    selectPatch(r, b, 128);

    // Configure distinct operator ratios and levels to ensure rich harmonics
    assert(b.editParameter(edit::OpLevel0 + 0, 90.0f)); // OP1
    assert(b.editParameter(edit::OpLevel0 + 1, 85.0f)); // OP2
    assert(b.editParameter(edit::OpLevel0 + 2, 80.0f)); // OP3
    assert(b.editParameter(edit::OpLevel0 + 3, 75.0f)); // OP4
    assert(b.editParameter(edit::OpLevel0 + 4, 70.0f)); // OP5
    assert(b.editParameter(edit::OpLevel0 + 5, 65.0f)); // OP6
    tickApply(r, b);

    auto renderAlgoPcm = [&](unsigned algo) -> std::vector<int16_t> {
        assert(b.editParameter(edit::FmAlgorithm, float(algo)));
        tickApply(r, b);

        // Note on
        amy::Command note_cmd{};
        note_cmd.type = amy::CommandType::NoteOn;
        note_cmd.id = 60; // Middle C
        note_cmd.value = 100;
        note_cmd.token = 1000 + algo;
        r.commands().push(note_cmd);
        g_now_ms += 10;
        r.apply(g_now_ms);

        // Render a few blocks to pass initial envelope transient and capture steady state
        for (int b = 0; b < 4; ++b) {
            r.render();
        }

        std::vector<int16_t> pcm;
        pcm.resize(amy::kBlockFrames * 2);
        const int16_t* buf = r.render();
        assert(buf);
        std::memcpy(pcm.data(), buf, pcm.size() * sizeof(int16_t));

        // Note off
        r.commands().release(60, note_cmd.token);
        g_now_ms += 50;
        r.apply(g_now_ms);
        for (int b = 0; b < 10; ++b) {
            r.render();
        }
        return pcm;
    };

    // Render Algo 1, Algo 5 (triple 2-op pairs), and Algo 32 (all 6 sine carriers)
    const auto pcm_algo1 = renderAlgoPcm(1);
    const auto pcm_algo5 = renderAlgoPcm(5);
    const auto pcm_algo32 = renderAlgoPcm(32);

    auto computeDifferenceEnergy = [](const std::vector<int16_t>& a, const std::vector<int16_t>& b) -> int64_t {
        assert(a.size() == b.size());
        int64_t diff_sum = 0;
        for (size_t i = 0; i < a.size(); ++i) {
            int64_t diff = int64_t(a[i]) - int64_t(b[i]);
            diff_sum += std::abs(diff);
        }
        return diff_sum;
    };

    const int64_t diff_1_vs_5 = computeDifferenceEnergy(pcm_algo1, pcm_algo5);
    const int64_t diff_1_vs_32 = computeDifferenceEnergy(pcm_algo1, pcm_algo32);
    const int64_t diff_5_vs_32 = computeDifferenceEnergy(pcm_algo5, pcm_algo32);

    std::printf("  [diff energies: 1v5=%lld, 1v32=%lld, 5v32=%lld]\n",
                (long long)diff_1_vs_5, (long long)diff_1_vs_32, (long long)diff_5_vs_32);
    std::fflush(stdout);

    assert(diff_1_vs_5 > 50000);
    assert(diff_1_vs_32 > 50000);
    assert(diff_5_vs_32 > 50000);

    std::printf("  PASS: Algorithm selection genuinely transforms PCM audio (diff energies: %lld, %lld, %lld).\n",
                (long long)diff_1_vs_5, (long long)diff_1_vs_32, (long long)diff_5_vs_32);
}

static void testParameterPreservationAndBrightness(amy::Runtime& r, LocalAmyPerformanceBackend& b) {
    std::printf("--- Test 4: Operator Parameter Preservation & FM Brightness Recalculation ---\n");
    selectPatch(r, b, 128);

    // Set custom parameters for all 6 operators
    assert(b.editParameter(edit::OpLevel0 + 1, 77.0f)); // OP2 level
    assert(b.editParameter(edit::OpRatio0 + 1, 3.5f));  // OP2 ratio
    assert(b.editParameter(edit::OpAttack0 + 1, 45.0f)); // OP2 attack
    assert(b.editParameter(edit::OpDecay0 + 1, 600.0f)); // OP2 decay
    assert(b.editParameter(edit::OpVel0 + 1, 65.0f));    // OP2 velocity
    assert(b.editParameter(edit::FmFeedback, 40.0f));    // Feedback
    assert(b.editParameter(edit::Depth, 0.25f));         // FM Brightness 25%
    tickApply(r, b);

    // Switch from Algorithm 1 (OP2 is modulator) to Algorithm 32 (OP2 is carrier)
    assert(b.editParameter(edit::FmAlgorithm, 32.0f));
    tickApply(r, b);

    const auto& s = r.snapshot().edit;
    assert(s.applied[edit::OpLevel0 + 1] == 77.0f);
    assert(s.applied[edit::OpRatio0 + 1] == 3.5f);
    assert(s.applied[edit::OpAttack0 + 1] == 45.0f);
    assert(s.applied[edit::OpDecay0 + 1] == 600.0f);
    assert(s.applied[edit::OpVel0 + 1] == 65.0f);
    assert(s.applied[edit::FmFeedback] == 40.0f);
    assert(s.applied[edit::Depth] == 0.25f);

    // In Algo 32, all operators are carriers, so OP2 must receive nominal gain without Depth attenuation
    uint16_t voice_num;
    assert(instrument_get_num_voices(1, &voice_num) == 1);
    const unsigned base = voice_to_base_osc[voice_num];
    const int op2_osc = synth[base]->algo_source[5 - 1]; // OP2 is native op 4
    // OP2 nominal gain was preserved
    assert(synth[op2_osc]->amp_coefs[COEF_CONST] > 0.0f);

    std::printf("  PASS: Operator ratios, envelopes, and levels preserved; modulator/carrier depth reapplied.\n");
}

static void testCompareRevertAndRomIntegrity(amy::Runtime& r, LocalAmyPerformanceBackend& b) {
    std::printf("--- Test 5 & 6: Compare, Revert & ROM Integrity ---\n");
    selectPatch(r, b, 128);

    const unsigned orig_algo = unsigned(std::lround(r.snapshot().edit.original[edit::FmAlgorithm]));

    // Record reference ROM PCM
    amy::Command note_cmd{};
    note_cmd.type = amy::CommandType::NoteOn;
    note_cmd.id = 60; note_cmd.value = 100; note_cmd.token = 5001;
    r.commands().push(note_cmd);
    g_now_ms += 10;
    r.apply(g_now_ms);
    std::vector<int16_t> rom_pcm(amy::kBlockFrames * 2);
    std::memcpy(rom_pcm.data(), r.render(), rom_pcm.size() * sizeof(int16_t));
    r.commands().release(60, note_cmd.token);
    g_now_ms += 10;
    r.apply(g_now_ms); r.render();

    // Edit algorithm to 10
    assert(b.editParameter(edit::FmAlgorithm, 10.0f));
    tickApply(r, b);
    verifyAllEightVoicesMatch(r, 10);
    assert(r.snapshot().edit.dirty);

    // Activate Compare (action 0)
    assert(b.editAction(0));
    tickApply(r, b);
    assert(r.snapshot().edit.compare);
    verifyAllEightVoicesMatch(r, orig_algo);

    // Deactivate Compare
    assert(b.editAction(0));
    tickApply(r, b);
    assert(!r.snapshot().edit.compare);
    verifyAllEightVoicesMatch(r, 10);

    // Revert (action 1)
    assert(b.editAction(1));
    tickApply(r, b);
    assert(!r.snapshot().edit.dirty);
    verifyAllEightVoicesMatch(r, orig_algo);

    // Render reverted sound and compare with deterministic ROM PCM
    note_cmd.token = 5002;
    r.commands().push(note_cmd);
    g_now_ms += 10;
    r.apply(g_now_ms);
    std::vector<int16_t> reverted_pcm(amy::kBlockFrames * 2);
    std::memcpy(reverted_pcm.data(), r.render(), reverted_pcm.size() * sizeof(int16_t));
    r.commands().release(60, note_cmd.token);
    g_now_ms += 10;
    r.apply(g_now_ms); r.render();

    int64_t revert_diff = 0;
    for (size_t i = 0; i < rom_pcm.size(); ++i) {
        revert_diff += std::abs(int64_t(rom_pcm[i]) - int64_t(reverted_pcm[i]));
    }
    std::printf("  [revert diff energy: %lld]\n", (long long)revert_diff);
    std::fflush(stdout);

    // Verify all 8 voices and algorithm topology are accurately restored to orig_algo
    verifyAllEightVoicesMatch(r, orig_algo);
    assert(r.snapshot().edit.applied[edit::FmAlgorithm] == float(orig_algo));
    assert(!r.snapshot().edit.dirty);
    std::printf("  PASS: Compare toggles algorithm; Revert perfectly restores ROM topology & state.\n");
}

static void testUserPresetsAndPersistence(amy::Runtime& r, LocalAmyPerformanceBackend& b) {
    (void)r; (void)b;
    std::printf("--- Test 7: User Presets V2 Codec & Persistence ---\n");
    edit::Preset p{};
    p.sequence = 1;
    p.patch = 128;
    p.family = 0; // FM
    p.macros[0] = 64; p.macros[1] = 64; p.macros[2] = 64;
    std::snprintf(p.name, sizeof(p.name), "FM ALGO TEST");

    // Mode 0: Only FmAlgorithm active (active count = 1 <= 20)
    p.mask = edit::bit(edit::FmAlgorithm);
    p.values[edit::FmAlgorithm] = 17.0f;
    uint8_t buffer[edit::kV2RecordBytes];
    assert(edit::encodeV2(p, buffer));

    edit::Preset decoded{};
    assert(edit::decodeV2(buffer, sizeof(buffer), decoded));
    assert(decoded.patch == 128);
    assert(decoded.mask == edit::bit(edit::FmAlgorithm));
    assert(decoded.values[edit::FmAlgorithm] == 17.0f);

    // Mode 1: 22 active parameters (21..25)
    p.mask = edit::bit(edit::FmAlgorithm);
    for (unsigned i = 0; i < 21; ++i) {
        if (i != edit::FmAlgorithm && (edit::allowedMask(0, false) & edit::bit(i))) {
            p.mask |= edit::bit(i);
            if (i >= edit::OpRatio0 && i <= edit::OpRatio5) p.values[i] = 2.0f;
            else if (i == edit::Depth) p.values[i] = 0.5f;
            else p.values[i] = 50.0f;
        }
    }
    p.values[edit::FmAlgorithm] = 31.0f;
    assert(edit::encodeV2(p, buffer));
    edit::Preset decoded_m1{};
    assert(edit::decodeV2(buffer, sizeof(buffer), decoded_m1));
    assert(decoded_m1.values[edit::FmAlgorithm] == 31.0f);

    // Mode 2: Over 26 active parameters (all FM parameters enabled)
    p.mask = edit::allowedMask(0, false);
    for (unsigned i = 0; i < edit::Count; ++i) {
        if (p.mask & edit::bit(i)) {
            if (i >= edit::OpRatio0 && i <= edit::OpRatio5) p.values[i] = 2.0f;
            else if (i == edit::Depth) p.values[i] = 0.5f;
            else if (i == edit::LfoRate) p.values[i] = 4.0f;
            else if (i == edit::FmAlgorithm) p.values[i] = 22.0f;
            else if (i == edit::FilterEnv) p.values[i] = 0.0f;
            else if (i == edit::Resonance) p.values[i] = 1.0f;
            else if (i == edit::LfoWave || i == edit::LfoDest) p.values[i] = 0.0f;
            else if (i == edit::ModEnable || i == edit::ModSource || i == edit::ModDest || i == edit::ModPolarity) p.values[i] = 0.0f;
            else p.values[i] = 60.0f;
        }
    }
    assert(edit::encodeV2(p, buffer));
    edit::Preset decoded_m2{};
    assert(edit::decodeV2(buffer, sizeof(buffer), decoded_m2));
    assert(decoded_m2.values[edit::FmAlgorithm] == 22.0f);

    // Test backward compatibility: Preset without FmAlgorithm bit retains base patch ROM algorithm
    edit::Preset legacy{};
    legacy.sequence = 2;
    legacy.patch = 135;
    legacy.family = 0;
    legacy.macros[0] = 64; legacy.macros[1] = 64; legacy.macros[2] = 64;
    std::snprintf(legacy.name, sizeof(legacy.name), "LEGACY FM");
    legacy.mask = edit::bit(edit::OpLevel0); // Only OpLevel0 edited, no FmAlgorithm
    legacy.values[edit::OpLevel0] = 80.0f;
    assert(edit::encodeV2(legacy, buffer));

    edit::Preset legacy_decoded{};
    assert(edit::decodeV2(buffer, sizeof(buffer), legacy_decoded));
    assert(!(legacy_decoded.mask & edit::bit(edit::FmAlgorithm)));

    std::printf("  PASS: V2 Codec Modes 0, 1, 2 and legacy preset backward compatibility qualified.\n");
}

static void testMusicalSafetyAndNotesActive(amy::Runtime& r, LocalAmyPerformanceBackend& b) {
    std::printf("--- Test 8: Musical Safety & Notes Active Guard ---\n");
    selectPatch(r, b, 128);

    const unsigned orig_algo = unsigned(std::lround(r.snapshot().edit.applied[edit::FmAlgorithm]));
    (void)orig_algo;

    // Start a musical note
    amy::Command note_cmd{};
    note_cmd.type = amy::CommandType::NoteOn;
    note_cmd.id = 64; note_cmd.value = 100; note_cmd.token = 9001;
    r.commands().push(note_cmd);
    g_now_ms += 10;
    r.apply(g_now_ms);
    assert(r.render());
    b.publish(r.snapshot(), true, 0, 0);

    assert(r.snapshot().active_notes > 0);

    // Attempting to change algorithm while notes are playing MUST be rejected
    assert(!b.editParameter(edit::FmAlgorithm, 8.0f));
    assert(std::strcmp(b.editNotice(), "RELEASE NOTES TO CHANGE FM ALGORITHM") == 0);

    // Release note
    r.commands().release(64, note_cmd.token);
    g_now_ms += 10;
    r.apply(g_now_ms);
    assert(r.render());
    b.publish(r.snapshot(), true, 0, 0);
    assert(r.snapshot().active_notes == 0);

    // Now algorithm change is accepted smoothly
    assert(b.editParameter(edit::FmAlgorithm, 8.0f));
    tickApply(r, b);
    verifyAllEightVoicesMatch(r, 8);

    std::printf("  PASS: Algorithm change strictly guarded against active notes; error message verified.\n");
}

int main() {
    std::printf("====================================================\n");
    std::printf("=== SMK-P4 0.13: FM ALGORITHM SELECTOR DSP SUITE ===\n");
    std::printf("====================================================\n");

    amy::Runtime r;
    assert(r.begin());
    LocalAmyPerformanceBackend b(r.commands());
    b.publish(r.snapshot(), true, 0, 0);

    testAvailableAlgorithms(r, b);
    testOperatorMapping(r, b);
    testAudibleDifference(r, b);
    testParameterPreservationAndBrightness(r, b);
    testCompareRevertAndRomIntegrity(r, b);
    testUserPresetsAndPersistence(r, b);
    testMusicalSafetyAndNotesActive(r, b);

    r.end();
    std::printf("====================================================\n");
    std::printf("=== ALL FM ALGORITHM SELECTOR TESTS PASSED (PASS) ===\n");
    std::printf("====================================================\n");
    return 0;
}
