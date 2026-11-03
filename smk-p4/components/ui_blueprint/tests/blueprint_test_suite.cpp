#include "preview_harness.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace smk::p4::ui;
using namespace smk::p4::ui::preview;

namespace {

// Recursively verifies every visible LVGL object is strictly inside 800x480
// and interactive buttons meet minimum touch target size.
void verifyObjectTreeBounds(lv_obj_t* obj, ScreenId screen, uint8_t tab, unsigned& widget_count, bool& found_demo_badge) {
    if (!obj || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    ++widget_count;

    lv_area_t coords{};
    lv_obj_get_coords(obj, &coords);
    if (coords.x1 < 0 || coords.y1 < 0 || coords.x2 >= kScreenWidth || coords.y2 >= kScreenHeight) {
        std::fprintf(stderr,
                     "LAYOUT OVERFLOW on screen %u tab %u: coords=(%d,%d)..(%d,%d)\n",
                     unsigned(screen), unsigned(tab),
                     int(coords.x1), int(coords.y1), int(coords.x2), int(coords.y2));
        assert(false && "Widget outside 800x480 screen bounds");
    }

    if (lv_obj_check_type(obj, &lv_label_class)) {
        const char* txt = lv_label_get_text(obj);
        if (txt && std::strcmp(txt, "UI DEMO") == 0) {
            found_demo_badge = true;
        }
    }

    if (lv_obj_check_type(obj, &lv_button_class)) {
        const int32_t bw = coords.x2 - coords.x1 + 1;
        const int32_t bh = coords.y2 - coords.y1 + 1;
        // Minimum touch target check (2-octave black keys are >= 28px wide; all action buttons >= 34x30)
        assert(bw >= 24 && bh >= 28 && "Touch button below minimum usable dimensions");
    }

    const uint32_t child_cnt = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < child_cnt; ++i) {
        verifyObjectTreeBounds(lv_obj_get_child(obj, i), screen, tab, widget_count, found_demo_badge);
    }
}

lv_obj_t* findButtonWithLabel(lv_obj_t* root, const char* label_prefix) {
    if (!root || lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN)) return nullptr;
    if (lv_obj_check_type(root, &lv_button_class)) {
        const uint32_t cnt = lv_obj_get_child_count(root);
        for (uint32_t i = 0; i < cnt; ++i) {
            lv_obj_t* ch = lv_obj_get_child(root, i);
            if (lv_obj_check_type(ch, &lv_label_class)) {
                const char* t = lv_label_get_text(ch);
                if (t && std::strncmp(t, label_prefix, std::strlen(label_prefix)) == 0) {
                    return root;
                }
            }
        }
    }
    const uint32_t cnt = lv_obj_get_child_count(root);
    for (uint32_t i = 0; i < cnt; ++i) {
        if (lv_obj_t* found = findButtonWithLabel(lv_obj_get_child(root, i), label_prefix)) {
            return found;
        }
    }
    return nullptr;
}

lv_obj_t* findFirstWidgetOfType(lv_obj_t* root, const lv_obj_class_t* cls) {
    if (!root || lv_obj_has_flag(root, LV_OBJ_FLAG_HIDDEN)) return nullptr;
    if (lv_obj_check_type(root, cls) && !lv_obj_has_state(root, LV_STATE_DISABLED)) {
        return root;
    }
    const uint32_t cnt = lv_obj_get_child_count(root);
    for (uint32_t i = 0; i < cnt; ++i) {
        if (lv_obj_t* found = findFirstWidgetOfType(lv_obj_get_child(root, i), cls)) {
            return found;
        }
    }
    return nullptr;
}

void testModel() {
    assert(!ParameterRegistry::hasDuplicateIds());
    size_t count = 0;
    const ParameterDescriptor* all = ParameterRegistry::all(count);
    assert(count >= 75 && count <= UiModel::kMaxParameters);

    for (size_t i = 0; i < count; ++i) {
        assert(all[i].id && all[i].id[0] != '\0');
        assert(all[i].ns && all[i].ns[0] != '\0');
        assert(all[i].name && all[i].name[0] != '\0');
        assert(all[i].evidence_ref && all[i].evidence_ref[0] != '\0');
        assert(all[i].description && all[i].description[0] != '\0');
        assert(all[i].minimum <= all[i].default_value);
        assert(all[i].default_value <= all[i].maximum);
    }

    UiModel model;
    // Clamping test
    assert(model.setFloat("filter.cutoff_hz", 999999.0f));
    assert(std::fabs(model.getFloat("filter.cutoff_hz") - 18000.0f) < 0.01f);
    assert(model.setFloat("filter.cutoff_hz", -50.0f));
    assert(std::fabs(model.getFloat("filter.cutoff_hz") - 20.0f) < 0.01f);

    // Type mismatch rejection
    assert(!model.setValue("filter.cutoff_hz", ParameterValue::makeBool(true)));

    // Enum bounds check
    assert(model.setValue("perf.layer_mode", ParameterValue::makeEnum(3)));
    assert(!model.setValue("perf.layer_mode", ParameterValue::makeEnum(99)));
    assert(!model.setValue("perf.layer_mode", ParameterValue::makeEnum(-1)));

    // Read-only parameter protection (sample rate must never be changed by mock)
    assert(!model.setInt("sys.sample_rate_hz", 44100));
    assert(model.getInt("sys.sample_rate_hz") == 48000);

    // Dependency & Synthesis Family applicability test
    // Default patch 128 is FM -> fm.algo is Planned, osc.1.pulse_width is Unavailable
    assert(model.effectiveStatus("fm.algo") == FeatureStatus::Planned);
    assert(model.effectiveStatus("osc.1.pulse_width") == FeatureStatus::Unavailable);
    assert(model.effectiveStatus("osc.sub_level") == FeatureStatus::Unavailable);

    // Select Subtractive patch 0 (JUNO BRASS SET 1) -> fm.* becomes Unavailable, osc.sub_level becomes Planned
    assert(model.selectPatchById(0, 0));
    assert(model.effectiveStatus("fm.algo") == FeatureStatus::Unavailable);
    assert(model.effectiveStatus("osc.sub_level") == FeatureStatus::Planned);

    // Set osc.1.wave to Pulse (1) -> osc.1.pulse_width becomes Planned
    assert(model.setInt("osc.1.wave", 1));
    assert(model.effectiveStatus("osc.1.pulse_width") == FeatureStatus::Planned);

    std::puts("PASS: [1/5] ParameterRegistry & UiModel bounds, types, read-only guard, dependencies and family matrix");
}

void testNavigation(UiBlueprint& app) {
    auto& nav = app.navigation();
    assert(nav.currentScreen() == ScreenId::Performance);

    // Visit every single screen 01..14
    for (uint8_t s = 0; s < static_cast<uint8_t>(ScreenId::Count); ++s) {
        const auto sid = static_cast<ScreenId>(s);
        assert(nav.navigateTo(sid));
        pumpFrame(app);
        assert(nav.currentScreen() == sid);
        assert(nav.activeView() != nullptr);
    }

    // Verify back() unwinds navigation history
    const ScreenId before_back = nav.currentScreen();
    assert(before_back == ScreenId::Diagnostics);
    assert(nav.back());
    pumpFrame(app);
    assert(nav.currentScreen() == ScreenId::GlobalSettings);

    // Verify 1-tap returnToPerformance()
    assert(nav.returnToPerformance());
    pumpFrame(app);
    assert(nav.currentScreen() == ScreenId::Performance);
    assert(nav.historyDepth() == 0);

    // Verify bottom nav bank cycling and clicking nav bar buttons
    for (int b = 0; b < 3; ++b) {
        lv_obj_t* bank_btn = findButtonWithLabel(app.rootObject(), "BANK:");
        assert(bank_btn != nullptr);
        lv_obj_send_event(bank_btn, LV_EVENT_CLICKED, nullptr);
        pumpFrame(app);
    }

    std::puts("PASS: [2/5] UiNavigationManager all 14 screens, history back-stack, bank cycling and returnToPerformance");
}

void testInteractions(UiBlueprint& app) {
    auto& backend = app.backend();
    auto& nav = app.navigation();

    // 1. Performance slider & button interactions
    assert(nav.returnToPerformance());
    pumpFrame(app);
    lv_obj_t* slider = findFirstWidgetOfType(app.contentObject(), &lv_slider_class);
    assert(slider != nullptr);
    lv_slider_set_value(slider, 80, LV_ANIM_OFF);
    lv_obj_send_event(slider, LV_EVENT_VALUE_CHANGED, nullptr);
    pumpFrame(app);

    // Compact view toggle on Performance
    assert(nav.navigateTo(ScreenId::Performance, 1));
    pumpFrame(app);
    lv_obj_t* arc = findFirstWidgetOfType(app.contentObject(), &lv_arc_class);
    assert(arc != nullptr);
    lv_arc_set_value(arc, 75);
    lv_obj_send_event(arc, LV_EVENT_VALUE_CHANGED, nullptr);
    pumpFrame(app);

    // 2. Patch Browser interactions
    assert(nav.navigateTo(ScreenId::PatchBrowser));
    pumpFrame(app);
    lv_obj_t* fav_btn = findButtonWithLabel(app.contentObject(), "FAVORITE:");
    assert(fav_btn != nullptr);
    const uint16_t cur_patch = backend.model().activePatch().id;
    const bool fav_before = backend.model().isFavorite(cur_patch);
    lv_obj_send_event(fav_btn, LV_EVENT_CLICKED, nullptr);
    pumpFrame(app);
    assert(backend.model().isFavorite(cur_patch) == !fav_before);

    // 3. Sequencer step toggle & transport
    assert(nav.navigateTo(ScreenId::Sequencer));
    pumpFrame(app);
    const bool step0_before = backend.model().seqStep(0).active;
    backend.model().toggleSeqStep(0);
    assert(backend.model().seqStep(0).active == !step0_before);
    lv_obj_t* play_btn = findButtonWithLabel(app.contentObject(), "PLAY DEMO SEQ");
    assert(play_btn != nullptr);
    lv_obj_send_event(play_btn, LV_EVENT_CLICKED, nullptr);
    pumpFrame(app, 200);
    assert(backend.model().getBool("seq.playing", false) == true);

    // 4. Harmony Scale Quantizer & Chord Assist calculation verification
    backend.model().setBool("harmony.scale_enable", true);
    backend.model().setInt("harmony.root_note", 0);  // C
    backend.model().setInt("harmony.scale_type", 2); // Natural Minor (C, D, Eb, F, G, Ab, Bb)
    backend.model().setBool("harmony.chord_enable", true);
    backend.model().setInt("harmony.chord_type", 2); // Minor triad (0, 3, 7)
    backend.model().setInt("harmony.inversion", 0);
    backend.model().setInt("harmony.spread", 0);

    // Input C#4 (61) is out of C Natural Minor -> quantizes to D4 (62), Minor triad = 62, 65, 69
    const HarmonyPreviewResult harm = backend.model().computeHarmony(61);
    assert(harm.is_demo_calculation);
    assert(!harm.input_in_scale);
    assert(harm.quantized_note == 62);
    assert(harm.chord_count == 3);
    assert(harm.chord_notes[0] == 62 && harm.chord_notes[1] == 65 && harm.chord_notes[2] == 69);

    // 5. MIDI Learn demo interaction & Global Panic cleanup
    assert(nav.navigateTo(ScreenId::MidiSettings, 1));
    pumpFrame(app);
    lv_obj_t* learn_btn = findButtonWithLabel(app.contentObject(), "LEARN");
    assert(learn_btn != nullptr);
    lv_obj_send_event(learn_btn, LV_EVENT_CLICKED, nullptr);
    pumpFrame(app);
    assert(backend.model().midiLearnSlot() >= 0);

    backend.triggerNoteOn(60, 100);
    assert(backend.model().diagnostics().active_voices == 1);

    lv_obj_t* panic_btn = findButtonWithLabel(app.rootObject(), "PANIC");
    assert(panic_btn != nullptr);
    lv_obj_send_event(panic_btn, LV_EVENT_CLICKED, nullptr);
    pumpFrame(app);
    assert(backend.model().diagnostics().active_voices == 0);
    assert(backend.model().getBool("seq.playing", false) == false);
    assert(backend.model().midiLearnSlot() == -1);

    std::puts("PASS: [3/5] Interactive widgets, Patch Browser, Sequencer, Scale/Chord Assist and Panic verified");
}

void testLayoutAndMemory(UiBlueprint& app) {
    auto& nav = app.navigation();

    // 1. Verify layout bounds across all 14 screens and all sub-tabs
    for (uint8_t s = 0; s < static_cast<uint8_t>(ScreenId::Count); ++s) {
        const auto sid = static_cast<ScreenId>(s);
        const uint8_t max_tabs = (sid == ScreenId::SoundEditor) ? 7
                               : (sid == ScreenId::Performance || sid == ScreenId::Modulation ||
                                  sid == ScreenId::Harmony || sid == ScreenId::MidiSettings) ? 2 : 1;
        for (uint8_t t = 0; t < max_tabs; ++t) {
            assert(nav.navigateTo(sid, t));
            pumpFrame(app);
            unsigned widget_count = 0;
            bool found_demo_badge = false;
            verifyObjectTreeBounds(app.rootObject(), sid, t, widget_count, found_demo_badge);
            assert(widget_count > 10);
            assert(found_demo_badge && "Persistent UI DEMO indicator missing");
        }
    }
    std::puts("PASS: [4/5] All 14 screens and sub-tabs verified strictly within 800x480 with persistent UI DEMO badge");

    // 2. Stress test memory across 50 full navigation cycles (700+ screen mounts/unmounts)
    // Warm-up pass so TLSF block splitting & LVGL internal caches reach steady state
    for (int warmup = 0; warmup < 2; ++warmup) {
        for (uint8_t s = 0; s < static_cast<uint8_t>(ScreenId::Count); ++s) {
            assert(nav.navigateTo(static_cast<ScreenId>(s)));
            pumpFrame(app, 5);
        }
    }
    assert(nav.returnToPerformance());
    pumpFrame(app, 5);
    lv_mem_monitor_t baseline{};
    lv_mem_monitor(&baseline);

    for (int cycle = 0; cycle < 50; ++cycle) {
        for (uint8_t s = 0; s < static_cast<uint8_t>(ScreenId::Count); ++s) {
            assert(nav.navigateTo(static_cast<ScreenId>(s)));
            pumpFrame(app, 5);
        }
    }
    assert(nav.returnToPerformance());
    pumpFrame(app, 5);

    lv_mem_monitor_t after{};
    lv_mem_monitor(&after);
    std::printf("LVGL 9.3.0 MEMORY REPORT (50 full 14-screen cycles = 700 screen mounts):\n"
                "  baseline_free=%zu  after_free=%zu  largest_block=%zu  peak_used=%zu  frag=%u%%\n",
                baseline.free_size, after.free_size, after.free_biggest_size,
                after.max_used, unsigned(after.frag_pct));
    std::fflush(stdout);

    assert(after.free_size == baseline.free_size && "LVGL memory leak detected across screen navigation cycles");
    assert(after.frag_pct <= 50 && "Excessive LVGL heap fragmentation");
    std::puts("PASS: [5/5] Zero LVGL memory leaks (0 bytes delta) across 700 screen creation/destruction transitions");
}

} // namespace

int main() {
    initHostLvgl();
    testModel();

    UiDemoBackend backend;
    UiBlueprint app(backend);
    assert(app.begin());
    pumpFrame(app);

    testNavigation(app);
    testInteractions(app);
    testLayoutAndMemory(app);

    app.shutdown();
    lv_mem_monitor_t final_mem{};
    lv_mem_monitor(&final_mem);
    std::printf("ALL UI BLUEPRINT TESTS PASSED (post-shutdown free=%zu bytes, frag=%u%%)\n",
                final_mem.free_size, unsigned(final_mem.frag_pct));
    return 0;
}
