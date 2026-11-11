#include "ui_blueprint_performance_adapter.h"
#include "local_performance_backend.h"
#include "amy_runtime.h"
#include "fake_nvs.h"
#include "user_preset_nvs_v2.h"
#include "user_preset_fx_nvs.h"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace smk::p4;
using namespace smk::p4::ui;

namespace smk::p4::ui {
const char* featureStatusLabel(FeatureStatus status) {
    switch (status) {
        case FeatureStatus::Available:   return "AVAIL";
        case FeatureStatus::Planned:     return "PLAN";
        case FeatureStatus::Unavailable: return "N/A";
        case FeatureStatus::Demo:        return "DEMO";
        default:                         return "UNK";
    }
}
}

int main() {
    std::puts("=========================================================");
    std::puts("=== SMK-P4 0.13.1: STABILITY & UX REGRESSION SUITE ===");
    std::puts("=========================================================");

    fake_nvs::blobs.clear();
    fake_nvs::free_entries = 800;

    amy::Runtime runtime;
    assert(runtime.begin());

    NvsVersionedPresets synth_store;
    assert(synth_store.begin());

    fx::NvsFxPresets fx_store;
    assert(fx_store.begin());

    LocalAmyPerformanceBackend backend(runtime.commands());
    backend.presetStore(&synth_store);
    backend.fxPresetStore(&fx_store);
    backend.publish(runtime.snapshot(), false, 0, 0);

    UiBlueprintPerformanceAdapter adapter(backend);
    uint32_t current_time_ms = 100;
    auto tick = [&](uint32_t ms = 0) {
        current_time_ms = (ms > current_time_ms + 30) ? ms : (current_time_ms + 35);
        adapter.service(current_time_ms);
        runtime.apply(current_time_ms);
        runtime.loadPendingPatch();
        runtime.render();
        backend.publish(runtime.snapshot(), false, 0, 0);
        adapter.service(current_time_ms);
        runtime.apply(current_time_ms);
        runtime.loadPendingPatch();
        runtime.render();
        backend.publish(runtime.snapshot(), false, 0, 0);
    };

    // ─────────────────────────────────────────────────────────────────────────
    // SECTION 1: TYPED REJECTION DIAGNOSTICS & USER PRESET SAFETY
    // ─────────────────────────────────────────────────────────────────────────
    std::puts("--- Scenario 1.1: Save As in Empty Slot (Clean Save) ---");
    assert(backend.setPatch(4, 10)); // Juno patch 4
    tick(20);
    runtime.loadPendingPatch();
    assert(runtime.render());
    backend.publish(runtime.snapshot(), false, 0, 0);

    assert(backend.editParameter(edit::Cutoff, 1500.0f));
    assert(backend.setFxParameter("fx.chorus.mode", 2.0f, 30));
    tick(40);

    assert(backend.editAction(2, 0, "CLEAN PRESET 01"));
    assert(backend.presetOperationStatus().state == smk::p4::PresetOperationState::WaitingForQuiet ||
           backend.presetOperationStatus().state == smk::p4::PresetOperationState::Writing);
    tick(60);
    assert(!backend.editError());
    assert(backend.presetOperationStatus().state == smk::p4::PresetOperationState::Confirmed);
    assert(std::strcmp(backend.presetOperationStatus().message, "SAVED TO SLOT 01") == 0);
    assert(std::strcmp(backend.editNotice(), "SAVED") == 0);
    std::puts("  PASS: Clean Save in empty slot confirmed asynchronously");

    std::puts("--- Scenario 1.2: Save As with Invalid Name & Limits ---");
    // Modify synth to make it unsaved
    assert(backend.editParameter(edit::Cutoff, 2000.0f));
    tick(80);

    // Empty name
    assert(!backend.editAction(2, 1, ""));
    assert(backend.lastPresetRejectReason() == PresetRejectReason::InvalidName);
    assert(std::strcmp(backend.presetRejectNotice(), "SAVE BLOCKED: INVALID PRESET NAME") == 0);

    // Null name
    assert(!backend.editAction(2, 1, nullptr));
    assert(backend.lastPresetRejectReason() == PresetRejectReason::InvalidName);

    // Name exceeding 24 chars
    assert(!backend.editAction(2, 1, "THIS NAME IS DEFINITELY LONGER THAN TWENTY FOUR CHARS"));
    assert(backend.lastPresetRejectReason() == PresetRejectReason::InvalidName);
    std::puts("  PASS: Invalid preset names rejected with specific typed diagnostics");

    std::puts("--- Scenario 1.3: Save Blocked by Active Notes & Sustain ---");
    // Hold a note
    assert(backend.noteOn(60, 100, 100));
    tick(110);
    assert(!backend.editAction(2, 1, "ACTIVE NOTE PRESET"));
    assert(backend.lastPresetRejectReason() == PresetRejectReason::ActiveNotes);
    assert(std::strcmp(backend.presetRejectNotice(), "SAVE BLOCKED: NOTES STILL ACTIVE") == 0);

    backend.noteOff(60);
    tick(120);

    std::puts("--- Scenario 1.4: Save Blocked During Compare Mode ---");
    assert(backend.editAction(0)); // Compare toggle ON
    tick(130);
    assert(std::strcmp(backend.editNotice(), "COMPARE ACTIVE") == 0);

    assert(!backend.editAction(2, 1, "COMPARE SAVE"));
    assert(backend.lastPresetRejectReason() == PresetRejectReason::CompareActive);
    assert(std::strcmp(backend.presetRejectNotice(), "OVERWRITE BLOCKED: COMPARE ACTIVE") == 0);

    assert(backend.editAction(0)); // Compare toggle OFF
    tick(140);
    std::puts("  PASS: Compare mode strictly guards against accidental saving/overwriting");

    std::puts("--- Scenario 1.5: Recall Blocked by Unsaved Synth & FX Edits ---");
    // Synth is currently unsaved (cutoff 2000 vs saved 1500)
    assert(backend.synthUnsaved());
    assert(!backend.editAction(3, 0)); // Attempt recall slot 0
    assert(backend.lastPresetRejectReason() == PresetRejectReason::UnsavedSynth);
    assert(std::strcmp(backend.presetRejectNotice(), "RECALL BLOCKED: UNSAVED SYNTH EDITS") == 0);

    // Revert synth
    assert(backend.editAction(1)); // Revert
    tick(160);
    assert(!backend.synthUnsaved());

    // Modify FX only
    assert(backend.setFxParameter("fx.delay.mix", 50.0f, 170));
    tick(180);
    assert(backend.fxUnsaved());
    assert(!backend.editAction(3, 0));
    assert(backend.lastPresetRejectReason() == PresetRejectReason::UnsavedFx);
    assert(std::strcmp(backend.presetRejectNotice(), "RECALL BLOCKED: UNSAVED FX EDITS") == 0);

    // Revert FX
    assert(backend.editAction(1));
    tick(200);
    assert(!backend.unsaved());
    assert(std::strcmp(backend.editNotice(), "FACTORY ORIGINAL") == 0);
    std::puts("  PASS: Recall strictly guarded against discarding unsaved synth or FX edits");

    std::puts("--- Scenario 1.6: Recall of Valid Slot vs Empty/Invalid Slot ---");
    // Valid slot 0 recall
    assert(backend.editAction(3, 0));
    assert(backend.presetOperationStatus().state == smk::p4::PresetOperationState::Requested);
    tick();
    tick();
    assert(backend.presetOperationStatus().state == smk::p4::PresetOperationState::Confirmed);
    assert(std::strcmp(backend.presetOperationStatus().message, "SLOT 01 RECALLED") == 0);
    assert(backend.soundEdit()->applied[edit::Cutoff] == 1500.0f);
    assert(backend.fxState()->config.chorus_mode == 2);

    // Empty slot 5 recall
    assert(!backend.editAction(3, 5));
    assert(backend.lastPresetRejectReason() == PresetRejectReason::StorageReadError);
    assert(std::strcmp(backend.presetRejectNotice(), "PRESET INVALID: SLOT 06") == 0);

    // Out of range slot 99 recall
    assert(!backend.editAction(3, 99));
    assert(backend.lastPresetRejectReason() == PresetRejectReason::InvalidSlot);
    assert(std::strcmp(backend.presetRejectNotice(), "PRESET INVALID: SLOT RANGE") == 0);
    std::puts("  PASS: Recall distinguishes valid slots, empty slots, and out-of-range slots");

    // ─────────────────────────────────────────────────────────────────────────
    // SECTION 2: ADAPTER & UI COMMAND CONFIRMATION
    // ─────────────────────────────────────────────────────────────────────────
    std::puts("--- Scenario 2.1: Adapter requestSoundAction Diagnostic Routing ---");
    // Attempting recall when unsaved via adapter
    assert(adapter.requestParameterFloat("filter.cutoff_hz", 3500.0f).accepted());
    tick(240);
    const auto bad_recall = adapter.requestSoundAction(3, 0);
    assert(bad_recall.state == UiCommandState::Rejected);
    assert(std::strcmp(bad_recall.notice, "RECALL BLOCKED: UNSAVED SYNTH EDITS") == 0);

    // Revert via adapter
    assert(adapter.requestSoundAction(1).accepted());
    tick(260);
    assert(!backend.unsaved());

    // Clean recall via adapter
    const auto good_recall = adapter.requestSoundAction(3, 0);
    assert(good_recall.accepted());
    tick();
    tick();
    assert(adapter.presetOperationStatus().state == smk::p4::ui::PresetOperationState::Confirmed);
    assert(std::strcmp(adapter.presetOperationStatus().message, "SLOT 01 RECALLED") == 0);
    std::puts("  PASS: Adapter correctly maps typed notices and tracks asynchronous states");

    // ─────────────────────────────────────────────────────────────────────────
    // SECTION 3: ROBUST FM ALGORITHM OWNER CONFIRMATION
    // ─────────────────────────────────────────────────────────────────────────
    std::puts("--- Scenario 3.1: Normal Algorithm Change & Owner Acknowledgment ---");
    assert(backend.setPatch(135, 300)); // DX7 patch 135
    tick(310);
    runtime.loadPendingPatch();
    assert(runtime.render());
    backend.publish(runtime.snapshot(), false, 0, 0);

    const unsigned initial_algo = unsigned(std::lround(backend.soundEdit()->applied[edit::FmAlgorithm]));
    assert(initial_algo >= 1 && initial_algo <= 32);

    // Request new Algorithm
    const unsigned target_algo = (initial_algo == 16) ? 22 : 16;
    const auto algo_req = adapter.requestParameterInt("fm.algo", target_algo);
    assert(algo_req.accepted());
    assert(algo_req.state == UiCommandState::Requested);
    assert(algo_req.token != 0);
    assert(std::strcmp(algo_req.notice, "ALGORITHM REQUESTED; WAIT OWNER") == 0);

    // Before engine processes, applied is still initial_algo
    assert(backend.soundEdit()->applied[edit::FmAlgorithm] == float(initial_algo));

    tick();
    // After engine applies, owner confirmation arrives
    assert(adapter.lastCommandState() == UiCommandState::Confirmed);
    assert(std::strcmp(adapter.lastCommandParameterId(), "fm.algo") == 0);
    assert(adapter.lastCommandToken() == algo_req.token);
    assert(std::strcmp(adapter.model().lastNotice(), "ALGORITHM APPLIED") == 0);
    assert(backend.soundEdit()->applied[edit::FmAlgorithm] == float(target_algo));
    std::puts("  PASS: Algorithm request correlated to token and applied snapshot");

    std::puts("--- Scenario 3.2: Rejection of Algorithm Change During Active Notes ---");
    assert(backend.noteOn(60, 100, 350));
    tick(360);

    // Live operator edits ARE allowed
    assert(backend.editParameter(edit::OpLevel0, 75.0f));
    tick(370);
    assert(backend.soundEdit()->applied[edit::OpLevel0] == 75.0f);

    // Re-sending active algorithm IS allowed
    assert(backend.editParameter(edit::FmAlgorithm, float(target_algo)));
    tick(380);
    assert(backend.soundEdit()->applied[edit::FmAlgorithm] == float(target_algo));

    // Changing topology to a different algorithm is REJECTED
    const unsigned blocked_algo = (target_algo == 22) ? 14 : 22;
    const auto note_blocked = adapter.requestParameterInt("fm.algo", blocked_algo);
    assert(note_blocked.state == UiCommandState::Rejected);
    assert(std::strcmp(note_blocked.notice, "RELEASE NOTES TO CHANGE FM ALGORITHM") == 0);
    assert(backend.soundEdit()->applied[edit::FmAlgorithm] == float(target_algo)); // Intact

    backend.noteOff(60);
    tick();
    assert(backend.editAction(1)); // Revert edits from Scenario 3.2
    tick();
    assert(!backend.unsaved());
    std::puts("  PASS: Live operator edits permitted; topology change guarded during notes");

    std::puts("--- Scenario 3.3: Concurrent Patch Change Cancels Pending Algorithm ---");
    // Request Algo 24
    const auto algo24_req = adapter.requestParameterInt("fm.algo", 24);
    assert(algo24_req.accepted());

    // Concurrent patch change (e.g. MIDI Program Change on runtime)
    assert(runtime.commands().push({amy::CommandType::Patch, 128, 0, 0, 0, 400}));
    runtime.apply(400);
    runtime.loadPendingPatch();
    assert(runtime.render());
    backend.publish(runtime.snapshot(), false, 0, 0);

    adapter.service(410);
    assert(adapter.lastCommandState() == UiCommandState::Rejected);
    assert(std::strcmp(adapter.lastCommandParameterId(), "fm.algo") == 0);
    assert(std::strcmp(adapter.model().lastNotice(), "PATCH CHANGED; ALGORITHM CANCELED") == 0);
    std::puts("  PASS: Concurrent patch change cleanly supersedes pending algorithm request");

    // ─────────────────────────────────────────────────────────────────────────
    // SECTION 4: COMPARE, REVERT & FEEDBACK HARDENING
    // ─────────────────────────────────────────────────────────────────────────
    std::puts("--- Scenario 4.1: Visual Feedback Matrix (Compare, Synth, FX, Both) ---");
    assert(backend.setPatch(4, 450));
    tick(460);
    runtime.loadPendingPatch();
    assert(runtime.render());
    backend.publish(runtime.snapshot(), false, 0, 0);

    assert(std::strcmp(backend.editNotice(), "FACTORY ORIGINAL") == 0);

    // 1. Synth dirty only
    assert(backend.editParameter(edit::Cutoff, 2500.0f));
    tick(470);
    assert(std::strcmp(backend.editNotice(), "EDITED / UNSAVED") == 0);

    // 2. Synth + FX dirty
    assert(backend.setFxParameter("fx.chorus.mode", 1.0f, 480));
    tick(490);
    assert(std::strcmp(backend.editNotice(), "SYNTH + FX UNSAVED") == 0);

    // 3. Compare active
    assert(backend.editAction(0));
    tick(500);
    assert(std::strcmp(backend.editNotice(), "COMPARE ACTIVE") == 0);

    // Compare off -> restores SYNTH + FX UNSAVED
    assert(backend.editAction(0));
    tick(510);
    assert(std::strcmp(backend.editNotice(), "SYNTH + FX UNSAVED") == 0);

    // Revert restores FACTORY ORIGINAL
    assert(backend.editAction(1));
    tick(520);
    assert(std::strcmp(backend.editNotice(), "FACTORY ORIGINAL") == 0);
    std::puts("  PASS: Visual feedback clearly distinguishes all unsaved combinations and Compare");

    std::puts("=========================================================");
    std::puts("=== ALL 0.13.1 STABILITY & UX REGRESSIONS PASSED (PASS) ===");
    std::puts("=========================================================");

    runtime.end();
    return 0;
}
