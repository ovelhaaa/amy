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
void verifyObjectTreeBounds(lv_obj_t* obj, ScreenId screen, uint8_t tab,
                            unsigned& widget_count, bool& found_demo_badge,
                            unsigned& badge_count) {
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
        if (txt) {
            if (std::strcmp(txt, "UI DEMO") == 0) {
                found_demo_badge = true;
            }
            if (std::strcmp(txt, "AVAIL") == 0 || std::strcmp(txt, "PLAN") == 0 ||
                std::strcmp(txt, "N/A") == 0 || std::strcmp(txt, "DEMO") == 0) {
                ++badge_count;
            }
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
        verifyObjectTreeBounds(lv_obj_get_child(obj, i), screen, tab,
                               widget_count, found_demo_badge, badge_count);
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

// Mock adapter simulating the real PerformanceBackend on feature/smk-p4-standalone-hardware-validation:
// - Only patches 128 and 135 are selectable (setPatch)
// - Master volume (perf.master_volume) and UI octave shift (perf.octave_shift) are supported
// - Macros (setMacro) and planned synth parameters are rejected with UiCommandState::Unsupported
class MockPerformanceBackendAdapter final : public IUiBackend {
public:
    MockPerformanceBackendAdapter() {
        model_.setNotice("P4 LIVE MOCK READY");
    }

    UiModel& model() override { return model_; }
    const UiModel& model() const override { return model_; }

    bool isDemoMode() const override { return false; }
    bool isConnected() const override { return connected_; }
    bool isBusy() const override { return busy_; }
    bool hasMidiError() const override { return midi_error_; }
    const char* backendName() const override { return "MockPerformanceBackendAdapter"; }
    const char* modeBadgeText() const override { return "P4 LIVE"; }
    const char* statusText() const override { return model_.lastNotice(); }

    FeatureStatus parameterStatus(const char* id) const override {
        return model_.effectiveStatus(id);
    }
    bool isParameterEditable(const char* id) const override {
        return model_.isParameterEnabled(id);
    }
    bool isPatchSelectable(uint16_t patch_id) const override {
        return patch_id == 128 || patch_id == 135;
    }
    UiCommandState lastCommandState() const override { return last_state_; }
    const char* lastCommandParameterId() const override { return last_param_; }

    UiCommandResult requestParameterValue(const char* id, const ParameterValue& val) override {
        if (!id) return {UiCommandState::Rejected, 0, "NULL PARAM"};
        last_param_ = id;
        if (!connected_ || busy_) {
            last_state_ = UiCommandState::Rejected;
            model_.setNotice("BACKEND BUSY OR DISCONNECTED");
            return {UiCommandState::Rejected, 0, model_.lastNotice()};
        }
        // Only perf.master_volume and perf.octave_shift are supported in standalone P4 0.3
        if (std::strcmp(id, "perf.master_volume") == 0 ||
            std::strcmp(id, "perf.octave_shift") == 0) {
            if (!model_.setValue(id, val)) {
                last_state_ = UiCommandState::Rejected;
                return {UiCommandState::Rejected, 0, "INVALID VALUE"};
            }
            if (std::strcmp(id, "perf.master_volume") == 0) {
                ++set_master_calls;
            }
            last_state_ = UiCommandState::Confirmed;
            model_.setNotice("APPLIED TO P4 BACKEND");
            return {UiCommandState::Confirmed, 0, model_.lastNotice()};
        }
        // All other parameters (including setMacro) are planned in 0.3
        ++unsupported_calls;
        last_state_ = UiCommandState::Unsupported;
        model_.setNotice("PLANNED: NOT APPLIED TO AMY");
        return {UiCommandState::Unsupported, 0, model_.lastNotice()};
    }

    UiCommandResult requestParameterFloat(const char* id, float val) override {
        const ParameterDescriptor* d = ParameterRegistry::findById(id);
        if (!d) return {UiCommandState::Rejected, 0, "UNKNOWN PARAM"};
        if (d->type == ParameterValueType::Int) {
            return requestParameterValue(id, ParameterValue::makeInt(static_cast<int32_t>(std::lround(val))));
        }
        return requestParameterValue(id, ParameterValue::makeFloat(val));
    }

    UiCommandResult requestParameterInt(const char* id, int32_t val) override {
        const ParameterDescriptor* d = ParameterRegistry::findById(id);
        if (!d) return {UiCommandState::Rejected, 0, "UNKNOWN PARAM"};
        if (d->type == ParameterValueType::Enum) {
            return requestParameterValue(id, ParameterValue::makeEnum(val));
        }
        return requestParameterValue(id, ParameterValue::makeInt(val));
    }

    UiCommandResult requestParameterBool(const char* id, bool val) override {
        return requestParameterValue(id, ParameterValue::makeBool(val));
    }

    UiCommandResult requestSelectPatch(uint16_t patch_id, uint8_t target_layer) override {
        last_param_ = "perf.patch_id";
        if (target_layer != 0 || !isPatchSelectable(patch_id)) {
            ++unsupported_calls;
            last_state_ = UiCommandState::Unsupported;
            model_.setNotice("PATCH NOT MATERIALIZED IN P4 ROM");
            return {UiCommandState::Unsupported, 0, model_.lastNotice()};
        }
        releaseAllUiNotes();
        model_.selectPatchById(patch_id, 0);
        ++set_patch_calls;
        last_state_ = UiCommandState::Confirmed;
        model_.setNotice("P4 ROM PATCH LOADED");
        return {UiCommandState::Confirmed, 0, model_.lastNotice()};
    }

    UiCommandResult requestNoteOn(uint8_t note, uint8_t velocity) override {
        if (note >= 128 || !connected_ || busy_) {
            last_state_ = UiCommandState::Rejected;
            return {UiCommandState::Rejected, 0, "NOTE REJECTED"};
        }
        if (velocity == 0) return requestNoteOff(note, 0);
        const uint32_t tok = next_token_++;
        held_tokens_[note] = tok;
        ++note_on_calls;
        last_note_on = note;
        syncVoices();
        last_state_ = UiCommandState::Confirmed;
        return {UiCommandState::Confirmed, tok, "NOTE ON"};
    }

    UiCommandResult requestNoteOff(uint8_t note, uint32_t token) override {
        if (note >= 128 || held_tokens_[note] == 0) {
            return {UiCommandState::Rejected, 0, "NOT HELD"};
        }
        if (token != 0 && held_tokens_[note] != token) {
            return {UiCommandState::Rejected, 0, "TOKEN MISMATCH"};
        }
        held_tokens_[note] = 0;
        ++note_off_calls;
        last_note_off = note;
        syncVoices();
        model_.bumpRevision();
        last_state_ = UiCommandState::Confirmed;
        return {UiCommandState::Confirmed, token, "NOTE OFF"};
    }

    void releaseAllUiNotes() override {
        for (uint8_t n = 0; n < 128; ++n) {
            if (held_tokens_[n] != 0) {
                held_tokens_[n] = 0;
                ++note_off_calls;
                last_note_off = n;
            }
        }
        syncVoices();
    }

    UiCommandResult requestPanic() override {
        releaseAllUiNotes();
        ++panic_calls;
        last_state_ = UiCommandState::Confirmed;
        model_.setNotice("P4 PANIC EXECUTED");
        return {UiCommandState::Confirmed, 0, model_.lastNotice()};
    }

    void service(uint32_t now_ms) override {
        model_.diagnostics().uptime_ms = now_ms;
    }

    const UiSignalMeters& meters() const override { return meters_; }
    uint8_t activeSequencerStep() const override { return 0; }
    bool isNoteHeld(uint8_t note) const override {
        return (note < 128) && (held_tokens_[note] != 0);
    }

    uint32_t set_patch_calls   = 0;
    uint32_t set_master_calls  = 0;
    uint32_t unsupported_calls = 0;
    uint32_t note_on_calls     = 0;
    uint32_t note_off_calls    = 0;
    uint32_t panic_calls       = 0;
    uint8_t  last_note_on      = 0;
    uint8_t  last_note_off     = 0;

private:
    void syncVoices() {
        uint8_t c = 0;
        for (uint32_t t : held_tokens_) {
            if (t != 0) ++c;
        }
        model_.diagnostics().active_voices = c;
    }

    UiModel        model_{};
    UiSignalMeters meters_{};
    uint32_t       held_tokens_[128]{};
    uint32_t       next_token_ = 1;
    bool           connected_  = true;
    bool           busy_       = false;
    bool           midi_error_ = false;
    UiCommandState last_state_ = UiCommandState::Confirmed;
    const char*    last_param_ = "";
};

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

    // Verify improved macro labels (Section 7)
    assert(std::strcmp(ParameterRegistry::findById("perf.macro.2")->name, "M2 BRIGHT") == 0);
    assert(std::strcmp(ParameterRegistry::findById("perf.macro.3")->name, "M3 MOTION") == 0);
    assert(std::strcmp(ParameterRegistry::findById("perf.macro.4")->name, "M4 SHAPE") == 0);
    assert(std::strcmp(ParameterRegistry::findById("perf.macro.7")->name, "M7 SPACE") == 0);
    assert(std::strcmp(ParameterRegistry::findById("perf.macro.8")->name, "M8 DRIVE") == 0);

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

    // Restore default patch 128
    assert(model.selectPatchById(128, 0));

    std::puts("PASS: [1/7] ParameterRegistry & UiModel bounds, types, read-only guard, labels and family matrix");
}

void testNavigation(UiBlueprint& app) {
    auto& nav = app.navigation();
    assert(nav.currentScreen() == ScreenId::Performance);
    assert(nav.currentPresentationLevel() == PresentationLevel::StageMinimal);

    // Visit every single screen 01..14
    for (uint8_t s = 0; s < static_cast<uint8_t>(ScreenId::Count); ++s) {
        const auto sid = static_cast<ScreenId>(s);
        assert(nav.navigateTo(sid));
        pumpFrame(app);
        assert(nav.currentScreen() == sid);
        assert(nav.activeView() != nullptr);
        if (sid == ScreenId::Performance) {
            assert(nav.currentPresentationLevel() == PresentationLevel::StageMinimal);
        } else {
            assert(nav.currentPresentationLevel() == PresentationLevel::BlueprintVerbose);
        }
    }

    // Verify back() unwinds navigation history
    assert(nav.currentScreen() == ScreenId::Diagnostics);
    assert(nav.back());
    pumpFrame(app);
    assert(nav.currentScreen() == ScreenId::GlobalSettings);

    // Verify deferred navigation (requestNavigateTo / requestReturnToPerformance)
    nav.requestNavigateTo(ScreenId::Effects);
    assert(nav.hasPendingNavigation());
    assert(nav.currentScreen() == ScreenId::GlobalSettings); // Not yet switched inside callback
    pumpFrame(app); // UiBlueprint::service processes pending navigation safely
    assert(!nav.hasPendingNavigation());
    assert(nav.currentScreen() == ScreenId::Effects);

    nav.requestReturnToPerformance();
    assert(nav.hasPendingNavigation());
    pumpFrame(app);
    assert(!nav.hasPendingNavigation());
    assert(nav.currentScreen() == ScreenId::Performance);
    assert(nav.historyDepth() == 0);

    // Verify bottom nav bank cycling and clicking nav bar buttons
    for (int b = 0; b < 3; ++b) {
        lv_obj_t* bank_btn = findButtonWithLabel(app.rootObject(), "BANK:");
        assert(bank_btn != nullptr);
        lv_obj_send_event(bank_btn, LV_EVENT_CLICKED, nullptr);
        pumpFrame(app);
    }

    std::puts("PASS: [2/7] UiNavigationManager all 14 screens, deferred navigation, history back-stack and bank cycling");
}

void testVirtualKeyboardLifecycle(UiBlueprint& app, UiDemoBackend& demo) {
    auto& nav = app.navigation();
    assert(nav.returnToPerformance());
    assert(nav.navigateTo(ScreenId::Performance, 0)); // MACRO FOCUS (1-octave keyboard C4..C5)
    pumpFrame(app);

    // Reset octave to 0 and clear event log
    demo.requestParameterInt("perf.octave_shift", 0);
    nav.refreshActive();
    pumpFrame(app);
    demo.clearEventLog();

    lv_obj_t* key_c4 = findButtonWithLabel(app.contentObject(), "C4");
    lv_obj_t* key_g4 = findButtonWithLabel(app.contentObject(), "G4");
    assert(key_c4 != nullptr);
    assert(key_g4 != nullptr);

    // 1. Standard tap cycle: PRESSED -> RELEASED -> CLICKED
    // Must emit exactly ONE Note On (60) and ONE Note Off (60); CLICKED must NOT emit a second Note On!
    lv_obj_send_event(key_c4, LV_EVENT_PRESSED, nullptr);
    assert(demo.noteOnCount() == 1);
    assert(demo.noteOffCount() == 0);
    assert(demo.isNoteHeld(60));
    assert(demo.model().diagnostics().active_voices == 1);

    // Sending duplicate PRESSED while already held must be ignored
    lv_obj_send_event(key_c4, LV_EVENT_PRESSED, nullptr);
    assert(demo.noteOnCount() == 1);

    lv_obj_send_event(key_c4, LV_EVENT_RELEASED, nullptr);
    lv_obj_send_event(key_c4, LV_EVENT_CLICKED, nullptr);
    assert(demo.noteOnCount() == 1);
    assert(demo.noteOffCount() == 1);
    assert(!demo.isNoteHeld(60));
    assert(demo.model().diagnostics().active_voices == 0);
    assert(demo.eventLogCount() == 2);
    assert(demo.eventLogAt(0).is_note_on && demo.eventLogAt(0).note == 60);
    assert(!demo.eventLogAt(1).is_note_on && demo.eventLogAt(1).note == 60);
    assert(demo.eventLogAt(0).token == demo.eventLogAt(1).token);

    // 2. Touch lost / finger dragged outside key: PRESSED -> PRESS_LOST -> RELEASED
    demo.clearEventLog();
    lv_obj_send_event(key_c4, LV_EVENT_PRESSED, nullptr);
    assert(demo.noteOnCount() == 1 && demo.isNoteHeld(60));
    lv_obj_send_event(key_c4, LV_EVENT_PRESS_LOST, nullptr);
    assert(demo.noteOnCount() == 1 && demo.noteOffCount() == 1 && !demo.isNoteHeld(60));
    // Subsequent RELEASED after PRESS_LOST must NOT emit a duplicate Note Off
    lv_obj_send_event(key_c4, LV_EVENT_RELEASED, nullptr);
    assert(demo.noteOffCount() == 1);

    // 3. Octave shift while key is held (Section 5 P0):
    // Press C4 (60), tap OCT + while C4 is still held, then release the key.
    // The release MUST send Note Off for 60 (original pitch), NOT 72!
    demo.clearEventLog();
    lv_obj_send_event(key_c4, LV_EVENT_PRESSED, nullptr);
    assert(demo.noteOnCount() == 1 && demo.isNoteHeld(60));

    lv_obj_t* oct_up = findButtonWithLabel(app.contentObject(), "OCT +");
    assert(oct_up != nullptr);
    lv_obj_send_event(oct_up, LV_EVENT_CLICKED, nullptr);
    pumpFrame(app);
    assert(demo.model().getInt("perf.octave_shift", 0) == 1);
    // Key C4 widget is still alive (no destructive rebuild on OCT +) and note 60 is still held
    assert(demo.isNoteHeld(60));

    // Now release the held key -> must release note 60, not 72!
    lv_obj_send_event(key_c4, LV_EVENT_RELEASED, nullptr);
    assert(demo.noteOffCount() == 1);
    assert(!demo.isNoteHeld(60));
    assert(demo.eventLogAt(1).note == 60);

    // Now press the same first key again at Octave +1 -> must trigger C5 (72)!
    lv_obj_send_event(key_c4, LV_EVENT_PRESSED, nullptr);
    assert(demo.isNoteHeld(72));
    lv_obj_send_event(key_c4, LV_EVENT_RELEASED, nullptr);
    assert(!demo.isNoteHeld(72));

    // Restore octave to 0
    lv_obj_t* oct_dn = findButtonWithLabel(app.contentObject(), "OCT -");
    assert(oct_dn != nullptr);
    lv_obj_send_event(oct_dn, LV_EVENT_CLICKED, nullptr);
    pumpFrame(app);
    assert(demo.model().getInt("perf.octave_shift", 0) == 0);

    // 4. Rapid repeated presses & 2-note chord order verification
    demo.clearEventLog();
    lv_obj_send_event(key_c4, LV_EVENT_PRESSED, nullptr);
    lv_obj_send_event(key_g4, LV_EVENT_PRESSED, nullptr);
    assert(demo.model().diagnostics().active_voices == 2);
    lv_obj_send_event(key_c4, LV_EVENT_RELEASED, nullptr);
    assert(demo.model().diagnostics().active_voices == 1);
    lv_obj_send_event(key_g4, LV_EVENT_RELEASED, nullptr);
    assert(demo.model().diagnostics().active_voices == 0);
    assert(demo.noteOnCount() == 2 && demo.noteOffCount() == 2);
    assert(demo.eventLogAt(0).is_note_on && demo.eventLogAt(0).note == 60);
    assert(demo.eventLogAt(1).is_note_on && demo.eventLogAt(1).note == 67);
    assert(!demo.eventLogAt(2).is_note_on && demo.eventLogAt(2).note == 60);
    assert(!demo.eventLogAt(3).is_note_on && demo.eventLogAt(3).note == 67);

    // 5. Panic while key is held: must clear active voices and visual state, and subsequent release must be a no-op
    demo.clearEventLog();
    lv_obj_send_event(key_c4, LV_EVENT_PRESSED, nullptr);
    assert(demo.isNoteHeld(60));
    lv_obj_t* panic_btn = findButtonWithLabel(app.rootObject(), "PANIC");
    assert(panic_btn != nullptr);
    lv_obj_send_event(panic_btn, LV_EVENT_CLICKED, nullptr);
    pumpFrame(app);
    assert(!demo.isNoteHeld(60));
    assert(demo.model().diagnostics().active_voices == 0);
    assert(demo.noteOffCount() == 1);
    lv_obj_send_event(key_c4, LV_EVENT_RELEASED, nullptr);
    assert(demo.noteOffCount() == 1); // No duplicate Note Off after Panic!

    // 6. Screen switch while key is held: must release active note cleanly on teardown
    demo.clearEventLog();
    lv_obj_send_event(key_c4, LV_EVENT_PRESSED, nullptr);
    assert(demo.isNoteHeld(60));
    assert(nav.navigateTo(ScreenId::SoundEditor));
    pumpFrame(app);
    assert(!demo.isNoteHeld(60));
    assert(demo.model().diagnostics().active_voices == 0);
    assert(demo.noteOnCount() == 1 && demo.noteOffCount() == 1);

    // 7. Performance Macro Bank 1-4 / 5-8 toggle & Macro/Keys mode switch
    assert(nav.returnToPerformance());
    pumpFrame(app);
    lv_obj_t* macro_bank_btn = findButtonWithLabel(app.contentObject(), "BANK: MACROS");
    assert(macro_bank_btn != nullptr);
    lv_obj_send_event(macro_bank_btn, LV_EVENT_CLICKED, nullptr);
    pumpFrame(app);
    // Toggle back to Macros 1-4
    macro_bank_btn = findButtonWithLabel(app.contentObject(), "BANK: MACROS");
    assert(macro_bank_btn != nullptr);
    lv_obj_send_event(macro_bank_btn, LV_EVENT_CLICKED, nullptr);
    pumpFrame(app);

    lv_obj_t* mode_btn = findButtonWithLabel(app.contentObject(), "KEYS FOCUS >");
    assert(mode_btn != nullptr);
    lv_obj_send_event(mode_btn, LV_EVENT_CLICKED, nullptr);
    pumpFrame(app);
    assert(nav.subTab(ScreenId::Performance) == 1);

    // 8. Harmony screen key press: must update in-place WITHOUT calling navigateTo or destroying the screen!
    assert(nav.navigateTo(ScreenId::Harmony, 0));
    pumpFrame(app);
    demo.clearEventLog();
    const uint32_t transitions_before = nav.transitionCount();
    ScreenView* view_before = nav.activeView();
    lv_obj_t* harm_c3 = findButtonWithLabel(app.contentObject(), "C3");
    assert(harm_c3 != nullptr);
    lv_obj_send_event(harm_c3, LV_EVENT_PRESSED, nullptr);
    lv_obj_send_event(harm_c3, LV_EVENT_RELEASED, nullptr);
    lv_obj_send_event(harm_c3, LV_EVENT_CLICKED, nullptr);
    pumpFrame(app);
    assert(nav.transitionCount() == transitions_before && "Harmony key press must not trigger navigateTo rebuild");
    assert(nav.activeView() == view_before && "Harmony ScreenView must remain alive during key press");
    assert(demo.noteOnCount() == 1 && demo.noteOffCount() == 1);

    std::puts("PASS: [3/7] VirtualKeyboard lifecycle (PRESSED/RELEASED/PRESS_LOST/CLICKED, octave-held safety, Panic, teardown, Harmony in-place)");
}

void testMockPerformanceBackendContract() {
    MockPerformanceBackendAdapter mock;
    UiBlueprint live_app(mock);
    assert(live_app.begin());
    pumpFrame(live_app);

    // 1. Master Volume slider is supported -> Confirmed
    assert(mock.requestParameterInt("perf.master_volume", 96).state == UiCommandState::Confirmed);
    assert(mock.model().getInt("perf.master_volume") == 96);
    assert(mock.set_master_calls == 1);

    // 2. Macro slider is planned in P4 0.3 -> Unsupported, model value remains unchanged, LVGL slider reverts
    const int32_t m1_before = mock.model().getInt("perf.macro.1");
    lv_obj_t* m1_slider = findFirstWidgetOfType(live_app.contentObject(), &lv_slider_class);
    assert(m1_slider != nullptr);
    lv_slider_set_value(m1_slider, 95, LV_ANIM_OFF);
    lv_obj_send_event(m1_slider, LV_EVENT_VALUE_CHANGED, nullptr);
    pumpFrame(live_app);
    assert(mock.lastCommandState() == UiCommandState::Unsupported);
    assert(mock.model().getInt("perf.macro.1") == m1_before);
    assert(mock.unsupported_calls >= 1);

    // 3. Patch selection: 135 is selectable (Confirmed), 0 is unsupported (Unsupported)
    assert(mock.requestSelectPatch(135, 0).state == UiCommandState::Confirmed);
    assert(mock.model().activePatch().id == 135);
    assert(mock.requestSelectPatch(0, 0).state == UiCommandState::Unsupported);
    assert(mock.model().activePatch().id == 135);

    // 4. NEXT > button on Performance skips unsupported patches and selects 128
    lv_obj_t* next_btn = findButtonWithLabel(live_app.contentObject(), "NEXT >");
    assert(next_btn != nullptr);
    lv_obj_send_event(next_btn, LV_EVENT_CLICKED, nullptr);
    pumpFrame(live_app);
    assert(mock.model().activePatch().id == 128);

    // 5. Note On / Note Off / Panic through IUiBackend
    lv_obj_t* key_c4 = findButtonWithLabel(live_app.contentObject(), "C4");
    assert(key_c4 != nullptr);
    lv_obj_send_event(key_c4, LV_EVENT_PRESSED, nullptr);
    assert(mock.note_on_calls == 1 && mock.last_note_on == 60);
    lv_obj_send_event(key_c4, LV_EVENT_RELEASED, nullptr);
    assert(mock.note_off_calls == 1 && mock.last_note_off == 60);

    lv_obj_t* panic_btn = findButtonWithLabel(live_app.rootObject(), "PANIC");
    assert(panic_btn != nullptr);
    lv_obj_send_event(panic_btn, LV_EVENT_CLICKED, nullptr);
    assert(mock.panic_calls == 1);

    live_app.shutdown();
    std::puts("PASS: [4/7] IUiBackend contract & MockPerformanceBackendAdapter (Confirmed/Unsupported states, ROM 128/135 filter, slider revert)");
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

    // Keys focus view on Performance
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

    backend.requestNoteOn(60, 100);
    assert(backend.model().diagnostics().active_voices == 1);

    lv_obj_t* panic_btn = findButtonWithLabel(app.rootObject(), "PANIC");
    assert(panic_btn != nullptr);
    lv_obj_send_event(panic_btn, LV_EVENT_CLICKED, nullptr);
    pumpFrame(app);
    assert(backend.model().diagnostics().active_voices == 0);
    assert(backend.model().getBool("seq.playing", false) == false);
    assert(backend.model().midiLearnSlot() == -1);

    std::puts("PASS: [5/7] Interactive widgets, Patch Browser, Sequencer, Scale/Chord Assist and Panic verified");
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
            unsigned badge_count = 0;
            verifyObjectTreeBounds(app.rootObject(), sid, t, widget_count, found_demo_badge, badge_count);
            assert(widget_count > 10);
            assert(found_demo_badge && "Persistent UI DEMO indicator missing");
            if (sid == ScreenId::Performance) {
                // StageMinimal must eliminate per-control AVAIL/PLAN badges on Performance Stage
                assert(badge_count == 0 && "Performance StageMinimal must not show technical AVAIL/PLAN badges");
            } else {
                // BlueprintVerbose screens must retain AVAIL/PLAN/DEMO technical status badges
                assert(badge_count >= 1 && "BlueprintVerbose screen missing technical status badges");
            }
        }
    }
    std::puts("PASS: [6/7] All 14 screens and sub-tabs verified strictly within 800x480 (StageMinimal vs BlueprintVerbose)");

    // 2. Stress test memory across 50 full navigation cycles (700+ screen mounts/unmounts)
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
    const ptrdiff_t leaked = static_cast<ptrdiff_t>(baseline.free_size) - static_cast<ptrdiff_t>(after.free_size);
    std::printf("INFO: LVGL Heap after 700 screen mounts: baseline_free=%zu after_free=%zu diff=%td peak_used=%zu frag=%u%%\n",
                baseline.free_size, after.free_size, leaked, after.max_used, unsigned(after.frag_pct));
    assert(leaked <= 64 && "LVGL memory leak detected across screen navigation cycles");
    assert(after.frag_pct <= 5 && "Excessive LVGL heap fragmentation");

    std::puts("PASS: [7/7] 700-screen-transition stress test passed with zero LVGL memory leaks");
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
    testVirtualKeyboardLifecycle(app, backend);
    testMockPerformanceBackendContract();
    testInteractions(app);
    testLayoutAndMemory(app);

    app.shutdown();
    std::puts("ALL UI BLUEPRINT 1.1 HOST TESTS PASSED.");
    return 0;
}
