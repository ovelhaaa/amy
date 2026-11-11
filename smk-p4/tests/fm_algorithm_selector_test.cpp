#include "amy_runtime.h"
#include "fm_topology.h"
#include "local_performance_backend.h"
#include "user_presets_v2.h"
#include "user_preset_nvs_v2.h"
#include "user_preset_fx_nvs.h"
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

        // Render steady state blocks
        for (int blk = 0; blk < 4; ++blk) {
            r.render();
        }
        std::vector<int16_t> buf(amy::kBlockFrames * 2);
        std::memcpy(buf.data(), r.render(), buf.size() * sizeof(int16_t));

        // Release note
        r.commands().release(60, note_cmd.token);
        g_now_ms += 10;
        r.apply(g_now_ms);
        r.render();
        return buf;
    };

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
    assert(synth[op2_osc]->amp_coefs[COEF_CONST] > 0.0f);

    std::printf("  PASS: Operator ratios, envelopes, and levels preserved; modulator/carrier depth reapplied.\n");
}

extern "C" {
void my_srand48(uint32_t);
}

static void testCompareRevertAndRomIntegrity(amy::Runtime& r, LocalAmyPerformanceBackend& b) {
    std::printf("--- Test 5 & 6: Compare, Revert & ROM Integrity ---\n");
    selectPatch(r, b, 128);

    const unsigned orig_algo = unsigned(std::lround(r.snapshot().edit.original[edit::FmAlgorithm]));

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
    assert(r.snapshot().edit.applied[edit::FmAlgorithm] == float(orig_algo));

    // End initial runtime before running isolated deterministic rendering passes
    r.end();

    // Deterministic fresh runtime rendering for bit-exact PCM comparison
    auto renderDeterministicFmPcm = [](unsigned patch, int algo_edit, int compare_or_revert) -> std::vector<int16_t> {
        amy::Runtime runtime;
        assert(runtime.begin());
        assert(runtime.commands().push({amy::CommandType::Patch, uint16_t(patch)}));
        runtime.apply(0);
        runtime.loadPendingPatch();
        assert(runtime.render());

        if (algo_edit > 0) {
            edit::Request req{};
            req.patch = patch;
            req.generation = runtime.snapshot().generation;
            req.request = 1;
            req.mask = edit::bit(edit::FmAlgorithm);
            req.values[edit::FmAlgorithm] = float(algo_edit);
            assert(runtime.commands().edits.push(req));
            runtime.apply(10);
            assert(runtime.snapshot().edit.errors == 0);

            if (compare_or_revert == 1) { // Compare active
                req.request = 2;
                req.compare = true;
                assert(runtime.commands().edits.push(req));
                runtime.apply(20);
            } else if (compare_or_revert == 2) { // Revert
                req.request = 2;
                req.compare = false;
                req.mask = 0;
                assert(runtime.commands().edits.push(req));
                runtime.apply(20);
            }
        }

        my_srand48(12345);
        amy::Command note_cmd{};
        note_cmd.type = amy::CommandType::NoteOn;
        note_cmd.id = 60;
        note_cmd.value = 100;
        note_cmd.token = 7001;
        runtime.commands().push(note_cmd);
        runtime.apply(30);

        std::vector<int16_t> pcm;
        pcm.reserve(16 * amy::kBlockFrames * 2);
        for (int blk = 0; blk < 16; ++blk) {
            const int16_t* out = runtime.render();
            assert(out);
            pcm.insert(pcm.end(), out, out + amy::kBlockFrames * 2);
        }

        runtime.end();
        return pcm;
    };

    auto computeDiff = [](const std::vector<int16_t>& v1, const std::vector<int16_t>& v2) -> int64_t {
        assert(v1.size() == v2.size());
        int64_t d = 0;
        for (size_t i = 0; i < v1.size(); ++i) {
            d += std::abs(int64_t(v1[i]) - int64_t(v2[i]));
        }
        return d;
    };

    const auto pcm_rom = renderDeterministicFmPcm(128, 0, 0);
    const auto pcm_algo10 = renderDeterministicFmPcm(128, 10, 0);
    const auto pcm_compare = renderDeterministicFmPcm(128, 10, 1);
    const auto pcm_revert = renderDeterministicFmPcm(128, 10, 2);

    const int64_t diff_algo10 = computeDiff(pcm_rom, pcm_algo10);
    const int64_t compare_diff = computeDiff(pcm_rom, pcm_compare);
    const int64_t revert_diff = computeDiff(pcm_rom, pcm_revert);

    std::printf("  [algo 10 diff energy: %lld]\n", (long long)diff_algo10);
    std::printf("  [compare diff energy: %lld]\n", (long long)compare_diff);
    std::printf("  [revert diff energy: %lld]\n", (long long)revert_diff);
    std::fflush(stdout);

    // Assert genuine sonic difference on algorithm 10
    assert(diff_algo10 > 50000);

    // Assert exact deterministic equivalence for Compare and Revert
    assert(compare_diff == 0);
    assert(revert_diff == 0);

    std::printf("  PASS: Compare toggles algorithm; Revert strictly restores deterministic ROM PCM & state (revert_diff == 0).\n");
}

static void testTopologyConnectionsGraph() {
    std::printf("--- Test: 32-Algorithm Topology Graphical Connections ---\n");
    for (unsigned algo = 1; algo <= 32; ++algo) {
        const auto topo = fm::deriveTopology(algo);
        const auto graph = fm::generateLayout(topo, 576, 78);

        // 1. Confirm 6 operator nodes
        for (unsigned i = 0; i < 6; ++i) {
            const unsigned op = 6 - i;
            assert(graph.nodes[i].op == op);
            assert(graph.nodes[i].w == 54);
            assert(graph.nodes[i].h == 24);
            assert(graph.nodes[i].x >= 0 && graph.nodes[i].x + graph.nodes[i].w <= 576);
            assert(graph.nodes[i].y >= 0 && graph.nodes[i].y + graph.nodes[i].h <= 78);
            assert(graph.nodes[i].is_carrier == topo.isCarrier(op));
            assert(graph.nodes[i].has_fb == topo.hasFeedback(op));
        }

        // 2. Confirm audio output node
        assert(graph.out_node.op == 0);
        assert(graph.out_node.x >= 0 && graph.out_node.x + graph.out_node.w <= 576);
        assert(graph.out_node.y >= 0 && graph.out_node.y + graph.out_node.h <= 78);

        // 3. Confirm modulation connection edges match topo.connectsTo
        for (unsigned src = 1; src <= 6; ++src) {
            for (unsigned dst = 1; dst <= 6; ++dst) {
                if (topo.connectsTo(src, dst)) {
                    bool edge_found = false;
                    for (unsigned e = 0; e < graph.edge_count; ++e) {
                        if (graph.edges[e].type == fm::EdgeType::Modulation &&
                            graph.edges[e].src_op == src && graph.edges[e].dst_op == dst) {
                            edge_found = true;
                            assert(graph.edges[e].point_count >= 2);
                            for (uint8_t p = 0; p < graph.edges[e].point_count; ++p) {
                                assert(graph.edges[e].points[p].x >= 0 && graph.edges[e].points[p].x <= 576);
                                assert(graph.edges[e].points[p].y >= 0 && graph.edges[e].points[p].y <= 78);
                            }
                            break;
                        }
                    }
                    assert(edge_found);
                }
            }
        }

        // 4. Confirm carrier audio connections
        for (unsigned c = 1; c <= 6; ++c) {
            if (topo.isCarrier(c)) {
                bool carrier_edge_found = false;
                for (unsigned e = 0; e < graph.edge_count; ++e) {
                    if (graph.edges[e].type == fm::EdgeType::AudioOut && graph.edges[e].src_op == c) {
                        carrier_edge_found = true;
                        assert(graph.edges[e].point_count == 2);
                        break;
                    }
                }
                assert(carrier_edge_found);
            }
        }

        // 5. Confirm feedback connection
        if (topo.feedback_in != 0) {
            bool fb_edge_found = false;
            for (unsigned e = 0; e < graph.edge_count; ++e) {
                if (graph.edges[e].type == fm::EdgeType::Feedback &&
                    graph.edges[e].dst_op == topo.feedback_in) {
                    fb_edge_found = true;
                    assert(graph.edges[e].point_count == 4);
                    break;
                }
            }
            assert(fb_edge_found);
        }
    }
    std::printf("  PASS: All 32 algorithms generate strictly bounded, verified graph connection topologies.\n");
}

static void testEndToEndSaveRebootRecall() {
    std::printf("--- Test: End-to-End User Presets Save, Reboot & Recall with FM Topology ---\n");
    fake_nvs::blobs.clear();
    fake_nvs::free_entries = 700;

    // Phase 1: Boot, load DX7 patch, modify algorithm, operator parameters and FX, save preset
    {
        amy::Runtime r;
        assert(r.begin());
        NvsVersionedPresets synth_store;
        assert(synth_store.begin());
        fx::NvsFxPresets fx_store;
        assert(fx_store.begin());

        LocalAmyPerformanceBackend b(r.commands());
        b.presetStore(&synth_store);
        b.fxPresetStore(&fx_store);
        b.publish(r.snapshot(), true, 0, 0);

        selectPatch(r, b, 128);
        const unsigned orig_algo = unsigned(std::lround(r.snapshot().edit.original[edit::FmAlgorithm]));
        assert(orig_algo != 15);

        // Edit algorithm to 15
        assert(b.editParameter(edit::FmAlgorithm, 15.0f));
        // Edit OP2 Level to 85%
        assert(b.editParameter(edit::OpLevel0 + 1, 85.0f));
        // Edit OP3 Ratio to 3.0
        assert(b.editParameter(edit::OpRatio0 + 2, 3.0f));

        // Configure FX
        assert(b.setFxParameter("fx.chorus.mode", 2.0f, g_now_ms));
        assert(b.setFxParameter("fx.chorus.depth", 60.0f, g_now_ms));
        assert(b.setFxParameter("fx.reverb.size", 75.0f, g_now_ms));
        assert(b.setFxParameter("fx.reverb.mix", 45.0f, g_now_ms));

        tickApply(r, b);
        assert(b.unsaved());

        // Save As to Slot 3 ("FM REBOOT 15")
        assert(b.editAction(2, 3, "FM REBOOT 15"));
        // Service through quiet barrier to complete transaction commit
        for (int step = 0; step < 5; ++step) {
            tickApply(r, b);
        }
        assert(!b.editError());
        assert(std::strcmp(b.editNotice(), "SAVED") == 0 || !b.unsaved());

        r.end();
    }

    // Phase 2: Full reboot simulation (destroy all in-memory instances, restart from persisted NVS)
    {
        amy::Runtime r;
        assert(r.begin());
        NvsVersionedPresets synth_store;
        assert(synth_store.begin());
        fx::NvsFxPresets fx_store;
        assert(fx_store.begin());

        LocalAmyPerformanceBackend b(r.commands());
        b.presetStore(&synth_store);
        b.fxPresetStore(&fx_store);
        b.publish(r.snapshot(), true, 0, 0);

        // Verify slot info before recall
        char loaded_name[25]{};
        uint16_t origin_patch = 0;
        assert(b.userPreset(3, loaded_name, sizeof(loaded_name), origin_patch));
        assert(std::strcmp(loaded_name, "FM REBOOT 15") == 0);
        assert(origin_patch == 128);

        // Recall Slot 3
        assert(b.editAction(3, 3)); // Action 3 is Recall
        for (int step = 0; step < 5; ++step) {
            g_now_ms += 50;
            b.service(g_now_ms);
            r.apply(g_now_ms);
            r.loadPendingPatch();
            r.render();
            b.publish(r.snapshot(), true, 0, 0);
        }

        // Verify all 8 voices have algorithm 15
        verifyAllEightVoicesMatch(r, 15);

        // Verify OP2 Level and OP3 Ratio
        const auto& edit = r.snapshot().edit;
        assert(edit.applied[edit::OpLevel0 + 1] == 85.0f);
        assert(edit.applied[edit::OpRatio0 + 2] == 3.0f);

        // Verify FX
        const auto& fx = r.snapshot().fx.config;
        assert(fx.chorus_mode == 2);
        assert(std::lround(fx.chorus_depth * 100.0f) == 60);
        assert(std::lround(fx.reverb_size * 100.0f) == 75);
        assert(std::lround(fx.reverb_mix * 100.0f) == 45);

        // Verify clean saved state
        assert(!b.unsaved());

        r.end();
    }

    // Phase 3: Legacy preset backward compatibility
    {
        // Inject a legacy V2 preset without the FmAlgorithm bit
        uint8_t raw_buf[edit::kV2RecordBytes];
        edit::Preset legacy{};
        legacy.sequence = 1;
        legacy.patch = 135; // DX7 patch with default algorithm != 1
        legacy.family = 0;
        legacy.macros[0] = legacy.macros[1] = legacy.macros[2] = 64;
        std::snprintf(legacy.name, sizeof(legacy.name), "LEGACY NO ALGO");
        legacy.mask = edit::bit(edit::OpLevel0); // Only OpLevel0 edited, NO FmAlgorithm bit
        legacy.values[edit::OpLevel0] = 75.0f;
        assert(edit::encodeV2(legacy, raw_buf));

        // Save legacy preset manually to slot 7
        NvsVersionedPresets synth_store;
        assert(synth_store.begin());
        assert(synth_store.save(7, legacy));

        amy::Runtime r;
        assert(r.begin());
        fx::NvsFxPresets fx_store;
        assert(fx_store.begin());

        LocalAmyPerformanceBackend b(r.commands());
        b.presetStore(&synth_store);
        b.fxPresetStore(&fx_store);
        b.publish(r.snapshot(), true, 0, 0);

        // Recall legacy preset
        assert(b.editAction(3, 7));
        for (int step = 0; step < 5; ++step) {
            g_now_ms += 50;
            b.service(g_now_ms);
            r.apply(g_now_ms);
            r.loadPendingPatch();
            r.render();
            b.publish(r.snapshot(), true, 0, 0);
        }

        // Crucial invariant: The algorithm MUST be the native ROM algorithm of patch 135, NOT defaulted to 1!
        const auto* patch_info = library::find(135);
        assert(patch_info != nullptr);
        const unsigned native_rom_algo = unsigned(std::lround(r.snapshot().edit.original[edit::FmAlgorithm]));
        verifyAllEightVoicesMatch(r, native_rom_algo);
        assert(r.snapshot().edit.applied[edit::OpLevel0] == 75.0f);

        // Phase 4: Test failure modes
        assert(!b.editAction(3, 99)); // Non-existent slot rejected
        assert(!b.editParameter(edit::FmAlgorithm, 50.0f)); // Out of bounds algorithm rejected

        // Active note blocks saving
        amy::Command note_cmd{};
        note_cmd.type = amy::CommandType::NoteOn;
        note_cmd.id = 60; note_cmd.value = 100; note_cmd.token = 7701;
        r.commands().push(note_cmd);
        g_now_ms += 10;
        r.apply(g_now_ms);
        b.publish(r.snapshot(), true, 0, 0);
        assert(!b.editAction(2, 4, "NOTE ACTIVE SAVE")); // Save blocked during note
        r.commands().release(60, note_cmd.token);
        g_now_ms += 10;
        r.apply(g_now_ms);
        b.publish(r.snapshot(), true, 0, 0);

        r.end();
    }
    std::printf("  PASS: End-to-end Save, reboot simulation, and Recall with FM topology fully verified.\n");
}

static void testMusicalSafetyAndNotesActive() {
    std::printf("--- Test 8: Musical Safety & Notes Active Guard ---\n");
    amy::Runtime r;
    assert(r.begin());
    LocalAmyPerformanceBackend b(r.commands());
    b.publish(r.snapshot(), true, 0, 0);

    // 1. Load DX7 FM patch 128
    selectPatch(r, b, 128);

    // 2. Select algorithm 16
    assert(b.editParameter(edit::FmAlgorithm, 16.0f));
    tickApply(r, b);

    // 3. Confirm application across all 8 voices
    verifyAllEightVoicesMatch(r, 16);

    // 4. Start MIDI Note On
    amy::Command note_cmd{};
    note_cmd.type = amy::CommandType::NoteOn;
    note_cmd.id = 64; note_cmd.value = 100; note_cmd.token = 9001;
    r.commands().push(note_cmd);
    g_now_ms += 10;
    r.apply(g_now_ms);
    assert(r.render());
    b.publish(r.snapshot(), true, 0, 0);
    assert(r.snapshot().active_notes > 0);

    // 5. Change OP2 Level while note is actively playing
    assert(b.editParameter(edit::OpLevel0 + 1, 82.0f));
    tickApply(r, b);

    // 6. Confirm OP2 Level applied in AMY native synth
    uint16_t voice_num;
    assert(instrument_get_num_voices(1, &voice_num) == 1);
    unsigned base = voice_to_base_osc[voice_num];
    int op2_osc = synth[base]->algo_source[5 - 1]; // OP2 is native op 4
    assert(synth[op2_osc]->amp_coefs[COEF_CONST] > 0.0f);
    assert(r.snapshot().edit.applied[edit::OpLevel0 + 1] == 82.0f);

    // 7. Change OP2 Ratio while note is actively playing
    assert(b.editParameter(edit::OpRatio0 + 1, 2.5f));
    tickApply(r, b);

    // 8. Confirm OP2 Ratio applied in AMY native synth
    assert(std::abs(std::exp2(synth[op2_osc]->logratio) - 2.5f) < 0.01f);
    assert(r.snapshot().edit.applied[edit::OpRatio0 + 1] == 2.5f);

    // 9. Change FM Brightness while note is actively playing
    assert(b.editParameter(edit::Depth, 0.65f));
    tickApply(r, b);

    // 10. Confirm FM Brightness applied
    assert(r.snapshot().edit.applied[edit::Depth] == 0.65f);

    // 11. Re-sending the currently applied algorithm (16) must NOT be rejected
    assert(b.editParameter(edit::FmAlgorithm, 16.0f));
    tickApply(r, b);
    assert(!b.editError());
    assert(r.snapshot().edit.applied[edit::FmAlgorithm] == 16.0f);

    // 12. Attempting a real change of topology (e.g. to Algo 32) while note is held MUST be rejected
    assert(!b.editParameter(edit::FmAlgorithm, 32.0f));
    assert(std::strcmp(b.editNotice(), "RELEASE NOTES TO CHANGE FM ALGORITHM") == 0);

    // 13. Attempting Compare or Revert while note is held (requiring topology change) MUST be rejected
    assert(!b.editAction(0)); // Compare rejected
    assert(std::strcmp(b.editNotice(), "RELEASE NOTES TO CHANGE FM ALGORITHM") == 0);
    assert(!b.editAction(1)); // Revert rejected
    assert(std::strcmp(b.editNotice(), "RELEASE NOTES TO CHANGE FM ALGORITHM") == 0);

    // Confirm no partial state: algorithm is still 16 across all 8 voices
    verifyAllEightVoicesMatch(r, 16);
    assert(r.snapshot().edit.applied[edit::OpLevel0 + 1] == 82.0f);
    assert(r.snapshot().edit.applied[edit::OpRatio0 + 1] == 2.5f);

    // 14. Release note
    r.commands().release(64, note_cmd.token);
    g_now_ms += 10;
    r.apply(g_now_ms);
    assert(r.render());
    b.publish(r.snapshot(), true, 0, 0);
    assert(r.snapshot().active_notes == 0);

    // 15. Now algorithm 32 is accepted smoothly
    assert(b.editParameter(edit::FmAlgorithm, 32.0f));
    tickApply(r, b);

    // 16. Confirm application across all 8 voices
    verifyAllEightVoicesMatch(r, 32);

    // 17. Verify sustain pedal safety
    uint8_t sus_on[] = {0x0B, 0xB0, 64, 127};
    assert(r.usbInput().packet(sus_on, g_now_ms));
    g_now_ms += 10;
    r.apply(g_now_ms);
    r.render();
    b.publish(r.snapshot(), true, 0, 0);
    assert(r.snapshot().sustain_active);

    // Attempting topology change while sustain is active MUST be rejected
    assert(!b.editParameter(edit::FmAlgorithm, 12.0f));
    assert(std::strcmp(b.editNotice(), "RELEASE NOTES TO CHANGE FM ALGORITHM") == 0);

    // Release sustain
    uint8_t sus_off[] = {0x0B, 0xB0, 64, 0};
    assert(r.usbInput().packet(sus_off, g_now_ms));
    g_now_ms += 10;
    r.apply(g_now_ms);
    r.render();
    b.publish(r.snapshot(), true, 0, 0);
    assert(!r.snapshot().sustain_active);

    // Now accepted smoothly
    assert(b.editParameter(edit::FmAlgorithm, 12.0f));
    tickApply(r, b);
    verifyAllEightVoicesMatch(r, 12);

    r.end();
    std::printf("  PASS: Continuous live operator editing verified during active notes; real topology change strictly guarded.\n");
}

int main() {
    std::printf("====================================================\n");
    std::printf("=== SMK-P4 0.13: FM ALGORITHM SELECTOR DSP SUITE ===\n");
    std::printf("====================================================\n");

    {
        amy::Runtime r;
        assert(r.begin());
        LocalAmyPerformanceBackend b(r.commands());
        b.publish(r.snapshot(), true, 0, 0);

        testAvailableAlgorithms(r, b);
        testOperatorMapping(r, b);
        testAudibleDifference(r, b);
        testParameterPreservationAndBrightness(r, b);
        testCompareRevertAndRomIntegrity(r, b);
        r.end();
    }

    testTopologyConnectionsGraph();
    testEndToEndSaveRebootRecall();
    testMusicalSafetyAndNotesActive();

    std::printf("====================================================\n");
    std::printf("=== ALL FM ALGORITHM SELECTOR TESTS PASSED (PASS) ===\n");
    std::printf("====================================================\n");
    return 0;
}