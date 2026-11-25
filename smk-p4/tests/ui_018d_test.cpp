#include "fake_nvs.h"
#include "pattern_nvs.h"
#include "performance_scene_nvs.h"
#include "ui_blueprint_performance_adapter.h"
#include "local_performance_backend.h"
#include "../ui-preview/preview_harness.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace smk::p4;
using namespace smk::p4::ui;

namespace {

lv_obj_t* findLabel(lv_obj_t* root, const char* text, bool prefix = false) {
    if (!root) return nullptr;
    if (lv_obj_check_type(root, &lv_label_class)) {
        const char* actual = lv_label_get_text(root);
        if (actual && (prefix ? std::strncmp(actual, text, std::strlen(text)) == 0
                              : std::strcmp(actual, text) == 0)) {
            return root;
        }
    }
    const uint32_t cnt = lv_obj_get_child_count(root);
    for (uint32_t i = 0; i < cnt; ++i) {
        if (auto* l = findLabel(lv_obj_get_child(root, int32_t(i)), text, prefix)) return l;
    }
    return nullptr;
}

lv_obj_t* findButton(lv_obj_t* root, const char* text, bool prefix = false) {
    if (lv_obj_check_type(root, &lv_button_class)) {
        for (unsigned i = 0; i < lv_obj_get_child_count(root); ++i) {
            auto* child = lv_obj_get_child(root, int(i));
            if (lv_obj_check_type(child, &lv_label_class)) {
                const auto* label = lv_label_get_text(child);
                if (label && (prefix ? std::strncmp(label, text, std::strlen(text)) == 0
                                     : std::strcmp(label, text) == 0)) {
                    return root;
                }
            }
        }
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(root); ++i) {
        if (auto* button = findButton(lv_obj_get_child(root, int(i)), text, prefix)) return button;
    }
    return nullptr;
}

void clickObj(lv_obj_t* obj, UiBlueprint& app) {
    assert(obj);
    lv_obj_send_event(obj, LV_EVENT_CLICKED, nullptr);
    preview::pumpFrame(app, 25);
}

void clickBtn(lv_obj_t* root, const char* text, UiBlueprint& app, bool prefix = false) {
    lv_obj_t* btn = findButton(root, text, prefix);
    if (!btn) {
        std::printf("MISSING BUTTON %s\n", text);
        std::fflush(stdout);
    }
    assert(btn);
    clickObj(btn, app);
}

uint32_t countObjects(lv_obj_t* obj) {
    if (!obj) return 0;
    uint32_t count = 1;
    const uint32_t children = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < children; ++i) {
        count += countObjects(lv_obj_get_child(obj, int32_t(i)));
    }
    return count;
}

struct LatencyStats {
    double median_us = 0.0;
    double p95_us    = 0.0;
    double max_us    = 0.0;
};

LatencyStats computeLatency(std::vector<double>& samples) {
    LatencyStats s{};
    if (samples.empty()) return s;
    std::sort(samples.begin(), samples.end());
    s.median_us = samples[samples.size() / 2];
    const size_t p95_idx = std::min(samples.size() - 1, (samples.size() * 95) / 100);
    s.p95_us = samples[p95_idx];
    s.max_us = samples.back();
    return s;
}

} // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const char* out_dir = (argc >= 2) ? argv[1] : "smk-p4/docs/ui_018d_shots";

    amy::Runtime runtime;
    assert(runtime.begin());
    LocalAmyPerformanceBackend perf(runtime.commands());
    UiBlueprintPerformanceAdapter adapter(perf);

    NvsPatternStore patterns;
    assert(patterns.begin());
    NvsSceneStore scenes;
    assert(scenes.begin());
    adapter.setPatternStore(patterns);
    adapter.setSceneStore(scenes);

    assert(preview::initHostLvgl() != nullptr);
    UiBlueprint app(adapter);
    assert(app.begin());

    auto sync_runtime = [&](uint32_t dt_ms = 25) {
        preview::g_sim_tick_ms += dt_ms;
        runtime.apply(preview::g_sim_tick_ms);
        runtime.loadPendingPatch();
        assert(runtime.render());
        perf.publish(runtime.snapshot(), runtime.snapshot().active_notes == 0, 120, 0);
        preview::pumpFrame(app, dt_ms);
    };

    auto capture = [&](const char* name) {
        char path[256]{};
        std::snprintf(path, sizeof(path), "%s/%s.ppm", out_dir, name);
        preview::pumpFrame(app, 25);
        auto* badge = findLabel(app.rootObject(), "P4 LIVE");
        if (badge) { lv_label_set_text(badge, "HOST SIM"); lv_refr_now(nullptr); }
        assert(preview::saveFramebufferPpm(path));
        if (badge) lv_label_set_text(badge, "P4 LIVE");
        std::printf("CAPTURED: %s\n", path);
    };

    auto& nav = app.navigation();
    auto* root = app.contentObject();
    auto click = [&](const char* label, bool prefix = false) {
        clickBtn(root, label, app, prefix);
        sync_runtime();
    };
    auto clickRoot = [&](const char* label, bool prefix = false) {
        clickBtn(app.rootObject(), label, app, prefix);
        sync_runtime();
    };
    auto request = [&](seq::Action action, int value = 0) {
        seq::Request r{};
        r.action = action;
        r.value = int16_t(value);
        assert(adapter.requestSequencer(r).accepted());
        sync_runtime();
    };

    sync_runtime();

    // Seed Patterns 01 & 02 and Scenes 01..03 for integrated workflow scenarios.
    seq::Slot pat1{};
    pat1.state = seq::SlotState::Valid;
    pat1.settings.pattern.length = seq::kSteps;
    pat1.settings.pattern.steps[0] = {60, 100, 60, true};
    pat1.settings.pattern.steps[4] = {64, 100, 60, true};
    pat1.settings.pattern.steps[8] = {67, 100, 60, true};
    pat1.settings.pattern.steps[12] = {72, 100, 60, true};
    std::snprintf(pat1.name, sizeof(pat1.name), "STAGE GROOVE");
    assert(patterns.commit(0, pat1));

    seq::Slot pat2 = pat1;
    pat2.settings.pattern.steps[0].note = 62;
    std::snprintf(pat2.name, sizeof(pat2.name), "NEXT VERSE");
    assert(patterns.commit(1, pat2));
    adapter.setPatternStore(patterns);

    scene::Slot sc1{};
    sc1.state = seq::SlotState::Valid;
    std::snprintf(sc1.config.name, sizeof(sc1.config.name), "INTRO PAD");
    sc1.config.domains = scene::Patch | scene::Fx | scene::Harmony | scene::Pattern | scene::Tempo;
    sc1.config.patch_id = 128;
    sc1.config.pattern_slot = 0;
    sc1.config.pattern_sequence = patterns.load(0).sequence;
    sc1.config.bpm = 120;
    assert(scenes.commit(0, sc1));

    scene::Slot sc2 = sc1;
    std::snprintf(sc2.config.name, sizeof(sc2.config.name), "CHORUS LEAD");
    sc2.config.patch_id = 129;
    sc2.config.pattern_slot = 1;
    sc2.config.pattern_sequence = patterns.load(1).sequence;
    sc2.config.bpm = 128;
    assert(scenes.commit(1, sc2));

    scene::Slot sc_bad = sc1;
    std::snprintf(sc_bad.config.name, sizeof(sc_bad.config.name), "BAD PATCH");
    sc_bad.config.domains = scene::Patch | scene::Tempo;
    sc_bad.config.patch_id = UINT16_MAX;
    assert(scenes.commit(2, sc_bad));
    adapter.setSceneStore(scenes);
    nav.refreshActive();
    sync_runtime();

    // Capture 01: Performance Stage refined
    capture("01_perf_stage_refined");

    // ── Scenario 1: Performance with Sequencer -> Quick FX -> Adjust -> Return ─
    assert(nav.navigateTo(ScreenId::Patterns));
    sync_runtime();
    click("01 STAGE", true);
    click("LOAD");
    for (unsigned i = 0; i < 4; ++i) sync_runtime();
    assert(adapter.sequencerStatus()->active_slot == 0);
    clickRoot("TOCAR");
    assert(nav.currentScreen() == ScreenId::Performance);
    capture("02_perf_pattern_active");

    request(seq::Action::Play);
    assert(adapter.sequencerStatus()->playing);
    for (unsigned i = 0; i < 8; ++i) sync_runtime();

    // Quick Action from Performance FX pill -> Effects (module preserved) -> adjust -> return
    auto* fx_pill_lbl = findLabel(root, "FX ON");
    assert(fx_pill_lbl);
    clickObj(lv_obj_get_parent(fx_pill_lbl), app);
    sync_runtime();
    assert(nav.currentScreen() == ScreenId::Effects);
    capture("06_fx_quick_action");
    assert(adapter.requestParameterInt("fx.delay.feedback", 42).accepted());
    sync_runtime();
    click("< TOCAR");
    assert(nav.currentScreen() == ScreenId::Performance);
    assert(adapter.sequencerStatus()->playing);
    capture("07_return_fx_to_perf");

    // ── Scenario 2: Arp with sustained MIDI notes -> Timbres -> Return ───────
    assert(adapter.requestParameterBool("arp.enable", true).accepted());
    sync_runtime();
    assert(adapter.requestArpTransport(true).accepted());
    sync_runtime();
    const uint8_t arp_on[]{0x09, 0x90, 60, 100};
    assert(runtime.usbInput().transfer(arp_on, sizeof(arp_on), preview::g_sim_tick_ms));
    for (unsigned i = 0; i < 16; ++i) sync_runtime();
    assert(runtime.snapshot().arp.length > 0 && adapter.sequencerStatus()->playing);
    capture("04_perf_arp_seq_active");

    // Open PatchBrowser via Quick Action (StatusStrip info box / Patch name)
    auto* patch_lbl = findLabel(root, "PATCH #", true);
    assert(patch_lbl);
    clickObj(patch_lbl, app);
    sync_runtime();
    assert(nav.currentScreen() == ScreenId::PatchBrowser);
    setSessionLockMask(perf_lock::Fx | perf_lock::Harmony);
    nav.refreshActive();
    sync_runtime();
    assert(findLabel(root, "LOCKS: FX HARM"));
    capture("05_patch_browser_quick_action");
    // Return to Performance via TOCAR footer without dropping Arp/Seq notes
    clickRoot("TOCAR");
    assert(nav.currentScreen() == ScreenId::Performance);
    assert(runtime.snapshot().arp.length > 0 && adapter.sequencerStatus()->playing);

    // ── Scenario 3: Pattern Switch NEXT BAR -> FX -> Return -> Confirmed ─────
    assert(nav.navigateTo(ScreenId::Patterns));
    sync_runtime();
    click("SWITCH MODE");
    assert(nav.patternSwitchMode() == seq::SwitchMode::NextBar);
    click("02 NEXT", true);
    click("LOAD");
    sync_runtime();
    assert(adapter.sequencerStatus()->pending_slot == 1);
    clickRoot("TOCAR");
    assert(findLabel(root, "SEQ P>2"));
    capture("03_perf_pattern_pending");

    clickRoot("FX");
    assert(nav.currentScreen() == ScreenId::Effects);
    clickRoot("TOCAR");
    assert(nav.currentScreen() == ScreenId::Performance);
    // Advance until 4/4 boundary applies Pattern 02 (~375 blocks at 120 BPM)
    for (unsigned i = 0; i < 500 && adapter.sequencerStatus()->pending_slot != seq::kNoSlot; ++i) {
        sync_runtime(25);
    }
    assert(adapter.sequencerStatus()->pending_slot == seq::kNoSlot);
    assert(adapter.sequencerStatus()->active_slot == 1);
    capture("15_pattern_scheduled_confirmed");

    // ── Scenario 4: Scene Recall with Performance Lock & Domain Summary ──────
    click("CENAS 1-8");
    assert(findLabel(root, "APLICAR: PATCH* | PATTERN | FX(LK) | HARMONY(LK) | TEMPO"));
    capture("08_scene_recall_domains");

    // Queue Scene 02 in NextBar via adapter (or Scenes screen) to capture Pending
    SceneOperation sc_pend{};
    sc_pend.slot = 1;
    sc_pend.mode = scene::RecallMode::NextBar;
    sc_pend.locks = sessionLockMask();
    adapter.requestScene(sc_pend);
    sync_runtime();
    assert(adapter.sceneView().recall.phase == scene::RecallPhase::WaitingBoundary);
    capture("09_scene_recall_pending");
    // Wait for boundary (~375 blocks at 120 BPM)
    for (unsigned i = 0; i < 500 && adapter.sceneView().recall.phase == scene::RecallPhase::WaitingBoundary; ++i) {
        sync_runtime(25);
    }
    // Capture simultaneous states (Arp + Seq + Pattern + Scene)
    click("MACROS 1-4");
    capture("18_simultaneous_states");

    // Trigger failed Scene Recall (slot 2 has invalid patch_id)
    SceneOperation sc_fail{};
    sc_fail.slot = 2;
    sc_fail.mode = scene::RecallMode::Immediate;
    adapter.requestScene(sc_fail);
    sync_runtime();
    sync_runtime();
    assert(adapter.sceneView().recall.phase == scene::RecallPhase::Failed);
    capture("10_scene_recall_failed");

    // Capture long patch name display (e.g. patch 130+ or longer name)
    assert(adapter.confirmPatchDiscard(130, 0).accepted());
    for (unsigned i = 0; i < 8; ++i) sync_runtime();
    capture("17_long_patch_name");

    // ── Scenario 5: Harmony Overlay & Independent Scopes ─────────────────────
    clickRoot("C MAJ", true);
    auto* harm_lbl = findLabel(app.rootObject(), "Estado:", true);
    assert(harm_lbl && std::strstr(lv_label_get_text(harm_lbl), "SEQ Q: OFF") != nullptr);
    capture("11_harmony_overlay");
    clickRoot("Q QUANT:", true);
    for (unsigned i = 0; i < 4; ++i) sync_runtime();
    assert(adapter.model().getBool("harmony.scale_enable", false));
    assert(!adapter.model().getBool("harmony.chord_enable", false));
    assert(!adapter.sequencerStatus()->quantize);
    capture("12_quantize_chord_independent");
    clickRoot("FECHAR");

    // ── Scenario 6: Sequencer PLAY & Step Record preserved across navigation ─
    assert(nav.navigateTo(ScreenId::Sequencer));
    sync_runtime();
    if (nav.sequencerEdit()) click("PLAY MODE");
    capture("13_seq_play_during_nav");

    click("EDIT MODE");
    click("MIDI / REC");
    click("STEP RECORD");
    assert(adapter.sequencerStatus()->midi.mode == seq::InputMode::StepRecord);
    click("CLOSE");
    click("03", true);
    const auto rec_before = adapter.sequencerStatus()->midi.selected;
    clickRoot("FX");
    clickRoot("RITMO");
    assert(nav.currentScreen() == ScreenId::Sequencer);
    assert(adapter.sequencerStatus()->midi.mode == seq::InputMode::StepRecord);
    assert(adapter.sequencerStatus()->midi.selected == rec_before);
    capture("14_step_record_after_return");

    // ── Scenario 7: Storage Operation with Confirmation & Cancel ─────────────
    request(seq::Action::Stop);
    assert(adapter.requestArpTransport(false).accepted());
    for (unsigned i = 0; i < 10; ++i) sync_runtime();
    assert(nav.navigateTo(ScreenId::Patterns));
    sync_runtime();
    click("03 EMPTY", true);
    click("SAVE");
    assert(findLabel(app.rootObject(), "SAVE NEW PATTERN - SLOT 03", true));
    capture("16_storage_op_confirm");
    clickRoot("CANCEL");
    click("SAVE");
    clickRoot("CONFIRM");
    for (unsigned i = 0; i < 12; ++i) sync_runtime();
    assert(std::strstr(adapter.patternNotice(), "CONFIRMED") != nullptr);

    // ── Stress Test (Section 15 minimums) ────────────────────────────────────
    // 1000 area navigations, 500 overlay open/closes, 500 returns to Performance,
    // 200 FX param changes, 200 PLAY/EDIT toggles, 200 Patterns navs,
    // 100 Scene recalls, 100 Harmony changes.
    assert(nav.navigateTo(ScreenId::Performance));
    sync_runtime();
    const auto objs_before = countObjects(lv_screen_active());
    lv_mem_monitor_t mem_before{};
    lv_mem_monitor(&mem_before);

    lv_obj_t* harm_btn = findButton(app.rootObject(), "C MAJ", true);
    assert(harm_btn);
    std::vector<double> times;
    times.reserve(500);
    for (unsigned i = 0; i < 500; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        // 2 area navigations per iteration = 1000 total
        assert(nav.navigateToArea(UiArea::Fx));
        if (i < 200) {
            assert(adapter.requestParameterInt("fx.delay.mix", int(20 + (i % 50))).accepted());
            sync_runtime(1);
        }
        assert(nav.navigateToArea(UiArea::Ritmo));
        if (i < 200) {
            nav.setSequencerEdit((i % 2) != 0);
            assert(nav.navigateTo(ScreenId::Sequencer));
            assert(nav.navigateTo(ScreenId::Patterns));
        }
        // Overlay open/close
        clickObj(harm_btn, app);
        if (i < 100) {
            assert(adapter.requestParameterInt("harmony.root_note", int(i % 12)).accepted());
            sync_runtime(1);
        }
        // Return to Performance closes overlay automatically in 1 tap
        clickRoot("TOCAR");
        assert(nav.currentScreen() == ScreenId::Performance);
        if (i < 100) {
            SceneOperation s_op{};
            s_op.slot = uint8_t(i % 2);
            s_op.locks = sessionLockMask();
            adapter.requestScene(s_op);
        }
        sync_runtime(1);
        times.push_back(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count());
    }

    lv_mem_monitor_t mem_after{};
    lv_mem_monitor(&mem_after);
    const auto objs_after = countObjects(lv_screen_active());
    const auto stats = computeLatency(times);
    std::printf("018D STRESS objs=%u->%u free=%zu->%zu largest=%zu->%zu peak=%zu frag=%u->%u median=%.1f p95=%.1f max=%.1f us\n",
                objs_before, objs_after, mem_before.free_size, mem_after.free_size,
                mem_before.free_biggest_size, mem_after.free_biggest_size, mem_after.max_used,
                mem_before.frag_pct, mem_after.frag_pct, stats.median_us, stats.p95_us, stats.max_us);
    assert(objs_before == objs_after);
    assert(mem_after.free_size >= mem_before.free_size);
    assert(mem_after.max_used <= 128u * 1024u);

    nav.shutdown();
    std::puts("PASS 0.18-D integrated musical workflow / host simulation");
    return 0;
}
