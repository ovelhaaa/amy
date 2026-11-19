#include "ui_prototype_018a.h"
#include "preview_harness.h"
#include "ui_demo_backend.h"
#include "ui_blueprint_performance_adapter.h"
#include "local_performance_backend.h"
#include "fake_nvs.h"
#include "pattern_nvs.h"
#include "performance_scene_nvs.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace smk::p4;
using namespace smk::p4::ui;
using namespace smk::p4::ui::proto018a;

namespace {

lv_point_t g_ptr_point{0, 0};
bool g_ptr_down = false;

void ptrReadCb(lv_indev_t*, lv_indev_data_t* data) {
    data->point = g_ptr_point;
    data->state = g_ptr_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

template <typename TickFn>
void clickPoint(lv_indev_t* indev, int32_t x, int32_t y, TickFn&& tick) {
    g_ptr_point = {x, y};
    g_ptr_down = true;
    ++preview::g_sim_tick_ms;
    lv_indev_read(indev);
    g_ptr_down = false;
    ++preview::g_sim_tick_ms;
    lv_indev_read(indev);
    tick();
}

template <typename TickFn>
void clickObj(lv_indev_t* indev, lv_obj_t* obj, TickFn&& tick) {
    assert(obj && !lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN));
    lv_area_t coords{};
    lv_obj_get_coords(obj, &coords);
    assert(coords.x1 >= 0 && coords.y1 >= 0 && coords.x2 < kScreenWidth && coords.y2 < kScreenHeight);
    const int32_t cx = (coords.x1 + coords.x2) / 2;
    const int32_t cy = (coords.y1 + coords.y2) / 2;
    clickPoint(indev, cx, cy, tick);
}

lv_obj_t* findLabelWithText(lv_obj_t* root, const char* needle, bool prefix = false) {
    if (!root || lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN)) return nullptr;
    if (lv_obj_check_type(root, &lv_label_class)) {
        const char* txt = lv_label_get_text(root);
        if (txt) {
            if (prefix ? (std::strncmp(txt, needle, std::strlen(needle)) == 0)
                       : (std::strcmp(txt, needle) == 0)) {
                return root;
            }
        }
    }
    const uint32_t cnt = lv_obj_get_child_count(root);
    for (uint32_t i = 0; i < cnt; ++i) {
        if (lv_obj_t* f = findLabelWithText(lv_obj_get_child(root, i), needle, prefix)) {
            return f;
        }
    }
    return nullptr;
}

lv_obj_t* findButtonWithText(lv_obj_t* root, const char* prefix_txt, bool exact = false) {
    if (!root || lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN)) return nullptr;
    if (lv_obj_check_type(root, &lv_button_class)) {
        const uint32_t cnt = lv_obj_get_child_count(root);
        for (uint32_t i = 0; i < cnt; ++i) {
            lv_obj_t* ch = lv_obj_get_child(root, i);
            if (lv_obj_check_type(ch, &lv_label_class)) {
                const char* t = lv_label_get_text(ch);
                if (t && (exact ? (std::strcmp(t, prefix_txt) == 0)
                                : (std::strncmp(t, prefix_txt, std::strlen(prefix_txt)) == 0))) {
                    return root;
                }
            }
        }
    }
    const uint32_t cnt = lv_obj_get_child_count(root);
    for (uint32_t i = 0; i < cnt; ++i) {
        if (lv_obj_t* b = findButtonWithText(lv_obj_get_child(root, i), prefix_txt, exact)) {
            return b;
        }
    }
    return nullptr;
}

void verifySubtreeGeometry(lv_obj_t* obj, bool strict_touch_h36, unsigned& widget_count) {
    if (!obj || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    ++widget_count;
    lv_area_t coords{};
    lv_obj_get_coords(obj, &coords);
    if (coords.x1 < 0 || coords.y1 < 0 || coords.x2 >= kScreenWidth || coords.y2 >= kScreenHeight) {
        const char* ltxt = lv_obj_check_type(obj, &lv_label_class) ? lv_label_get_text(obj) : "<non-label>";
        std::fprintf(stderr, "BOUNDS VIOLATION: (%d,%d)..(%d,%d) txt='%s'\n",
                     int(coords.x1), int(coords.y1), int(coords.x2), int(coords.y2), ltxt ? ltxt : "");
        assert(false && "018-A widget outside 800x480");
    }
    if (lv_obj_check_type(obj, &lv_button_class)) {
        const int32_t bw = coords.x2 - coords.x1 + 1;
        const int32_t bh = coords.y2 - coords.y1 + 1;
        // White/black touch keys can be >= 26px wide; all non-keyboard buttons are >= 40px wide
        assert(bw >= 26);
        assert(bh >= (strict_touch_h36 ? 34 : 30));
    }
    const uint32_t cnt = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < cnt; ++i) {
        verifySubtreeGeometry(lv_obj_get_child(obj, i), strict_touch_h36, widget_count);
    }
}

}  // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    preview::initHostLvgl();
    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, ptrReadCb);
    lv_indev_set_mode(indev, LV_INDEV_MODE_EVENT);

    // ── PART A: Real AMY Runtime + LocalAmyPerformanceBackend + UiBlueprintPerformanceAdapter ──
    amy::Runtime runtime;
    assert(runtime.begin());
    LocalAmyPerformanceBackend perf_backend(runtime.commands());
    UiBlueprintPerformanceAdapter adapter(perf_backend);

    NvsSceneStore scene_store;
    assert(scene_store.begin());
    scene::Slot s0{};
    s0.state = seq::SlotState::Valid;
    std::strcpy(s0.config.name, "LEAD LIVE");
    s0.config.domains = scene::Patch | scene::Fx | scene::Harmony | scene::Macros;
    s0.config.patch_id = runtime.snapshot().patch_id;
    s0.config.harmony = runtime.snapshot().harmony;
    s0.config.harmony.scale_on = true;
    s0.config.harmony.chord_on = false;
    assert(scene_store.commit(0, s0));
    adapter.setSceneStore(scene_store);

    UiShell018A shell(adapter);
    assert(shell.begin());

    auto syncStep = [&]() {
        preview::g_sim_tick_ms += 20;
        runtime.apply(preview::g_sim_tick_ms);
        runtime.loadPendingPatch();
        assert(runtime.render());
        perf_backend.publish(runtime.snapshot(), true, 0, 0);
        perf_backend.publishMidi(runtime.usbInput().isSmk25(), false, runtime.usbInput().events.load());
        shell.service(preview::g_sim_tick_ms);
        lv_timer_handler();
        lv_refr_now(nullptr);
    };

    syncStep();
    const auto& snap = runtime.snapshot();

    // [T1] 5-Area Navigation & Shell Persistence
    assert(shell.metrics().shell_builds == 1U);
    assert(shell.activeArea() == AreaId::Tocar);
    assert(!shell.sidebarBox().visible);
    assert(shell.panelBox().x == 0 && shell.panelBox().w == 800);
    assert(shell.footerBox().y == 432 && shell.footerBox().h == 48);

    for (AreaId area : {AreaId::Timbres, AreaId::Ritmo, AreaId::Fx, AreaId::Mais}) {
        clickObj(indev, shell.footerButton(area), syncStep);
        assert(shell.activeArea() == area);
        assert(shell.sidebarBox().visible && shell.sidebarBox().w == 160);
        assert(shell.panelBox().x == 160 && shell.panelBox().w == 640);
        assert(shell.metrics().shell_builds == 1U);
    }
    clickObj(indev, shell.footerButton(AreaId::Tocar), syncStep);
    assert(shell.activeArea() == AreaId::Tocar && !shell.sidebarBox().visible);
    std::puts("PASS: [T1] 5 fixed areas, Tocar full 800px stage, 4 sidebar+panel areas, shell built once");

    // [T2] Module & Sub-tab Context Persistence + Strict Layout Bounds (Variant A & B)
    clickObj(indev, shell.footerButton(AreaId::Fx), syncStep);
    shell.selectFxModule(FxModule::Reverb);
    syncStep();
    clickObj(indev, shell.footerButton(AreaId::Ritmo), syncStep);
    shell.selectRitmoModule(RitmoModule::Arpejador);
    syncStep();
    clickObj(indev, shell.footerButton(AreaId::Timbres), syncStep);
    shell.selectTimbresModule(TimbresModule::Sintese);
    shell.selectSynthSubTab(SynthSubTab::FiltroEnv);
    syncStep();

    clickObj(indev, shell.footerButton(AreaId::Fx), syncStep);
    assert(shell.fxModule() == FxModule::Reverb);
    clickObj(indev, shell.footerButton(AreaId::Ritmo), syncStep);
    assert(shell.ritmoModule() == RitmoModule::Arpejador);
    clickObj(indev, shell.footerButton(AreaId::Timbres), syncStep);
    assert(shell.timbresModule() == TimbresModule::Sintese);
    assert(shell.synthSubTab() == SynthSubTab::FiltroEnv);

    for (HeaderVariant hv : {HeaderVariant::CompactOverlay, HeaderVariant::ExpandedTwoRow}) {
        shell.setHeaderVariant(hv);
        syncStep();
        int32_t min_gap = 0;
        assert(shell.headerTargetsDisjoint(&min_gap));
        assert(min_gap >= 3);
        const bool strict_h = (hv == HeaderVariant::CompactOverlay);

        for (uint8_t m = 0; m < uint8_t(TocarMode::Count); ++m) {
            shell.selectTocarMode(static_cast<TocarMode>(m));
            syncStep();
            unsigned wc = 0;
            verifySubtreeGeometry(shell.root(), strict_h, wc);
            assert(wc >= 15);
        }
        shell.selectTocarMode(TocarMode::Macros1To4);
        for (uint8_t m = 0; m < uint8_t(TimbresModule::Count); ++m) {
            shell.selectTimbresModule(static_cast<TimbresModule>(m));
            syncStep();
            unsigned wc = 0;
            verifySubtreeGeometry(shell.root(), strict_h, wc);
        }
        for (uint8_t m = 0; m < uint8_t(RitmoModule::Count); ++m) {
            shell.selectRitmoModule(static_cast<RitmoModule>(m));
            if (static_cast<RitmoModule>(m) == RitmoModule::Sequenciador) {
                shell.selectSeqViewMode(SeqViewMode::Play);
                syncStep();
                unsigned wc = 0;
                verifySubtreeGeometry(shell.root(), strict_h, wc);
                shell.selectSeqViewMode(SeqViewMode::Edit);
            }
            syncStep();
            unsigned wc = 0;
            verifySubtreeGeometry(shell.root(), strict_h, wc);
        }
        for (uint8_t m = 0; m < uint8_t(FxModule::Count); ++m) {
            shell.selectFxModule(static_cast<FxModule>(m));
            syncStep();
            unsigned wc = 0;
            verifySubtreeGeometry(shell.root(), strict_h, wc);
        }
        for (uint8_t m = 0; m < uint8_t(MaisModule::Count); ++m) {
            shell.selectMaisModule(static_cast<MaisModule>(m));
            syncStep();
            unsigned wc = 0;
            verifySubtreeGeometry(shell.root(), strict_h, wc);
        }
    }
    shell.setHeaderVariant(HeaderVariant::CompactOverlay);
    shell.selectTocarMode(TocarMode::Macros1To4);
    syncStep();
    std::puts("PASS: [T2] Session module/sub-tab persistence and strict 800x480 bounds verified (Hdr A & B)");

    // [T3] Header Independent Q/CH, Rejection, Overlay PANIC Accessibility
    assert(!snap.harmony.scale_on && !snap.harmony.chord_on);
    clickObj(indev, shell.qButton(), syncStep);
    assert(snap.harmony.scale_on && !snap.harmony.chord_on);
    clickObj(indev, shell.chButton(), syncStep);
    assert(snap.harmony.scale_on && snap.harmony.chord_on);
    clickObj(indev, shell.qButton(), syncStep);
    assert(!snap.harmony.scale_on && snap.harmony.chord_on);
    clickObj(indev, shell.chButton(), syncStep);
    assert(!snap.harmony.scale_on && !snap.harmony.chord_on);

    // Unsupported Chord Type ADD9 (index 10) must be rejected cleanly
    assert(!adapter.isParameterValueSupported("harmony.chord_type", 10));
    const UiCommandResult rej = adapter.requestParameterValue("harmony.chord_type", ParameterValue::makeEnum(10));
    assert(rej.state == UiCommandState::Rejected || rej.state == UiCommandState::Unsupported);

    // Open Harmony overlay in both header variants and verify PANIC remains outside backdrop & clickable
    for (HeaderVariant hv : {HeaderVariant::CompactOverlay, HeaderVariant::ExpandedTwoRow}) {
        shell.setHeaderVariant(hv);
        syncStep();
        clickObj(indev, shell.harmButton(), syncStep);
        assert(shell.activeOverlay() == OverlayKind::HarmonyContext);
        assert(shell.panicAccessibleInOverlay());
        unsigned wc = 0;
        verifySubtreeGeometry(shell.root(), hv == HeaderVariant::CompactOverlay, wc);
        clickObj(indev, findButtonWithText(shell.overlayCard(), "FECHAR"), syncStep);
        assert(shell.activeOverlay() == OverlayKind::None);
    }
    shell.setHeaderVariant(HeaderVariant::CompactOverlay);
    syncStep();

    // Hold a USB note while BPM overlay is open -> PANIC immediately clears voices without closing first
    runtime.usbInput().commands.push({amy::CommandType::NoteOn, 60, 100, 0, 0, preview::g_sim_tick_ms});
    syncStep();
    assert(snap.active_notes >= 1U);
    clickObj(indev, shell.bpmButton(), syncStep);
    assert(shell.activeOverlay() == OverlayKind::TempoBpm && shell.panicAccessibleInOverlay());
    clickObj(indev, shell.panicButton(), syncStep);
    assert(snap.active_notes == 0U);
    shell.closeOverlay();
    syncStep();
    std::puts("PASS: [T3] Independent Q/CH, rejection of ADD9, and overlay-persistent PANIC verified");

    // [T4] FX Honesty, SYNC Lock, Real Chorus OFF, Master Gaps
    shell.selectFxModule(FxModule::Delay);
    syncStep();
    assert(findLabelWithText(shell.panel(), "BYPASS INDIV", true) != nullptr);
    assert(adapter.requestParameterValue("fx.delay.mode", ParameterValue::makeEnum(1)).accepted());
    syncStep();
    // Ensure panel reflects SYNC LOCKED and disables delay time slider
    shell.selectFxModule(FxModule::Master);
    shell.selectFxModule(FxModule::Delay);
    syncStep();
    assert(findLabelWithText(shell.panel(), "SYNC LOCKED", true) != nullptr);

    shell.selectFxModule(FxModule::Chorus);
    syncStep();
    assert(snap.fx.config.chorus_mode == uint8_t(fx::ChorusMode::Off));
    assert(adapter.requestParameterValue("fx.chorus.mode", ParameterValue::makeEnum(2)).accepted());
    syncStep();
    assert(snap.fx.config.chorus_mode == uint8_t(fx::ChorusMode::JunoI));
    assert(snap.fx.applied.chorus_mode == uint8_t(fx::ChorusMode::JunoI));

    shell.selectFxModule(FxModule::Reverb);
    syncStep();
    assert(findLabelWithText(shell.panel(), "BYPASS INDIVIDUAL DE REVERB", true) != nullptr);

    shell.selectFxModule(FxModule::Master);
    syncStep();
    assert(findLabelWithText(shell.panel(), "FX.DRIVE / FX.ROUTING", true) != nullptr);
    assert(findLabelWithText(shell.panel(), "eq_mid_db", true) != nullptr ||
           findLabelWithText(shell.panel(), "GAPS DE CONTRATO", true) != nullptr);
    std::puts("PASS: [T4] FX SYNC lock, real Chorus OFF/Juno1 confirmation, and honest Delay/Reverb/Master notices verified");

    // [T5] Ritmo PLAY vs EDIT, SEQ QUANT != Header Q, STOP ARP != STOP SEQ
    shell.selectRitmoModule(RitmoModule::Sequenciador);
    shell.selectSeqViewMode(SeqViewMode::Edit);
    syncStep();
    clickObj(indev, findButtonWithText(shell.panel(), "PASSO: REST"), syncStep);
    assert(snap.sequencer_pattern.steps[0].enabled);

    clickObj(indev, findButtonWithText(shell.panel(), "PLAY SEQ"), syncStep);
    assert(snap.sequencer.playing);
    clickObj(indev, findButtonWithText(shell.panel(), "PLAY MODE"), syncStep);
    assert(shell.seqViewMode() == SeqViewMode::Play && snap.sequencer.playing);
    clickObj(indev, findButtonWithText(shell.panel(), "SEQ QUANT:"), syncStep);
    assert(snap.sequencer.quantize && !snap.harmony.scale_on);

    shell.selectRitmoModule(RitmoModule::Arpejador);
    syncStep();
    clickObj(indev, findButtonWithText(shell.panel(), "ARP: "), syncStep);
    assert(snap.arp.config.enabled && snap.arp.armed && snap.sequencer.playing);
    clickObj(indev, findButtonWithText(shell.panel(), "STOP ARP"), syncStep);
    assert(!snap.arp.armed && snap.sequencer.playing);
    clickObj(indev, findButtonWithText(shell.panel(), "PLAY ARP"), syncStep);
    assert(snap.arp.armed && snap.sequencer.playing);
    clickObj(indev, findButtonWithText(shell.panel(), "ARP: "), syncStep);
    assert(!snap.arp.config.enabled && snap.sequencer.playing);
    shell.selectRitmoModule(RitmoModule::Sequenciador);
    syncStep();
    clickObj(indev, findButtonWithText(shell.panel(), "STOP SEQ"), syncStep);
    assert(!snap.sequencer.playing);
    std::puts("PASS: [T5] Sequencer PLAY/EDIT separation, SEQ QUANT vs Header Q, and ARP!=SEQ stop verified");

    // [T6] Sustained USB MIDI Note Preserved Across Full Navigation; Touch-Only Released Leaving Teclado
    runtime.usbInput().commands.push({amy::CommandType::NoteOn, 64, 100, 0, 0, preview::g_sim_tick_ms});
    syncStep();
    assert(snap.active_notes == 1U);
    const uint32_t usb_releases_before = shell.metrics().ui_note_releases;
    for (AreaId a : {AreaId::Fx, AreaId::Ritmo, AreaId::Timbres, AreaId::Mais, AreaId::Tocar}) {
        clickObj(indev, shell.footerButton(a), syncStep);
        assert(snap.active_notes == 1U && "Navigation interrupted sustained USB MIDI note!");
    }
    assert(shell.metrics().ui_note_releases == usb_releases_before);

    // Hold a touch note on Teclado while USB E4 is active, then leave Teclado
    shell.selectTocarMode(TocarMode::VirtualKeys);
    syncStep();
    lv_obj_t* touch_c = findButtonWithText(shell.panel(), "C", true);
    assert(touch_c);
    lv_area_t kcoords{};
    lv_obj_get_coords(touch_c, &kcoords);
    g_ptr_point = {(kcoords.x1 + kcoords.x2) / 2, kcoords.y2 - 8};
    g_ptr_down = true;
    ++preview::g_sim_tick_ms;
    lv_indev_read(indev);
    syncStep();
    assert(snap.active_notes == 2U);
    shell.selectTocarMode(TocarMode::Macros1To4);
    syncStep();
    g_ptr_down = false;
    ++preview::g_sim_tick_ms;
    lv_indev_read(indev);
    syncStep();
    assert(shell.metrics().ui_note_releases == usb_releases_before + 1U);
    assert(snap.active_notes == 1U && "Leaving Teclado released USB note or failed to release Touch key!");
    clickObj(indev, shell.panicButton(), syncStep);
    assert(snap.active_notes == 0U);
    std::puts("PASS: [T6] USB notes untouched across navigation; Touch notes safely released on leaving Teclado");

    // [T7] Demo Mode Honesty Check & 500-Transition Navigation Stress + Timing / Heap Audit
    // Warmup 10 cycles = 50 transitions
    for (int w = 0; w < 10; ++w) {
        for (AreaId a : {AreaId::Timbres, AreaId::Ritmo, AreaId::Fx, AreaId::Mais, AreaId::Tocar}) {
            shell.selectArea(a);
            syncStep();
        }
    }
    shell.selectArea(AreaId::Tocar);
    syncStep();

    const LvglHeapStats heap_before = UiShell018A::readLvglHeapStats();
    const uint32_t obj_baseline = shell.countAllObjects();
    std::vector<double> durations_us;
    durations_us.reserve(500);

    for (int cycle = 0; cycle < 100; ++cycle) {
        for (AreaId a : {AreaId::Timbres, AreaId::Ritmo, AreaId::Fx, AreaId::Mais, AreaId::Tocar}) {
            const auto t0 = std::chrono::steady_clock::now();
            shell.selectArea(a);
            lv_timer_handler();
            lv_refr_now(nullptr);
            const auto t1 = std::chrono::steady_clock::now();
            const double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
            durations_us.push_back(us);
        }
    }
    shell.selectArea(AreaId::Tocar);
    syncStep();

    const LvglHeapStats heap_after = UiShell018A::readLvglHeapStats();
    const uint32_t obj_after = shell.countAllObjects();
    std::sort(durations_us.begin(), durations_us.end());
    const double median_us = durations_us[durations_us.size() / 2U];
    const double p95_us = durations_us[(durations_us.size() * 95U) / 100U];
    const double max_us = durations_us.back();
    const ptrdiff_t leaked = static_cast<ptrdiff_t>(heap_before.free_bytes) - static_cast<ptrdiff_t>(heap_after.free_bytes);

    std::printf("INFO: [0.18-A 500-transition host nav] objs_before=%u objs_after=%u | "
                "LVGL free_before=%uB free_after=%uB diff=%tdB peak_used=%uB frag_before=%u%% frag_after=%u%% | "
                "HOST duration (NOT touch/DSI): median=%.1fus P95=%.1fus max=%.1fus\n",
                unsigned(obj_baseline), unsigned(obj_after),
                unsigned(heap_before.free_bytes), unsigned(heap_after.free_bytes),
                leaked, unsigned(heap_after.max_used), unsigned(heap_before.frag_pct), unsigned(heap_after.frag_pct),
                median_us, p95_us, max_us);

    assert(obj_baseline == obj_after);
    assert(shell.metrics().shell_builds == 1U);
    assert(leaked <= 64 && "LVGL heap leak across 500 transitions");
    assert(heap_after.frag_pct <= 5U && "LVGL heap fragmentation > 5%");

    shell.teardown();
    lv_timer_handler();

    // Verify UiDemoBackend explicit DEMO N/OP badge & honest Scene fallback
    UiDemoBackend demo_backend;
    UiShell018A demo_shell(demo_backend);
    assert(demo_shell.begin());
    preview::g_sim_tick_ms += 20;
    demo_shell.service(preview::g_sim_tick_ms);
    lv_timer_handler();
    lv_refr_now(nullptr);
    assert(findLabelWithText(demo_shell.header(), "DEMO N/OP", false) != nullptr);
    demo_shell.selectArea(AreaId::Ritmo);
    demo_shell.selectRitmoModule(RitmoModule::Bateria);
    preview::g_sim_tick_ms += 20;
    demo_shell.service(preview::g_sim_tick_ms);
    lv_timer_handler();
    assert(findLabelWithText(demo_shell.panel(), "NAO OPERACIONAL", true) != nullptr ||
           findLabelWithText(demo_shell.panel(), "DRUM ENGINE", true) != nullptr);
    demo_shell.teardown();

    std::puts("PASS: [T7] Demo honesty, teardown safety, and 500-transition host nav/heap benchmark passed.");
    std::puts("ALL 0.18-A PROTOTYPE HOST TESTS PASSED.");
    return 0;
}
