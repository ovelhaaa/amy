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
    auto* lbl = findLabel(root, text, prefix);
    assert(lbl != nullptr);
    auto* btn = lv_obj_get_parent(lbl);
    assert(lv_obj_check_type(btn, &lv_button_class));
    return btn;
}

void clickBtn(lv_obj_t* root, const char* text, UiBlueprint& app, bool prefix = false) {
    lv_obj_t* btn = findButton(root, text, prefix);
    lv_obj_send_event(btn, LV_EVENT_CLICKED, nullptr);
    preview::pumpFrame(app, 25);
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
    const char* out_dir = (argc >= 2) ? argv[1] : "smk-p4/docs/ui_018c1_shots";

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

    auto make_seq_req = [](seq::Action a) {
        seq::Request r{};
        r.action = a;
        return r;
    };

    auto sync_runtime = [&](uint32_t dt_ms = 25) {
        preview::g_sim_tick_ms += dt_ms;
        runtime.apply(preview::g_sim_tick_ms);
        assert(runtime.render());
        perf.publish(runtime.snapshot(), runtime.snapshot().active_notes == 0, 120, 0);
        preview::pumpFrame(app, dt_ms);
    };

    sync_runtime();

    const auto capture = [&](const char* name) {
        char path[256]{};
        std::snprintf(path, sizeof(path), "%s/%s.ppm", out_dir, name);
        preview::pumpFrame(app, 25);
        assert(preview::saveFramebufferPpm(path));
        std::printf("CAPTURED: %s\n", path);
    };

    // ── 1. Prepare musical state & scenes for realistic captures ──────────────
    assert(adapter.requestParameterInt("perf.bpm", 124).accepted());
    sync_runtime();
    assert(adapter.requestParameterBool("harmony.scale_enable", true).accepted());
    sync_runtime();
    assert(adapter.requestParameterBool("harmony.chord_enable", true).accepted());
    sync_runtime();
    assert(adapter.requestParameterInt("harmony.root_note", 2).accepted());   // D
    sync_runtime();
    assert(adapter.requestParameterInt("harmony.scale_type", 2).accepted());  // MIN
    sync_runtime();

    // Save Scene 1 & Scene 2 for real scene state captures
    {
        scene::Slot s1{};
        s1.state = seq::SlotState::Valid;
        std::snprintf(s1.config.name, sizeof(s1.config.name), "INTRO");
        s1.config.domains = scene::Patch | scene::Tempo;
        s1.config.patch_id = runtime.snapshot().patch_id;
        s1.config.bpm = 124;
        assert(scenes.commit(0, s1));

        scene::Slot s2{};
        s2.state = seq::SlotState::Valid;
        std::snprintf(s2.config.name, sizeof(s2.config.name), "CHORUS");
        s2.config.domains = scene::Patch | scene::Tempo;
        s2.config.patch_id = runtime.snapshot().patch_id;
        s2.config.bpm = 124;
        assert(scenes.commit(1, s2));

        adapter.setSceneStore(scenes);
        assert(adapter.sceneSlot(0) && adapter.sceneSlot(0)->state == seq::SlotState::Valid);
        assert(adapter.sceneSlot(1) && adapter.sceneSlot(1)->state == seq::SlotState::Valid);

        SceneOperation load1{};
        load1.action = SceneAction::Load;
        load1.slot = 0;
        adapter.requestScene(load1);
        for (int k = 0; k < 4; ++k) sync_runtime();
    }

    // Capture 1: Tocar - Macros 1-4
    assert(app.navigation().navigateTo(ScreenId::Performance, 0));
    sync_runtime();
    capture("01_tocar_macros_1_4");

    // Capture 2: Tocar - Macros 5-8
    clickBtn(app.contentObject(), "MACROS 5-8", app);
    capture("02_tocar_macros_5_8");

    // Capture 3: Tocar - Cenas
    clickBtn(app.contentObject(), "CENAS 1-8", app);
    capture("03_tocar_cenas");

    // Capture 4: Tocar - Teclado Touch
    clickBtn(app.contentObject(), "TECLADO", app);
    lv_obj_t* key_c4 = findButton(app.contentObject(), "C4");
    lv_obj_send_event(key_c4, LV_EVENT_PRESSED, nullptr);
    sync_runtime();
    assert(adapter.isNoteHeld(60));
    capture("04_tocar_teclado_touch");

    // Leaving TECLADO releases ONLY Touch keyboard note
    clickBtn(app.contentObject(), "MACROS 1-4", app);
    sync_runtime();
    assert(!adapter.isNoteHeld(60));

    // Capture 5: Cabecalho harmonico em uso (Harmony overlay)
    app.statusStrip().openHarmony(adapter);
    sync_runtime();
    capture("05_header_harmony_overlay");
    app.statusStrip().closeOverlays();
    sync_runtime();

    // Capture 6: Overlay de BPM
    app.statusStrip().openTempo(adapter);
    sync_runtime();
    capture("06_overlay_bpm");
    app.statusStrip().closeOverlays();
    sync_runtime();

    // Capture 7: FX - Delay
    lv_obj_t* footer = lv_obj_get_child(app.rootObject(), 2);
    clickBtn(footer, "FX", app);
    sync_runtime();
    assert(app.navigation().currentScreen() == ScreenId::Effects);
    clickBtn(app.contentObject(), "DELAY", app);
    sync_runtime();
    capture("07_fx_delay");

    // Capture 8: FX - Master
    clickBtn(app.contentObject(), "MASTER", app);
    sync_runtime();
    capture("08_fx_master");

    // Capture 9: Timbres - selecao de modulo
    clickBtn(footer, "TIMBRES", app);
    sync_runtime();
    app.statusStrip().openAreaModules(UiArea::Timbres, adapter, app.navigation());
    sync_runtime();
    capture("09_timbres_selecao");
    app.statusStrip().closeOverlays();

    // Capture 10: Ritmo - selecao de modulo
    clickBtn(footer, "RITMO", app);
    sync_runtime();
    app.statusStrip().openAreaModules(UiArea::Ritmo, adapter, app.navigation());
    sync_runtime();
    capture("10_ritmo_selecao");
    app.statusStrip().closeOverlays();

    // Capture 11: Mais - selecao de modulo
    clickBtn(footer, "MAIS", app);
    sync_runtime();
    app.statusStrip().openAreaModules(UiArea::Mais, adapter, app.navigation());
    sync_runtime();
    capture("11_mais_selecao");
    app.statusStrip().closeOverlays();

    // Capture 12: Cena pendente e erro de recall (NextBar pending on C2 + Incompatible C8 on CENAS 1-8)
    {
        scene::Slot bad_slot{};
        bad_slot.state = seq::SlotState::Valid;
        std::snprintf(bad_slot.config.name, sizeof(bad_slot.config.name), "BADPATCH");
        bad_slot.config.domains = scene::Patch | scene::Tempo;
        bad_slot.config.bpm = 140;
        bad_slot.config.patch_id = UINT16_MAX;
        assert(scenes.commit(7, bad_slot));
        adapter.setSceneStore(scenes);
    }
    assert(adapter.requestSequencer(make_seq_req(seq::Action::Play)).accepted());
    sync_runtime();
    SceneOperation next_bar_load{};
    next_bar_load.action = SceneAction::Load;
    next_bar_load.slot = 1;
    next_bar_load.mode = scene::RecallMode::NextBar;
    adapter.requestScene(next_bar_load);
    sync_runtime();
    assert(app.navigation().navigateTo(ScreenId::Performance, 5));
    sync_runtime();
    capture("12_cena_pendente_erro_recall");
    assert(adapter.requestSequencer(make_seq_req(seq::Action::Stop)).accepted());
    adapter.requestScene({SceneAction::Cancel});
    for (int k = 0; k < 4; ++k) sync_runtime();

    // ── 2. Critical Note Ownership & Preservation Tests (Section 5 & 7) ───────
    // Hold USB MIDI chord + Sustain CC 64 + Start Sequencer + Session Locks
    assert(adapter.requestParameterBool("harmony.chord_enable", false).accepted());
    sync_runtime();
    setSessionLockMask(perf_lock::Fx | perf_lock::Harmony);
    const uint8_t usb_c4[]{0x09, 0x90, 60, 100};
    const uint8_t usb_e4[]{0x09, 0x90, 64, 100};
    const uint8_t sus_on[]{0x0B, 0xB0, 64, 127};
    assert(runtime.usbInput().transfer(usb_c4, sizeof(usb_c4), preview::g_sim_tick_ms));
    assert(runtime.usbInput().transfer(usb_e4, sizeof(usb_e4), preview::g_sim_tick_ms));
    assert(runtime.usbInput().transfer(sus_on, sizeof(sus_on), preview::g_sim_tick_ms));
    sync_runtime();
    assert(runtime.snapshot().active_notes >= 2 && runtime.snapshot().sustain_active);
    assert(adapter.requestSequencer(make_seq_req(seq::Action::Play)).accepted());
    sync_runtime();
    assert(runtime.snapshot().active_notes >= 2 && runtime.snapshot().sustain_active);
    assert(runtime.snapshot().sequencer.playing);

    // Navigate across all 5 areas: USB notes, Sustain, Sequencer, Locks MUST survive
    for (int cycle = 0; cycle < 10; ++cycle) {
        for (const char* area : {"TIMBRES", "RITMO", "FX", "MAIS", "TOCAR"}) {
            clickBtn(footer, area, app);
            sync_runtime(10);
            assert(runtime.snapshot().active_notes >= 2 && runtime.snapshot().sustain_active);
            assert(runtime.snapshot().sequencer.playing);
            assert(sessionLockMask() == (perf_lock::Fx | perf_lock::Harmony));
        }
    }

    // Touch keyboard on TECLADO releases ONLY Touch notes on exit, preserving USB chord + Seq
    assert(app.navigation().navigateTo(ScreenId::Performance, 1));
    sync_runtime();
    lv_obj_t* g4_key = findButton(app.contentObject(), "G4");
    lv_obj_send_event(g4_key, LV_EVENT_PRESSED, nullptr);
    sync_runtime();
    assert(adapter.isNoteHeld(67));
    clickBtn(footer, "FX", app);
    sync_runtime();
    assert(!adapter.isNoteHeld(67));
    assert(runtime.snapshot().active_notes >= 2 && runtime.snapshot().sustain_active);
    assert(runtime.snapshot().sequencer.playing);

    // Enable Arpeggiator + Arp Transport alongside Sequencer across navigation
    assert(adapter.requestParameterBool("arp.enable", true).accepted());
    sync_runtime();
    assert(adapter.requestArpTransport(true).accepted());
    sync_runtime();
    assert(runtime.snapshot().arp.config.enabled && runtime.snapshot().arp.armed && runtime.snapshot().sequencer.playing);
    clickBtn(footer, "RITMO", app);
    sync_runtime();
    clickBtn(footer, "TOCAR", app);
    sync_runtime();
    assert(runtime.snapshot().arp.armed && runtime.snapshot().sequencer.playing);

    // Patch change with active Performance Locks (Fx | Harmony) preserves lock mask
    assert(adapter.requestSelectPatch(135, 0, sessionLockMask()).accepted());
    sync_runtime();
    assert(sessionLockMask() == (perf_lock::Fx | perf_lock::Harmony));

    // PANIC accessible with Harmony overlay open -> clears all voices, Touch, Arp/Seq
    app.statusStrip().openHarmony(adapter);
    sync_runtime();
    clickBtn(app.rootObject(), "PANIC", app);
    sync_runtime();
    assert(runtime.snapshot().active_notes == 0);
    assert(!runtime.snapshot().sequencer.playing);
    app.statusStrip().closeOverlays();
    sync_runtime();
    std::puts("PASS: Note ownership, USB/Sustain/Arp/Seq preservation, Touch release, Locks & Overlay PANIC");

    // ── 3. Section 7 Stress Benchmarks & LVGL Memory Metrics ──────────────────
    assert(app.navigation().navigateTo(ScreenId::Performance, 0));
    sync_runtime();
    // Warm up caches (areas, FX, macros, harmony overlay)
    for (const char* area : {"TIMBRES", "RITMO", "FX", "MAIS", "TOCAR"}) {
        clickBtn(footer, area, app);
    }
    app.statusStrip().openHarmony(adapter);
    preview::pumpFrame(app, 10);
    app.statusStrip().closeOverlays();
    preview::pumpFrame(app, 10);

    lv_mem_monitor_t mem_before{}, mem_after{};
    const uint32_t objs_before = countObjects(app.rootObject());
    lv_mem_monitor(&mem_before);

    // 3a. 500 switches between the 5 areas
    std::vector<double> lat_500_areas;
    lat_500_areas.reserve(500);
    const char* kAreaCycle[5] = {"TIMBRES", "RITMO", "FX", "MAIS", "TOCAR"};
    for (int i = 0; i < 500; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        clickBtn(footer, kAreaCycle[i % 5], app);
        const auto t1 = std::chrono::steady_clock::now();
        lat_500_areas.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
        assert(lv_mem_test() == LV_RESULT_OK);
    }
    {
        lv_mem_monitor_t m{}; lv_mem_monitor(&m);
        std::printf("  after 3a (500 areas): objs=%u free=%zu frag=%u%%\n", unsigned(countObjects(app.rootObject())), m.free_size, m.frag_pct);
    }

    // 3b. 500 switches between FX modules
    clickBtn(footer, "FX", app);
    const char* kFxMods[4] = {"DELAY", "REVERB", "CHORUS", "MASTER"};
    for (int i = 0; i < 500; ++i) {
        clickBtn(app.contentObject(), kFxMods[i % 4], app);
    }
    {
        lv_mem_monitor_t m{}; lv_mem_monitor(&m);
        std::printf("  after 3b (500 fx): objs=%u free=%zu frag=%u%%\n", unsigned(countObjects(app.rootObject())), m.free_size, m.frag_pct);
    }

    // 3c. 100 toggles between macro banks (MACROS 1-4 <-> MACROS 5-8)
    clickBtn(footer, "TOCAR", app);
    for (int i = 0; i < 100; ++i) {
        clickBtn(app.contentObject(), (i % 2 == 0) ? "MACROS 5-8" : "MACROS 1-4", app);
    }
    {
        lv_mem_monitor_t m{}; lv_mem_monitor(&m);
        std::printf("  after 3c (100 macros): objs=%u free=%zu frag=%u%%\n", unsigned(countObjects(app.rootObject())), m.free_size, m.frag_pct);
    }

    // 3d. 100 open/close cycles of Harmony overlay
    for (int i = 0; i < 100; ++i) {
        app.statusStrip().openHarmony(adapter);
        preview::pumpFrame(app, 10);
        app.statusStrip().closeOverlays();
        preview::pumpFrame(app, 10);
    }
    clickBtn(footer, "FX", app);
    clickBtn(footer, "TOCAR", app);
    {
        lv_mem_monitor_t m{}; lv_mem_monitor(&m);
        std::printf("  after 3d (100 harmony): objs=%u free=%zu frag=%u%%\n", unsigned(countObjects(app.rootObject())), m.free_size, m.frag_pct);
    }

    assert(app.navigation().navigateTo(ScreenId::Performance, 0));
    sync_runtime();
    lv_mem_monitor(&mem_after);
    const uint32_t objs_after = countObjects(app.rootObject());
    const LatencyStats st = computeLatency(lat_500_areas);

    std::printf(
        "STRESS_018C1: objs=%u->%u free=%zu->%zu largest=%zu->%zu diff=%zd peak=%zu frag=%u%%->%u%% | "
        "500_area_nav_us(HOST): median=%.1f P95=%.1f max=%.1f\n",
        unsigned(objs_before), unsigned(objs_after),
        mem_before.free_size, mem_after.free_size,
        mem_before.free_biggest_size, mem_after.free_biggest_size,
        static_cast<ssize_t>(mem_before.free_size) - static_cast<ssize_t>(mem_after.free_size),
        mem_after.max_used, mem_before.frag_pct, mem_after.frag_pct,
        st.median_us, st.p95_us, st.max_us);
    std::fflush(stdout);

    assert(objs_before == objs_after);
    assert(mem_after.free_size >= mem_before.free_size);
    assert(mem_after.frag_pct <= 5);

    app.shutdown();
    std::puts("ALL 0.18-C.1 INTEGRATION & STRESS TESTS PASSED.");
    return 0;
}
