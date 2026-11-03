#include "ui_demo_backend.h"
#include "lvgl.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace smk::p4::ui {

const char* commandStateLabel(UiCommandState state) {
    switch (state) {
        case UiCommandState::Confirmed:   return "CONFIRMED";
        case UiCommandState::Requested:   return "REQUESTED";
        case UiCommandState::DemoApplied: return "DEMO RAM";
        case UiCommandState::Unsupported: return "PLANNED";
        case UiCommandState::Rejected:    return "REJECTED";
        default:                          return "---";
    }
}

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

bool UiDemoBackend::isPatchSelectable(uint16_t patch_id) const {
    size_t count = 0;
    const BlueprintPatchEntry* cat = UiModel::catalog(count);
    for (size_t i = 0; i < count; ++i) {
        if (cat[i].id == patch_id) return true;
    }
    return false;
}

UiCommandResult UiDemoBackend::requestParameterValue(const char* id, const ParameterValue& val) {
    if (!id) {
        last_cmd_state_ = UiCommandState::Rejected;
        last_cmd_param_ = "";
        return {UiCommandState::Rejected, 0, "INVALID PARAMETER ID"};
    }
    const ParameterDescriptor* desc = ParameterRegistry::findById(id);
    if (!desc) {
        last_cmd_state_ = UiCommandState::Rejected;
        last_cmd_param_ = id;
        return {UiCommandState::Rejected, 0, "UNKNOWN PARAMETER ID"};
    }
    last_cmd_param_ = desc->id;
    if (!model_.setValue(id, val)) {
        last_cmd_state_ = UiCommandState::Rejected;
        return {UiCommandState::Rejected, 0, model_.lastNotice()};
    }
    last_cmd_state_ = UiCommandState::DemoApplied;
    recomputeMeters();
    return {UiCommandState::DemoApplied, 0, model_.lastNotice()};
}

UiCommandResult UiDemoBackend::requestParameterFloat(const char* id, float val) {
    if (!id) return {UiCommandState::Rejected, 0, "INVALID PARAMETER ID"};
    const ParameterDescriptor* desc = ParameterRegistry::findById(id);
    if (!desc) return {UiCommandState::Rejected, 0, "UNKNOWN PARAMETER ID"};
    last_cmd_param_ = desc->id;
    if (!model_.setFloat(id, val)) {
        last_cmd_state_ = UiCommandState::Rejected;
        return {UiCommandState::Rejected, 0, model_.lastNotice()};
    }
    last_cmd_state_ = UiCommandState::DemoApplied;
    recomputeMeters();
    return {UiCommandState::DemoApplied, 0, model_.lastNotice()};
}

UiCommandResult UiDemoBackend::requestParameterInt(const char* id, int32_t val) {
    if (!id) return {UiCommandState::Rejected, 0, "INVALID PARAMETER ID"};
    const ParameterDescriptor* desc = ParameterRegistry::findById(id);
    if (!desc) return {UiCommandState::Rejected, 0, "UNKNOWN PARAMETER ID"};
    last_cmd_param_ = desc->id;
    if (!model_.setInt(id, val)) {
        last_cmd_state_ = UiCommandState::Rejected;
        return {UiCommandState::Rejected, 0, model_.lastNotice()};
    }
    last_cmd_state_ = UiCommandState::DemoApplied;
    recomputeMeters();
    return {UiCommandState::DemoApplied, 0, model_.lastNotice()};
}

UiCommandResult UiDemoBackend::requestParameterBool(const char* id, bool val) {
    if (!id) return {UiCommandState::Rejected, 0, "INVALID PARAMETER ID"};
    const ParameterDescriptor* desc = ParameterRegistry::findById(id);
    if (!desc) return {UiCommandState::Rejected, 0, "UNKNOWN PARAMETER ID"};
    last_cmd_param_ = desc->id;
    if (!model_.setBool(id, val)) {
        last_cmd_state_ = UiCommandState::Rejected;
        return {UiCommandState::Rejected, 0, model_.lastNotice()};
    }
    last_cmd_state_ = UiCommandState::DemoApplied;
    recomputeMeters();
    return {UiCommandState::DemoApplied, 0, model_.lastNotice()};
}

UiCommandResult UiDemoBackend::requestSelectPatch(uint16_t patch_id, uint8_t target_layer) {
    last_cmd_param_ = "perf.patch_id";
    if (!model_.selectPatchById(patch_id, target_layer)) {
        last_cmd_state_ = UiCommandState::Rejected;
        return {UiCommandState::Rejected, 0, "PATCH NOT FOUND"};
    }
    last_cmd_state_ = UiCommandState::DemoApplied;
    recomputeMeters();
    return {UiCommandState::DemoApplied, 0, model_.lastNotice()};
}

UiCommandResult UiDemoBackend::requestNoteOn(uint8_t note, uint8_t velocity) {
    if (note >= 128) {
        return {UiCommandState::Rejected, 0, "NOTE OUT OF RANGE"};
    }
    if (velocity == 0) {
        return requestNoteOff(note, 0);
    }

    const uint32_t token = next_token_++;
    if (next_token_ == 0) next_token_ = 1;

    held_tokens_[note] = token;
    ++note_on_count_;
    recordNoteEvent(true, note, velocity, token);

    model_.setLastPreviewNote(note);
    updateActiveVoiceCount();

    const HarmonyPreviewResult harm = model_.computeHarmony(note);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "NOTE %u -> QUANT %u (%u NOTES)",
                  note, harm.quantized_note, harm.chord_count);
    model_.setNotice(buf);
    recomputeMeters();
    last_cmd_state_ = UiCommandState::DemoApplied;
    return {UiCommandState::DemoApplied, token, model_.lastNotice()};
}

UiCommandResult UiDemoBackend::requestNoteOff(uint8_t note, uint32_t token) {
    if (note >= 128) {
        return {UiCommandState::Rejected, 0, "NOTE OUT OF RANGE"};
    }
    const uint32_t active_token = held_tokens_[note];
    if (active_token == 0) {
        return {UiCommandState::Rejected, 0, "NOTE NOT ACTIVE"};
    }
    // Prevent releasing a note with a stale token from a prior interaction
    if (token != 0 && token != active_token) {
        return {UiCommandState::Rejected, 0, "TOKEN MISMATCH"};
    }

    held_tokens_[note] = 0;
    ++note_off_count_;
    recordNoteEvent(false, note, 0, active_token);

    updateActiveVoiceCount();
    recomputeMeters();
    model_.bumpRevision();
    last_cmd_state_ = UiCommandState::DemoApplied;
    return {UiCommandState::DemoApplied, active_token, model_.lastNotice()};
}

void UiDemoBackend::releaseAllUiNotes() {
    bool released_any = false;
    for (uint8_t n = 0; n < 128; ++n) {
        if (held_tokens_[n] != 0) {
            const uint32_t tok = held_tokens_[n];
            held_tokens_[n] = 0;
            ++note_off_count_;
            recordNoteEvent(false, n, 0, tok);
            released_any = true;
        }
    }
    if (released_any) {
        updateActiveVoiceCount();
        recomputeMeters();
        model_.bumpRevision();
    }
}

UiCommandResult UiDemoBackend::requestPanic() {
    releaseAllUiNotes();
    std::memset(held_tokens_, 0, sizeof(held_tokens_));
    model_.diagnostics().active_voices = 0;
    ++model_.diagnostics().demo_panic_count;
    model_.setBool("seq.playing", false);
    model_.setBool("arp.latch", false);
    model_.setMidiLearnSlot(-1);
    recomputeMeters();
    model_.setNotice("DEMO PANIC: VOICES/LATCH CLEARED IN UI MODEL (NO AMY TX)");
    last_cmd_state_ = UiCommandState::DemoApplied;
    return {UiCommandState::DemoApplied, 0, model_.lastNotice()};
}

void UiDemoBackend::updateActiveVoiceCount() {
    uint8_t count = 0;
    for (uint32_t tok : held_tokens_) {
        if (tok != 0) ++count;
    }
    model_.diagnostics().active_voices = std::min<uint8_t>(count, model_.diagnostics().max_voices);
}

void UiDemoBackend::recordNoteEvent(bool is_on, uint8_t note, uint8_t velocity, uint32_t token) {
    if (event_log_count_ < kEventLogCapacity) {
        event_log_[event_log_count_++] = NoteEventRecord{is_on, note, velocity, token};
    } else {
        for (size_t i = 1; i < kEventLogCapacity; ++i) {
            event_log_[i - 1] = event_log_[i];
        }
        event_log_[kEventLogCapacity - 1] = NoteEventRecord{is_on, note, velocity, token};
    }
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
