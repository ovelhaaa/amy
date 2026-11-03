#include "ui_demo_backend.h"
#include "lvgl.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace smk::p4::ui {

UiDemoBackend::UiDemoBackend() {
    recomputeMeters();
}

void UiDemoBackend::service(uint32_t now_ms) {
    auto& diag = model_.diagnostics();
    diag.uptime_ms = now_ms;

    // Query LVGL 9.3.0 memory monitor when available
    lv_mem_monitor_t mem{};
    lv_mem_monitor(&mem);
    diag.lvgl_free_bytes = static_cast<uint32_t>(mem.free_size);
    diag.lvgl_largest_block = static_cast<uint32_t>(mem.free_biggest_size);
    diag.lvgl_peak_used = static_cast<uint32_t>(mem.max_used);
    diag.lvgl_frag_pct = mem.frag_pct;

    // Advance demo sequencer step visually when seq.playing or arp.enable is true
    if (model_.getBool("seq.playing", false) || model_.getBool("arp.enable", false)) {
        const float bpm = std::clamp(model_.getFloat("perf.bpm", 120.0f), 40.0f, 240.0f);
        const uint32_t step_interval_ms = static_cast<uint32_t>(15000.0f / bpm);
        if (now_ms - last_step_ms_ >= step_interval_ms) {
            last_step_ms_ = now_ms;
            const int32_t len = std::clamp<int32_t>(model_.getInt("seq.length", 16), 1, 16);
            active_demo_step_ = static_cast<uint8_t>((active_demo_step_ + 1) % len);
            model_.bumpRevision();
        }
    }
    recomputeMeters();
}

void UiDemoBackend::triggerNoteOn(uint8_t note, uint8_t velocity) {
    if (note >= 128) return;
    if (velocity == 0) {
        triggerNoteOff(note);
        return;
    }
    held_notes_[note] = true;
    model_.setLastPreviewNote(note);

    uint8_t count = 0;
    for (bool h : held_notes_) {
        if (h) ++count;
    }
    model_.diagnostics().active_voices = std::min<uint8_t>(count, model_.diagnostics().max_voices);

    const HarmonyPreviewResult harm = model_.computeHarmony(note);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "NOTE %u -> QUANT %u (%u NOTES)",
                  note, harm.quantized_note, harm.chord_count);
    model_.setNotice(buf);
    recomputeMeters();
}

void UiDemoBackend::triggerNoteOff(uint8_t note) {
    if (note >= 128) return;
    held_notes_[note] = false;
    uint8_t count = 0;
    for (bool h : held_notes_) {
        if (h) ++count;
    }
    model_.diagnostics().active_voices = std::min<uint8_t>(count, model_.diagnostics().max_voices);
    recomputeMeters();
    model_.bumpRevision();
}

void UiDemoBackend::triggerPanic() {
    std::memset(held_notes_, 0, sizeof(held_notes_));
    model_.diagnostics().active_voices = 0;
    ++model_.diagnostics().demo_panic_count;
    model_.setBool("seq.playing", false);
    model_.setBool("arp.latch", false);
    model_.setMidiLearnSlot(-1);
    recomputeMeters();
    model_.setNotice("DEMO PANIC: VOICES/LATCH CLEARED IN UI MODEL (NO AMY TX)");
}

void UiDemoBackend::recomputeMeters() {
    meters_.is_simulated = true;
    const bool a_mute = model_.getBool("mixer.layer_a.mute", false);
    const bool b_mute = model_.getBool("mixer.layer_b.mute", false);
    const bool d_mute = model_.getBool("mixer.drums.mute", false);

    const int32_t a_lvl = a_mute ? 0 : model_.getInt("mixer.layer_a.level", 85);
    const int32_t b_lvl = b_mute ? 0 : model_.getInt("mixer.layer_b.level", 75);
    const int32_t d_lvl = d_mute ? 0 : model_.getInt("mixer.drums.level", 80);
    const int32_t m_lvl = model_.getInt("perf.master_volume", 64);

    // Static proportional preview levels scaled by configured mixer faders
    const int32_t activity_boost = (model_.diagnostics().active_voices > 0) ? 20 : 0;
    meters_.layer_a_l = static_cast<uint8_t>(std::clamp<int32_t>((a_lvl * 65) / 100 + activity_boost, 0, 100));
    meters_.layer_a_r = static_cast<uint8_t>(std::clamp<int32_t>((a_lvl * 60) / 100 + activity_boost, 0, 100));
    meters_.layer_b_l = static_cast<uint8_t>(std::clamp<int32_t>((b_lvl * 55) / 100 + activity_boost / 2, 0, 100));
    meters_.layer_b_r = static_cast<uint8_t>(std::clamp<int32_t>((b_lvl * 58) / 100 + activity_boost / 2, 0, 100));
    meters_.drums_l   = static_cast<uint8_t>(std::clamp<int32_t>((d_lvl * 50) / 100, 0, 100));
    meters_.drums_r   = static_cast<uint8_t>(std::clamp<int32_t>((d_lvl * 50) / 100, 0, 100));

    const int32_t sum_l = (meters_.layer_a_l + meters_.layer_b_l + meters_.drums_l) / 3;
    const int32_t sum_r = (meters_.layer_a_r + meters_.layer_b_r + meters_.drums_r) / 3;
    meters_.master_l = static_cast<uint8_t>(std::clamp<int32_t>((sum_l * m_lvl) / 96, 0, 100));
    meters_.master_r = static_cast<uint8_t>(std::clamp<int32_t>((sum_r * m_lvl) / 96, 0, 100));
}

} // namespace smk::p4::ui
