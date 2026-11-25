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

void clickBtn(lv_obj_t* root, const char* text, UiBlueprint& app, bool prefix = false) {
    lv_obj_t* btn = findButton(root, text, prefix);
    if (!btn) {
        std::printf("MISSING BUTTON %s\n", text);
        std::fflush(stdout);
    }
    assert(btn);
    lv_obj_send_event(btn, LV_EVENT_CLICKED, nullptr);
    preview::pumpFrame(app, 25);
}

bool isButtonHighlighted(lv_obj_t* root, const char* text, uint32_t expected_hex) {
    lv_obj_t* btn = findButton(root, text, false);
    if (!btn) return false;
    return lv_color_eq(lv_obj_get_style_bg_color(btn, LV_PART_MAIN), lv_color_hex(expected_hex));
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
    const char* out_dir = (argc >= 2) ? argv[1] : "smk-p4/docs/ui_018c3a_shots";

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
    auto request = [&](seq::Action action, int value = 0) {
        seq::Request r{};
        r.action = action;
        r.value = int16_t(value);
        assert(adapter.requestSequencer(r).accepted());
        sync_runtime();
    };
    auto sendMidiNote = [&](uint8_t note, uint8_t velocity) {
        const uint8_t on[]{0x09, 0x90, note, velocity};
        const uint8_t off[]{0x08, 0x80, note, 0};
        assert(runtime.usbInput().transfer(on, sizeof(on), preview::g_sim_tick_ms));
        sync_runtime();
        assert(runtime.usbInput().transfer(off, sizeof(off), preview::g_sim_tick_ms));
        sync_runtime();
    };

    sync_runtime();

    // Seed two valid slots so Pattern Switch and Storage Dialogs can be tested cleanly.
    seq::Slot slot_a{};
    slot_a.state = seq::SlotState::Valid;
    slot_a.settings.pattern = *adapter.sequencerPattern();
    slot_a.settings.pattern.length = seq::kSteps;
    slot_a.settings.pattern.steps[0] = {60, 100, 50, true};
    std::snprintf(slot_a.name, sizeof(slot_a.name), "HOST GROOVE");
    assert(patterns.commit(0, slot_a));

    seq::Slot slot_b = slot_a;
    slot_b.settings.pattern.steps[0] = {64, 110, 60, true};
    std::snprintf(slot_b.name, sizeof(slot_b.name), "ALT GROOVE");
    assert(patterns.commit(1, slot_b));
    adapter.setPatternStore(patterns);

    // ── 1. P0: Pattern Switch Mode persistence across navigation ─────────────
    assert(nav.navigateTo(ScreenId::Patterns));
    sync_runtime();
    assert(findLabel(root, "IMMEDIATE"));
    capture("01_patterns_immediate");

    click("SWITCH MODE");
    assert(findLabel(root, "NEXT BAR / 4/4"));
    assert(nav.patternSwitchMode() == seq::SwitchMode::NextBar);
    capture("02_patterns_next_bar");

    // Navigate across FX, TIMBRES, TOCAR, MAIS and return to PATTERNS.
    assert(nav.navigateTo(ScreenId::Effects));
    sync_runtime();
    assert(nav.navigateTo(ScreenId::PatchBrowser));
    sync_runtime();
    assert(nav.navigateTo(ScreenId::Performance));
    sync_runtime();
    assert(nav.navigateTo(ScreenId::MidiSettings));
    sync_runtime();
    assert(nav.navigateTo(ScreenId::Patterns));
    sync_runtime();
    assert(findLabel(root, "NEXT BAR / 4/4"));
    assert(nav.patternSwitchMode() == seq::SwitchMode::NextBar);
    capture("03_patterns_next_bar_preserved");

    // Load slot 01 first cleanly, then test NEXT BAR with stopped and running transport.
    click("01 HOST", true);
    click("LOAD");
    for (unsigned i = 0; i < 4; ++i) sync_runtime();
    assert(adapter.sequencerStatus()->active_slot == 0);
    assert(adapter.sequencerStatus()->pending_slot == seq::kNoSlot);
    assert(findLabel(root, "NEXT BAR / 4/4"));

    // Start transport, schedule NEXT BAR load of slot 02, then switch visual pref to IMMEDIATE.
    request(seq::Action::Play);
    assert(adapter.sequencerStatus()->playing);
    click("02 ALT", true);
    click("LOAD");
    sync_runtime();
    assert(adapter.sequencerStatus()->pending_slot == 1);
    click("SWITCH MODE");
    assert(findLabel(root, "IMMEDIATE"));
    assert(nav.patternSwitchMode() == seq::SwitchMode::Immediate);
    // Visual preference change must NOT cancel or retroactively convert pending NEXT BAR switch.
    for (unsigned i = 0; i < 3; ++i) sync_runtime();
    assert(adapter.sequencerStatus()->pending_slot == 1);
    assert(adapter.sequencerStatus()->active_slot == 0);
    capture("04_next_bar_pending_pref_immediate");

    // Cancel pending switch explicitly.
    click("CANCEL QUEUE");
    for (unsigned i = 0; i < 3; ++i) sync_runtime();
    assert(adapter.sequencerStatus()->pending_slot == seq::kNoSlot);
    assert(adapter.sequencerStatus()->active_slot == 0);

    // Repeat with transport stopped: NEXT BAR applies immediately at stop without changing pref.
    click("SWITCH MODE");
    assert(findLabel(root, "NEXT BAR / 4/4"));
    request(seq::Action::Stop);
    for (unsigned i = 0; i < 4; ++i) sync_runtime();
    assert(!adapter.sequencerStatus()->playing);
    click("02 ALT", true);
    click("LOAD");
    for (unsigned i = 0; i < 4; ++i) sync_runtime();
    assert(adapter.sequencerStatus()->active_slot == 1);
    assert(nav.patternSwitchMode() == seq::SwitchMode::NextBar);

    // ── 2. P1: Patterns REJECTED capture (must not stay stuck on REQUESTED) ─
    click("02 ALT", true);
    click("SAVE");
    clickBtn(app.rootObject(), "CONFIRM", app);
    for (unsigned i = 0; i < 12; ++i) sync_runtime();
    assert(std::strstr(adapter.patternNotice(), "CONFIRMED") != nullptr);
    PatternOperation invalid{};
    invalid.slot = seq::kPatternSlots;
    adapter.requestPattern(invalid);
    sync_runtime();
    assert(findLabel(root, "PATTERN CLEAN   REJECTED", true));
    capture("10_patterns_rejected");

    // ── 3. P1: Storage dialogs (SAVE NEW vs OVERWRITE vs DELETE vs SAVE&LOAD)
    click("03 EMPTY", true);
    click("SAVE");
    assert(findLabel(app.rootObject(), "SAVE NEW PATTERN - SLOT 03", true));
    assert(findLabel(app.rootObject(), "Storage quiet silences audio and stops transport.", true));
    capture("11_save_new_pattern");
    clickBtn(app.rootObject(), "CANCEL", app);
    sync_runtime();

    click("01 HOST", true);
    click("SAVE");
    assert(findLabel(app.rootObject(), "OVERWRITE PATTERN - SLOT 01", true));
    capture("12_overwrite_pattern");
    clickBtn(app.rootObject(), "CANCEL", app);
    sync_runtime();

    click("01 HOST", true);
    click("DELETE");
    assert(findLabel(app.rootObject(), "DELETE PATTERN - SLOT 01", true));
    clickBtn(app.rootObject(), "CANCEL", app);
    sync_runtime();

    // ── 4. P0: Separate Visible Page, Selected Step, and Record Cursor ──────
    assert(nav.navigateTo(ScreenId::Sequencer));
    sync_runtime();
    click("EDIT MODE");
    assert(nav.sequencerPage() == 0);
    capture("05_seq_edit_page_1_8");

    // Activate STEP RECORD and verify confirmed button highlight (no optimistic highlight).
    click("MIDI / REC");
    click("STEP RECORD");
    assert(adapter.sequencerStatus()->midi.mode == seq::InputMode::StepRecord);
    assert(isButtonHighlighted(root, "STEP RECORD", palette::kAccentPrimary));
    assert(!isButtonHighlighted(root, "NORMAL PLAY", palette::kActive));
    capture("08_step_record_confirmed");
    click("CLOSE");

    // Explicitly select step 3 (index 2).
    click("03", true);
    assert(adapter.sequencerStatus()->midi.selected == 2);
    const auto subtab_before = nav.subTab(ScreenId::Sequencer);
    const auto pattern_before = *adapter.sequencerPattern();
    const auto gen_before = runtime.snapshot().generation;

    // Switch visual page to 9-16: must NOT send SelectStep, move cursor, or mutate pattern.
    click("1-8 / 9-16");
    assert(nav.sequencerPage() == 1);
    assert(findLabel(root, "09", true));
    assert(adapter.sequencerStatus()->midi.selected == 2);
    assert(nav.subTab(ScreenId::Sequencer) == subtab_before);
    assert(*adapter.sequencerPattern() == pattern_before);
    assert(runtime.snapshot().generation == gen_before);
    capture("06_seq_edit_page_9_16_cursor_preserved");

    // Send USB MIDI note while viewing page 9-16 -> must record at step 3 and advance to step 4.
    sendMidiNote(67, 105);
    assert(adapter.sequencerPattern()->steps[2].note == 67);
    assert(adapter.sequencerPattern()->steps[2].enabled);
    assert(adapter.sequencerStatus()->midi.selected == 3);
    assert(nav.sequencerPage() == 1);
    assert(findLabel(root, "STEP RECORD  REC 04 (OFF-PAGE 1-8)", true));
    capture("07_step_record_off_page_cursor");

    // Return to page 1-8 and verify no unintended edits occurred on page 9-16.
    click("1-8 / 9-16");
    assert(nav.sequencerPage() == 0);
    assert(adapter.sequencerPattern()->steps[2].note == 67);
    for (unsigned s = 8; s < seq::kSteps; ++s) {
        assert(adapter.sequencerPattern()->steps[s] == pattern_before.steps[s]);
    }

    // Test wrap in STEP RECORD across length=4 while viewing page 9-16.
    request(seq::Action::SetLength, 4);
    click("1-8 / 9-16");
    assert(nav.sequencerPage() == 1);
    sendMidiNote(69, 100); // records step 4 (index 3), wraps to step 1 (index 0)
    assert(adapter.sequencerPattern()->steps[3].note == 69);
    assert(adapter.sequencerStatus()->midi.selected == 0);
    assert(nav.sequencerPage() == 1);
    request(seq::Action::SetLength, 16);
    click("1-8 / 9-16");

    // Test STEP INPUT mode with off-page viewing.
    click("MIDI / REC");
    click("STEP INPUT");
    assert(adapter.sequencerStatus()->midi.mode == seq::InputMode::StepInput);
    assert(isButtonHighlighted(root, "STEP INPUT", palette::kActive));
    capture("09_step_input_confirmed");
    click("CLOSE");

    click("02", true);
    assert(adapter.sequencerStatus()->midi.selected == 1);
    click("1-8 / 9-16");
    assert(nav.sequencerPage() == 1);
    sendMidiNote(72, 110);
    assert(adapter.sequencerPattern()->steps[1].note == 72);
    assert(adapter.sequencerStatus()->midi.selected == 1);
    assert(nav.sequencerPage() == 1);

    // Repeat with NORMAL PLAY, running transport, and Live Transpose.
    request(seq::Action::SetInputMode, 0);
    request(seq::Action::SetLiveChannel, 0);
    request(seq::Action::SetLive, 1);
    request(seq::Action::Play);
    assert(adapter.sequencerStatus()->playing);
    click("1-8 / 9-16");
    assert(nav.sequencerPage() == 0);
    click("1-8 / 9-16");
    assert(nav.sequencerPage() == 1);
    assert(adapter.sequencerStatus()->playing);

    // ── 5. P1: Arp RESTART label check ──────────────────────────────────────
    assert(adapter.requestParameterBool("arp.enable", true).accepted());
    sync_runtime();
    assert(adapter.requestArpTransport(true).accepted());
    sync_runtime();
    assert(nav.navigateTo(ScreenId::Arpeggiator));
    sync_runtime();
    assert(findLabel(root, "RESTART"));
    assert(!findLabel(root, "RESTART ON NEW CHORD"));
    capture("13_arp_restart_label");

    // Hold a note in Arp (latched, then key released) while Sequencer is also running for simultaneous stress.
    request(seq::Action::SetLive, 0);
    assert(adapter.requestParameterBool("arp.latch", true).accepted());
    sync_runtime();
    const uint8_t arp_on[]{0x09, 0x90, 60, 100};
    const uint8_t arp_off[]{0x08, 0x80, 60, 0};
    assert(runtime.usbInput().transfer(arp_on, sizeof(arp_on), preview::g_sim_tick_ms));
    for (unsigned i = 0; i < 16; ++i) sync_runtime();
    assert(runtime.usbInput().transfer(arp_off, sizeof(arp_off), preview::g_sim_tick_ms));
    sync_runtime();
    assert(runtime.snapshot().arp.length > 0 && adapter.sequencerStatus()->playing);

    // ── 6. Stress & LVGL resource budget verification ───────────────────────
    assert(nav.navigateTo(ScreenId::Performance));
    sync_runtime();
    const auto objects_before = countObjects(lv_screen_active());
    lv_mem_monitor_t initial{};
    lv_mem_monitor(&initial);
    const auto gen_stress = runtime.snapshot().generation;
    const auto cmd_stress = adapter.sequencerStatus()->command.token;
    const auto arp_phase = runtime.snapshot().arp.tempo.phase;

    std::vector<double> times;
    times.reserve(500);
    for (unsigned i = 0; i < 500; ++i) {
        const auto start = std::chrono::steady_clock::now();
        assert(nav.navigateTo(ScreenId::Sequencer));
        nav.setSequencerEdit(i % 2 != 0);
        assert(nav.navigateTo(ScreenId::Sequencer));
        if (nav.sequencerEdit()) {
            clickBtn(root, "1-8 / 9-16", app);
            clickBtn(root, "1-8 / 9-16", app);
        }
        if (i < 200) {
            assert(nav.navigateTo(ScreenId::Patterns));
            if (i < 100) {
                clickBtn(root, "SWITCH MODE", app);
                clickBtn(root, "SAVE", app);
                clickBtn(app.rootObject(), "CANCEL", app);
            }
            assert(nav.navigateTo(ScreenId::Arpeggiator));
        }
        assert(nav.navigateTo(ScreenId::Effects));
        assert(nav.navigateTo(ScreenId::Performance));
        sync_runtime(1);
        times.push_back(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count());
    }

    assert(adapter.sequencerStatus()->playing);
    assert(runtime.snapshot().arp.length > 0);
    assert(runtime.snapshot().generation == gen_stress);
    assert(adapter.sequencerStatus()->command.token == cmd_stress);
    assert(runtime.snapshot().arp.tempo.phase > arp_phase);

    // Verify 100 Step Record cycles while Sequencer + Arp are running.
    assert(nav.navigateTo(ScreenId::Sequencer));
    sync_runtime();
    request(seq::Action::SetLive, 0);
    request(seq::Action::SetInputMode, int(seq::InputMode::StepRecord));
    for (unsigned i = 0; i < 100; ++i) {
        const uint8_t note = uint8_t(60 + (i % 12));
        const auto cur_sel = adapter.sequencerStatus()->midi.selected;
        sendMidiNote(note, 100);
        assert(adapter.sequencerPattern()->steps[cur_sel].note == note);
        assert(adapter.sequencerStatus()->midi.selected == (cur_sel + 1) % adapter.sequencerPattern()->length);
    }

    assert(nav.navigateTo(ScreenId::Performance));
    sync_runtime();
    assert(adapter.sequencerStatus()->playing);
    assert(runtime.snapshot().arp.length > 0);
    assert(runtime.snapshot().generation == gen_stress);

    lv_mem_monitor_t final_mem{};
    lv_mem_monitor(&final_mem);
    const auto objects_after = countObjects(lv_screen_active());
    const auto stats = computeLatency(times);
    std::printf("C3A STRESS objs=%u->%u free=%zu->%zu largest=%zu->%zu peak=%zu frag=%u->%u median=%.1f p95=%.1f max=%.1f us\n",
                objects_before, objects_after, initial.free_size, final_mem.free_size,
                initial.free_biggest_size, final_mem.free_biggest_size, final_mem.max_used,
                initial.frag_pct, final_mem.frag_pct, stats.median_us, stats.p95_us, stats.max_us);
    assert(objects_before == objects_after);
    assert(initial.free_size == final_mem.free_size);
    assert(final_mem.max_used <= 128u * 1024u);

    nav.shutdown();
    std::puts("PASS C3a rhythm UX hardening / host simulation");
    return 0;
}
