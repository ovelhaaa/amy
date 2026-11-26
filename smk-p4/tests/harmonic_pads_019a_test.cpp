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
    if (!root) return nullptr;
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

struct Box {
    int32_t x1, y1, x2, y2;
    int32_t width() const { return x2 - x1; }
    int32_t height() const { return y2 - y1; }
};

Box getCoords(lv_obj_t* obj) {
    assert(obj);
    lv_obj_update_layout(obj);
    lv_area_t a{};
    lv_obj_get_coords(obj, &a);
    return {a.x1, a.y1, a.x2 + 1, a.y2 + 1};
}

bool overlaps(const Box& a, const Box& b) {
    return !(a.x2 <= b.x1 || b.x2 <= a.x1 || a.y2 <= b.y1 || b.y2 <= a.y1);
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

void testHarmonicPadOwner();
int main(int argc, char** argv) {
    testHarmonicPadOwner();
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const char* out_dir = (argc >= 2) ? argv[1] : "smk-p4/docs/harmonic_pads_019a_shots";

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

    // Seed Patterns 01 & 02 and Scenes 01..05
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

    // C1: Patch + Pattern + Fx + Harmony + Arp + Tempo
    scene::Slot sc1{};
    sc1.state = seq::SlotState::Valid;
    std::snprintf(sc1.config.name, sizeof(sc1.config.name), "INTRO PAD");
    sc1.config.domains = scene::Patch | scene::Pattern | scene::Fx | scene::Harmony | scene::Arp | scene::Tempo;
    sc1.config.patch_id = 128;
    sc1.config.pattern_slot = 0;
    sc1.config.pattern_sequence = patterns.load(0).sequence;
    sc1.config.bpm = 120;
    assert(scenes.commit(0, sc1));

    // C2: Chorus Lead
    scene::Slot sc2 = sc1;
    std::snprintf(sc2.config.name, sizeof(sc2.config.name), "CHORUS LEAD");
    sc2.config.patch_id = 129;
    sc2.config.pattern_slot = 1;
    sc2.config.pattern_sequence = patterns.load(1).sequence;
    sc2.config.bpm = 128;
    assert(scenes.commit(1, sc2));

    // C3: Bad Patch (PatchUnavailable)
    scene::Slot sc_bad = sc1;
    std::snprintf(sc_bad.config.name, sizeof(sc_bad.config.name), "BAD PATCH");
    sc_bad.config.domains = scene::Patch | scene::Tempo;
    sc_bad.config.patch_id = UINT16_MAX;
    assert(scenes.commit(2, sc_bad));

    // C4: Pattern Unavailable (stale sequence)
    scene::Slot sc_bad_pat = sc1;
    std::snprintf(sc_bad_pat.config.name, sizeof(sc_bad_pat.config.name), "STALE PAT");
    sc_bad_pat.config.domains = scene::Pattern | scene::Fx;
    sc_bad_pat.config.pattern_slot = 0;
    sc_bad_pat.config.pattern_sequence = 0xDEADBEEFu;
    assert(scenes.commit(3, sc_bad_pat));

    // C5: FX-only (to test empty effective domains when FX is locked)
    scene::Slot sc_fx_only = sc1;
    std::snprintf(sc_fx_only.config.name, sizeof(sc_fx_only.config.name), "FX ONLY");
    sc_fx_only.config.domains = scene::Fx;
    assert(scenes.commit(4, sc_fx_only));

    adapter.setSceneStore(scenes);
    nav.refreshActive();
    sync_runtime();

    // ── D. Quick Actions Ergonomics & Geometry Verification ──────────────────
    {
        lv_obj_t* lib_btn = findButton(root, "TIMBRES >");
        lv_obj_t* arp_lbl = findLabel(root, "ARP ", true);
        lv_obj_t* seq_lbl = findLabel(root, "SEQ ", true);
        lv_obj_t* fx_lbl = findLabel(root, "FX ", true);
        lv_obj_t* harm_lbl = findLabel(root, "HARM OFF");
        assert(lib_btn && arp_lbl && seq_lbl && fx_lbl && harm_lbl);

        lv_obj_t* pills[5] = {
            lib_btn,
            lv_obj_get_parent(arp_lbl),
            lv_obj_get_parent(seq_lbl),
            lv_obj_get_parent(fx_lbl),
            lv_obj_get_parent(harm_lbl)
        };
        Box boxes[5]{};
        for (int i = 0; i < 5; ++i) {
            boxes[i] = getCoords(pills[i]);
            assert(boxes[i].height() >= 44);
            assert(boxes[i].width() >= 80);
            if (i > 0) {
                assert(!overlaps(boxes[i - 1], boxes[i]));
                assert(boxes[i].x1 - boxes[i - 1].x2 >= 3);
            }
        }
        std::printf("QUICK_ACTIONS_GEOM lib=%dx%d arp=%dx%d seq=%dx%d fx=%dx%d harm=%dx%d\n",
                    boxes[0].width(), boxes[0].height(),
                    boxes[1].width(), boxes[1].height(),
                    boxes[2].width(), boxes[2].height(),
                    boxes[3].width(), boxes[3].height(),
                    boxes[4].width(), boxes[4].height());
    }
    capture("01_perf_stage_refined");
    capture("13_quick_actions_hitboxes");

    // ── E. Harmony Identification (OFF, Q ON, CH ON, Q+CH) + Overlay ─────────
    assert(findLabel(root, "HARM OFF"));
    capture("02_harmony_off");

    assert(adapter.requestParameterBool("harmony.scale_enable", true).accepted());
    sync_runtime();
    assert(findLabel(root, "Q ON"));
    capture("03_quantize_on_chord_off");

    assert(adapter.requestParameterBool("harmony.scale_enable", false).accepted());
    sync_runtime();
    assert(adapter.requestParameterBool("harmony.chord_enable", true).accepted());
    sync_runtime();
    assert(findLabel(root, "CH ON"));
    capture("04_quantize_off_chord_on");

    assert(adapter.requestParameterBool("harmony.scale_enable", true).accepted());
    sync_runtime();
    assert(findLabel(root, "Q+CH"));
    capture("05_quantize_chord_on");

    // Quick Action harm_pill_ opens Harmony Overlay in 1 tap
    {
        auto* hl = findLabel(root, "Q+CH");
        assert(hl);
        clickObj(lv_obj_get_parent(hl), app);
        sync_runtime();
        assert(findLabel(app.rootObject(), "HARMONIA > ESCALA (Q), ACORDE (CH) E ESCOPO", true));
        clickRoot("FECHAR");
    }

    // Reset harmony off for cleaner states below
    assert(adapter.requestParameterBool("harmony.scale_enable", false).accepted());
    sync_runtime();
    assert(adapter.requestParameterBool("harmony.chord_enable", false).accepted());
    sync_runtime();

    // ── F. Outdated Navigation Descriptions Fixed ────────────────────────────
    clickRoot("TIMBRES");
    clickRoot("TIMBRES");
    assert(findLabel(app.rootObject(), "Editor contextual FM 6-OP / Subtrativo"));
    assert(!findLabel(app.rootObject(), "7 abas", true));
    capture("14_module_selector_updated");
    clickRoot("TOCAR");
    assert(nav.currentScreen() == ScreenId::Performance && nav.subTab(ScreenId::Performance) == 0);

    // ── A. TOCAR as Musical Home + Note/Transport Integrity ──────────────────
    // Start Arp (Latched), Sequencer, USB MIDI sustained note, CC64 Sustain
    assert(adapter.requestParameterBool("arp.enable", true).accepted());
    sync_runtime();
    assert(adapter.requestArpTransport(true).accepted());
    sync_runtime();
    const uint8_t usb_on[]{0x09, 0x90, 60, 100};
    assert(runtime.usbInput().transfer(usb_on, sizeof(usb_on), preview::g_sim_tick_ms));
    const uint8_t cc64_on[]{0x0B, 0xB0, 64, 127};
    assert(runtime.usbInput().transfer(cc64_on, sizeof(cc64_on), preview::g_sim_tick_ms));
    request(seq::Action::Play);
    for (unsigned i = 0; i < 8; ++i) sync_runtime();
    assert(runtime.snapshot().arp.length > 0 && adapter.sequencerStatus()->playing);

    // 1. From MACROS 5-8 -> TOCAR -> MACROS 1-4
    click("MACROS 5-8");
    assert(nav.subTab(ScreenId::Performance) == 4);
    clickRoot("TOCAR");
    assert(nav.currentScreen() == ScreenId::Performance && nav.subTab(ScreenId::Performance) == 0);

    // 2. Already in MACROS 1-4 -> TOCAR -> stays without rebuild
    const uint32_t tr_before = nav.transitionCount();
    clickRoot("TOCAR");
    assert(nav.transitionCount() == tr_before);

    // 3. From CENAS 1-8 -> TOCAR -> MACROS 1-4
    click("CENAS 1-8");
    assert(nav.subTab(ScreenId::Performance) == 5);
    clickRoot("TOCAR");
    assert(nav.currentScreen() == ScreenId::Performance && nav.subTab(ScreenId::Performance) == 0);
    capture("11_return_scenes_to_macros_tocar");

    // 4. From TECLADO with Touch note held -> TOCAR -> releases Touch note only, keeps USB/Arp/Seq
    click("TECLADO");
    assert(nav.subTab(ScreenId::Performance) == 1);
    const UiCommandResult touch_r = adapter.requestNoteOn(67, 100);
    assert(touch_r.accepted());
    sync_runtime();
    clickRoot("TOCAR");
    assert(nav.currentScreen() == ScreenId::Performance && nav.subTab(ScreenId::Performance) == 0);
    assert(adapter.sequencerStatus()->playing);
    assert(runtime.snapshot().arp.length > 0);
    capture("12_return_touch_kb_to_macros");

    // 5. Destructive confirmation open -> TOCAR does NOT silently close it
    click("CENAS 1-8");
    click("LIMPAR");
    assert(nav.activeView()->hasBlockingConfirmation());
    clickRoot("TOCAR");
    assert(nav.activeView()->hasBlockingConfirmation());
    assert(nav.subTab(ScreenId::Performance) == 5);
    clickRoot("CANCELAR");
    assert(!nav.activeView()->hasBlockingConfirmation());
    clickRoot("TOCAR");
    assert(nav.subTab(ScreenId::Performance) == 0);

    // 6. From other areas (Timbres, Ritmo, Fx, Mais) -> TOCAR -> MACROS 1-4
    for (ScreenId scr : {ScreenId::PatchBrowser, ScreenId::Sequencer, ScreenId::Effects, ScreenId::MidiSettings}) {
        assert(nav.navigateTo(scr));
        sync_runtime();
        clickRoot("TOCAR");
        assert(nav.currentScreen() == ScreenId::Performance && nav.subTab(ScreenId::Performance) == 0);
    }

    // ── C. Scene Recall Domain Summary (all 9 cases) ─────────────────────────
    click("CENAS 1-8");
    // 1. Full recall without locks (TEMPO(OFF) because SceneOperation::apply_tempo == false)
    setSessionLockMask(0);
    nav.refreshActive();
    sync_runtime();
    assert(findLabel(root, "RECALL TOTAL: PATCH* | PATTERN | FX | HARMONY | ARP | TEMPO(OFF)"));
    capture("08_scene_recall_total_no_locks");

    // 2. Full recall with FX locked
    setSessionLockMask(perf_lock::Fx);
    nav.refreshActive();
    sync_runtime();
    assert(findLabel(root, "RECALL TOTAL: PATCH* | PATTERN | FX(LK) | HARMONY | ARP | TEMPO(OFF)"));

    // 3. Full recall with Harmony & Arp locked
    setSessionLockMask(perf_lock::Harmony | perf_lock::Arp);
    nav.refreshActive();
    sync_runtime();
    assert(findLabel(root, "RECALL TOTAL: PATCH* | PATTERN | FX | HARMONY(LK) | ARP(LK) | TEMPO(OFF)"));
    capture("09_scene_recall_total_with_locks");

    // 6. Pattern unavailable (C4)
    click("C4:", true);
    assert(findLabel(root, "RECALL TOTAL: PATTERN | FX | HARMONY(LK) | ARP(LK) [PAT N/A]", true) ||
           findLabel(root, "RECALL TOTAL: PATTERN | FX [PAT N/A]", true));

    // 7. Patch incompatible / unavailable (C3)
    click("C3:", true);
    assert(findLabel(root, "RECALL TOTAL: PATCH* | TEMPO(OFF) [PATCH N/A]"));

    // 8. No effective domains (C5 FX-only with FX locked)
    setSessionLockMask(perf_lock::Fx | perf_lock::Harmony);
    click("C5:", true);
    assert(findLabel(root, "RECALL TOTAL: NENHUM (LOCKED) [NO DOM]"));

    // 4. Selective recall (ScenesScreen) preserves its own domain selection
    click("SELETIVO...");
    assert(nav.currentScreen() == ScreenId::Scenes);
    click("01 INTRO", true);
    click("LOAD");
    assert(findLabel(root, "EFFECTIVE PATCH / PATTERN / ARP"));
    capture("10_scene_recall_selective");
    click("CLOSE");
    clickRoot("TOCAR");

    // ── B. Persistent Context + Pattern Pending + Active Scene + Failed Recall ─
    // Load Pattern 01, schedule Pattern 02 NEXT BAR
    assert(nav.navigateTo(ScreenId::Patterns));
    sync_runtime();
    click("01 STAGE", true);
    click("LOAD");
    for (unsigned i = 0; i < 4; ++i) sync_runtime();
    click("SWITCH MODE");
    click("02 NEXT", true);
    click("LOAD");
    sync_runtime();
    clickRoot("TOCAR");
    assert(findLabel(root, "PAT 01->02", true) || findLabel(root, "SEQ P>2"));
    capture("06_pattern_active_pending");

    // Let Pattern 02 finish applying at NEXT BAR so patternBusy() clears before Scene Recall
    for (unsigned i = 0; i < 500 && adapter.sequencerStatus()->pending_slot != seq::kNoSlot; ++i) {
        sync_runtime(25);
    }

    // Recall C2 (CHORUS LEAD) and let it become active
    SceneOperation sc2_op{};
    sc2_op.slot = 1;
    sc2_op.mode = scene::RecallMode::Immediate;
    sc2_op.locks = sessionLockMask();
    adapter.requestScene(sc2_op);
    for (unsigned i = 0; i < 10; ++i) sync_runtime();
    assert(adapter.sceneView().recall.active == 1);

    // Now trigger failed recall of C3 (BAD PATCH) while C2 remains active + locks active
    SceneOperation sc3_op{};
    sc3_op.slot = 2;
    sc3_op.mode = scene::RecallMode::Immediate;
    adapter.requestScene(sc3_op);
    sync_runtime();
    sync_runtime();
    assert(adapter.sceneView().recall.phase == scene::RecallPhase::Failed);
    assert(findLabel(root, "SC: CHORUS LEAD", true));
    assert(findLabel(root, "RECALL: FAILED", true) || findLabel(root, "RECALL C3: FAILED", true));
    capture("07_active_scene_recall_failed");

    // Long patch name + simultaneous states
    assert(adapter.confirmPatchDiscard(130, 0).accepted());
    for (unsigned i = 0; i < 8; ++i) sync_runtime();
    capture("15_long_patch_simultaneous_states");

    // Independent navigation safety fixture after transport assertions.
    assert(nav.returnToPerformance());
    sync_runtime();
    // 5. Destructive confirmation open -> TOCAR does NOT silently close it
    click("CENAS 1-8");
    nav.requestNavigateTo(ScreenId::Effects);
    lv_obj_send_event(findButton(root, "LIMPAR"), LV_EVENT_CLICKED, nullptr);
    assert(!nav.processPendingNavigation());
    assert(nav.activeView()->hasBlockingConfirmation());
    capture("pads_17_confirmation");
    const auto protected_depth = nav.historyDepth();
    const auto protected_transitions = nav.transitionCount();
    for (const char* area : {"TIMBRES", "RITMO", "FX", "MAIS"}) {
        clickRoot(area);
        assert(nav.activeView()->hasBlockingConfirmation());
    }
    assert(!nav.navigateTo(ScreenId::Effects));
    assert(!nav.navigateTo(ScreenId::Performance, 0));
    assert(!nav.back());
    nav.requestNavigateTo(ScreenId::MidiSettings, 1);
    assert(!nav.processPendingNavigation());
    runtime.usbInput().enableControls();
    runtime.usbInput().identifyDevice(0x4353, 0x4b4d);
    adapter.setMidiInput(runtime.usbInput());
    runtime.usbInput().setLegacyProfile(true);
    for (uint8_t note : {uint8_t(46), uint8_t(49), uint8_t(50)}) {
        const uint8_t packet[4] = {0x09, 0x99, note, 100};
        assert(runtime.usbInput().packet(packet, preview::g_sim_tick_ms));
        sync_runtime();
        assert(nav.activeView()->hasBlockingConfirmation());
        const uint8_t off[4] = {0x08, 0x89, note, 0};
        assert(runtime.usbInput().packet(off, preview::g_sim_tick_ms));
        sync_runtime();
    }
    assert(nav.historyDepth() == protected_depth);
    assert(nav.transitionCount() == protected_transitions);
    clickRoot("PANIC");
    assert(!nav.activeView()->hasBlockingConfirmation());
    click("LIMPAR");
    assert(nav.activeView()->hasBlockingConfirmation());
    clickRoot("TOCAR");
    assert(nav.activeView()->hasBlockingConfirmation());
    assert(nav.subTab(ScreenId::Performance) == 5);
    clickRoot("CANCELAR");
    assert(!nav.activeView()->hasBlockingConfirmation());
    clickRoot("TOCAR");
    assert(nav.subTab(ScreenId::Performance) == 0);

    // Save Pattern and externally mounted User Presets obey the same policy.
    assert(nav.navigateTo(ScreenId::Patterns));
    sync_runtime();
    click("SAVE");
    assert(nav.hasBlockingConfirmation());
    assert(!nav.navigateTo(ScreenId::Effects));
    assert(!nav.back());
    click("CANCEL");
    assert(!nav.hasBlockingConfirmation());
    assert(nav.navigateTo(ScreenId::SoundEditor));
    sync_runtime();
    showUserPresetPanel(root, adapter);
    assert(nav.hasBlockingConfirmation());
    assert(!nav.navigateTo(ScreenId::Effects));
    nav.requestNavigateTo(ScreenId::MidiSettings, 1);
    assert(!nav.processPendingNavigation());
    clickBtn(lv_screen_active(), "CLOSE", app);
    assert(!nav.hasBlockingConfirmation());
    assert(nav.navigateTo(ScreenId::Effects));


    // Harmonic Pads: real LVGL events and real owner acknowledgements.
    clickRoot("PANIC");sync_runtime();
    assert(nav.returnToPerformance());sync_runtime();click("PADS");
    auto padButton=[&](const char* chord){return findButton(root,chord,true);};
    auto press=[&](lv_obj_t* p){assert(p);lv_obj_send_event(p,LV_EVENT_PRESSED,nullptr);sync_runtime();};
    auto lift=[&](lv_obj_t* p,lv_event_code_t e=LV_EVENT_RELEASED){lv_obj_send_event(p,e,nullptr);sync_runtime();};
    capture("pads_01_c_major");
    assert(adapter.requestParameterBool("arp.enable",true).accepted());sync_runtime();
    assert(adapter.requestArpTransport(true).accepted());sync_runtime();
    click("MACROS 1-4");click("PADS");
    assert(adapter.arpStatus()->config.enabled);capture("pads_13_arp");
    request(seq::Action::Play);click("MACROS 1-4");click("PADS");
    assert(adapter.sequencerStatus()->playing);capture("pads_14_sequencer");
    request(seq::Action::Stop);assert(adapter.requestArpTransport(false).accepted());sync_runtime();
    clickRoot("PANIC");sync_runtime();

    auto* tonic=padButton("C\n");press(tonic);assert(runtime.snapshot().active_notes==3);
    capture("pads_05_pressed");lift(tonic,LV_EVENT_PRESS_LOST);assert(!runtime.snapshot().active_notes);
    click("MOMENTARY");press(tonic);lift(tonic);assert(runtime.snapshot().active_notes==3);
    auto* second=padButton("Dm\n");press(second);lift(second);assert(runtime.snapshot().active_notes==6);
    capture("pads_06_two_chords");capture("pads_07_hold");
    auto* third=padButton("Em\n");press(third);lift(third);assert(runtime.snapshot().active_notes==6);
    capture("pads_12_capacity");click("RELEASE ALL PADS");assert(!runtime.snapshot().active_notes);
    capture("pads_08_released");
    // Frozen held chord while local root changes. Global Harmony stays unchanged.
    const auto global_before=harmony::pack(runtime.snapshot().harmony);
    press(tonic);lift(tonic);click("ROOT C");
    assert(runtime.snapshot().pads.pads[0].notes[0]==60);
    assert(harmony::pack(runtime.snapshot().harmony)==global_before);
    click("RELEASE ALL PADS");click("ROOT C#");press(padButton("D\n"));
    assert(runtime.snapshot().pads.pads[0].notes[0]==62);
    clickRoot("PANIC");assert(!runtime.snapshot().active_notes);capture("pads_16_panic");
    // C major -> A natural minor -> D Dorian -> G Mixolydian.
    for(unsigned i=0;i<7;++i)clickBtn(root,"ROOT ",app,true);
    click("SCALE MAJOR");capture("pads_02_a_minor");
    for(unsigned i=0;i<5;++i)clickBtn(root,"ROOT ",app,true);
    for(unsigned i=0;i<5;++i)clickBtn(root,"SCALE ",app,true);
    capture("pads_03_d_dorian");
    for(unsigned i=0;i<5;++i)clickBtn(root,"ROOT ",app,true);
    for(unsigned i=0;i<3;++i)clickBtn(root,"SCALE ",app,true);
    capture("pads_04_g_mixolydian");clickBtn(root,"SCALE ",app,true);
    capture("pads_11_incompatible");
    // Restore C major for stress, retaining no source groups.
    for(unsigned i=0;i<5;++i)clickBtn(root,"ROOT ",app,true);
    clickBtn(root,"SCALE ",app,true);capture("pads_10_scale");
    clickBtn(root,"OCT ",app,true);capture("pads_09_octave");
    for(unsigned i=0;i<8;++i)clickBtn(root,"OCT ",app,true);
    click("HOLD");
    click("MACROS 1-4");click("PADS");sync_runtime(500);
    lv_mem_monitor_t pad_mem_before{};lv_mem_monitor(&pad_mem_before);
    const auto pad_objects=countObjects(lv_screen_active());
    std::vector<double> pad_times;pad_times.reserve(1000);
    for(unsigned i=0;i<1000;++i) {
        const auto pad_started=std::chrono::steady_clock::now();
        tonic=padButton("C\n");press(tonic);lift(tonic);
        pad_times.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-pad_started).count());
        assert(!runtime.snapshot().active_notes);
        if(i<500){click("MACROS 1-4");click("PADS");}
        if(i<200){clickBtn(root,"ROOT ",app,true);for(unsigned j=0;j<11;++j)clickBtn(root,"ROOT ",app,true);}
        if(i<200){for(unsigned j=0;j<9;++j)clickBtn(root,"OCT ",app,true);}
        if(i<200){click("MOMENTARY");click("HOLD");}
        if(i<100){
            click("MOMENTARY");press(padButton("C\n"));lift(padButton("C\n"));
            press(padButton("Dm\n"));lift(padButton("Dm\n"));
            press(padButton("Em\n"));lift(padButton("Em\n"));
            assert(runtime.snapshot().active_notes==6);
            click("RELEASE ALL PADS");assert(!runtime.snapshot().active_notes);click("HOLD");
        }
        if(i<100){press(padButton("C\n"));clickRoot("PANIC");assert(!runtime.snapshot().active_notes);}
        if(i<100){press(padButton("C\n"));click("MACROS 1-4");assert(!runtime.snapshot().active_notes);click("PADS");}
    }
    click("MACROS 1-4");click("PADS");sync_runtime(500);
    lv_mem_monitor_t pad_mem_after{};lv_mem_monitor(&pad_mem_after);
    assert(countObjects(lv_screen_active())==pad_objects);
    std::printf("PAD HEAP before=%zu after=%zu objects=%u->%u\n",pad_mem_before.free_size,pad_mem_after.free_size,pad_objects,countObjects(lv_screen_active()));
    assert(pad_mem_after.free_size>=pad_mem_before.free_size);
    assert(pad_mem_after.max_used<=128u*1024u);
    std::printf("PADS STRESS objects=%u free=%zu->%zu largest=%zu frag=%u peak=%zu active=%u drops=%u\n",pad_objects,pad_mem_before.free_size,pad_mem_after.free_size,pad_mem_after.free_biggest_size,pad_mem_after.frag_pct,pad_mem_after.max_used,runtime.snapshot().active_notes,runtime.commands().overflows());
    const auto pad_latency=computeLatency(pad_times);
    std::printf("PADS HOST event-cycle median=%.1f p95=%.1f max=%.1f us\n",pad_latency.median_us,pad_latency.p95_us,pad_latency.max_us);
    // Unmount releases only pads, preserving same-pitch USB and keyboard groups.
    assert(adapter.requestParameterBool("arp.enable",false).accepted());sync_runtime();
    amy::Command normal{};normal.type=amy::CommandType::Harmony;normal.token=harmony::pack(harmony::Config{});
    assert(runtime.commands().push(normal));sync_runtime();
    const auto keyboard_note=adapter.requestNoteOn(60,100);assert(keyboard_note.accepted());
    const uint8_t independent_usb[]={0x09,0x90,60,100};assert(runtime.usbInput().transfer(independent_usb,4,preview::g_sim_tick_ms));
    sync_runtime();press(padButton("C\n"));assert(runtime.snapshot().active_notes==5);
    clickRoot("FX");assert(runtime.snapshot().active_notes==2);
    assert(adapter.requestNoteOff(60,keyboard_note.token).accepted());
    const uint8_t independent_off[]={0x08,0x80,60,0};assert(runtime.usbInput().transfer(independent_off,4,preview::g_sim_tick_ms));
    sync_runtime();assert(!runtime.snapshot().active_notes);
    const auto stale_pad_snapshot=runtime.snapshot();clickRoot("PANIC");
    perf.publish(stale_pad_snapshot,true,120,0);
    assert(adapter.padState()->generation!=stale_pad_snapshot.pads.generation);
    sync_runtime();clickRoot("TOCAR");capture("pads_15_macros");

    // ── Stress Test (Section 10 minimums) ────────────────────────────────────
    // 1000 TOCAR returns, 500 overlays, 500 module transitions,
    // 200 scene recalls, 200 pattern switches, 100 harmony toggles, 100 touch kb cycles
    assert(nav.navigateTo(ScreenId::Performance));
    sync_runtime();
    const auto objs_before = countObjects(lv_screen_active());
    lv_mem_monitor_t mem_before{};
    lv_mem_monitor(&mem_before);

    lv_obj_t* harm_hdr_btn = findButton(app.rootObject(), "C MAJ", true);
    assert(harm_hdr_btn);
    std::vector<double> times;
    times.reserve(1000);
    for (unsigned i = 0; i < 1000; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        if (i < 500) {
            assert(nav.navigateTo(i % 2 ? ScreenId::Effects : ScreenId::Sequencer));
            clickObj(harm_hdr_btn, app);
        } else {
            assert(nav.navigateTo(ScreenId::Performance, uint8_t((i % 2) ? 5 : 4)));
        }
        if (i < 200) {
            SceneOperation s_op{};
            s_op.slot = uint8_t(i % 2);
            s_op.locks = sessionLockMask();
            adapter.requestScene(s_op);
            PatternOperation p_op{};
            p_op.action = PatternAction::Load;
            p_op.slot = uint8_t(i % 2);
            adapter.requestPattern(p_op);
        }
        if (i < 100) {
            assert(adapter.requestParameterBool("harmony.scale_enable", (i % 2) == 0).accepted());
            assert(nav.navigateTo(ScreenId::Performance, 1));
            const UiCommandResult tr = adapter.requestNoteOn(64, 90);
            if (tr.accepted()) adapter.requestNoteOff(64, tr.token);
        }
        clickRoot("TOCAR");
        assert(nav.currentScreen() == ScreenId::Performance && nav.subTab(ScreenId::Performance) == 0);
        sync_runtime(1);
        times.push_back(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count());
    }

    lv_mem_monitor_t mem_after{};
    lv_mem_monitor(&mem_after);
    const auto objs_after = countObjects(lv_screen_active());
    const auto stats = computeLatency(times);
    std::printf("018D1 STRESS objs=%u->%u free=%zu->%zu largest=%zu->%zu peak=%zu frag=%u->%u median=%.1f p95=%.1f max=%.1f us\n",
                objs_before, objs_after, mem_before.free_size, mem_after.free_size,
                mem_before.free_biggest_size, mem_after.free_biggest_size, mem_after.max_used,
                mem_before.frag_pct, mem_after.frag_pct, stats.median_us, stats.p95_us, stats.max_us);
    assert(objs_before == objs_after);
    assert(mem_after.free_size >= mem_before.free_size);
    assert(mem_after.max_used <= 128u * 1024u);

    nav.shutdown();
    std::puts("PASS 0.18-D.1 performance polish / host simulation");
    return 0;
}
