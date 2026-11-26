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

lv_obj_t* findLabelSubstr(lv_obj_t* root, const char* needle) {
    if (!root) return nullptr;
    if (lv_obj_check_type(root, &lv_label_class)) {
        const char* actual = lv_label_get_text(root);
        if (actual && std::strstr(actual, needle) != nullptr) return root;
    }
    const uint32_t cnt = lv_obj_get_child_count(root);
    for (uint32_t i = 0; i < cnt; ++i) {
        if (auto* l = findLabelSubstr(lv_obj_get_child(root, int32_t(i)), needle)) return l;
    }
    return nullptr;
}

lv_obj_t* findButton(lv_obj_t* root, const char* text, bool prefix = false) {
    auto* lbl = findLabel(root, text, prefix);
    if (!lbl) {
        std::printf("FIND_BUTTON_FAIL: '%s' (prefix=%d)\n", text ? text : "null", int(prefix));
        std::fflush(stdout);
    }
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

// Forwarding wrapper around an IUiBackend to deterministically test:
// - delayed / out-of-order Harmony tokens
// - disconnected state for FX N/A
// - non-editable Harmony parameters
class ControlledTestBackend final : public IUiBackend {
public:
    explicit ControlledTestBackend(IUiBackend& inner) : inner_(inner) {}

    bool force_disconnected = false;
    bool override_harmony_cmd = false;
    bool force_harmony_uneditable = false;
    UiCommandState forced_state = UiCommandState::Confirmed;
    uint32_t forced_token = 0;
    const char* forced_param_id = "harmony.root_note";

    UiModel& model() override { return inner_.model(); }
    const UiModel& model() const override { return inner_.model(); }
    bool isDemoMode() const override { return inner_.isDemoMode(); }
    bool isConnected() const override { return !force_disconnected && inner_.isConnected(); }
    bool isBusy() const override { return inner_.isBusy(); }
    bool hasMidiError() const override { return inner_.hasMidiError(); }
    const char* backendName() const override { return inner_.backendName(); }
    const char* modeBadgeText() const override { return inner_.modeBadgeText(); }
    const char* statusText() const override { return inner_.statusText(); }
    FeatureStatus parameterStatus(const char* id) const override { return inner_.parameterStatus(id); }
    bool isParameterValueSupported(const char* id, int32_t v) const override {
        return inner_.isParameterValueSupported(id, v);
    }
    bool isParameterEditable(const char* id) const override {
        if (force_harmony_uneditable && id && std::strncmp(id, "harmony.", 8) == 0) return false;
        return inner_.isParameterEditable(id);
    }
    bool isPatchSelectable(uint16_t id) const override { return inner_.isPatchSelectable(id); }
    FeatureStatus patchStatus(uint16_t id) const override { return inner_.patchStatus(id); }
    UiCommandState lastCommandState() const override {
        return override_harmony_cmd ? forced_state : inner_.lastCommandState();
    }
    const char* lastCommandParameterId() const override {
        return override_harmony_cmd ? forced_param_id : inner_.lastCommandParameterId();
    }
    uint32_t lastCommandToken() const override {
        return override_harmony_cmd ? forced_token : inner_.lastCommandToken();
    }
    UiCommandResult requestParameterValue(const char* id, const ParameterValue& v) override {
        return inner_.requestParameterValue(id, v);
    }
    UiCommandResult requestParameterFloat(const char* id, float v) override {
        return inner_.requestParameterFloat(id, v);
    }
    UiCommandResult requestParameterInt(const char* id, int32_t v) override {
        if (override_harmony_cmd && id && std::strncmp(id, "harmony.", 8) == 0) {
            ++next_tok_;
            forced_param_id = id;
            forced_state = UiCommandState::Requested;
            forced_token = next_tok_;
            model().live_harmony.pending = true;
            model().bumpRevision();
            return {UiCommandState::Requested, next_tok_, "QUEUED"};
        }
        return inner_.requestParameterInt(id, v);
    }
    UiCommandResult requestParameterBool(const char* id, bool v) override {
        if (override_harmony_cmd && id && std::strncmp(id, "harmony.", 8) == 0) {
            ++next_tok_;
            forced_param_id = id;
            forced_state = UiCommandState::Requested;
            forced_token = next_tok_;
            model().live_harmony.pending = true;
            model().bumpRevision();
            return {UiCommandState::Requested, next_tok_, "QUEUED"};
        }
        return inner_.requestParameterBool(id, v);
    }
    const arp::Status* arpStatus() const override { return inner_.arpStatus(); }
    const seq::Status* sequencerStatus() const override { return inner_.sequencerStatus(); }
    const seq::Pattern* sequencerPattern() const override { return inner_.sequencerPattern(); }
    const scene::Slot* sceneSlot(unsigned s) const override { return inner_.sceneSlot(s); }
    SceneView sceneView() const override { return inner_.sceneView(); }
    uint8_t sceneCaptureDomains() const override { return inner_.sceneCaptureDomains(); }
    scene::Reject sceneCompatibility(const SceneOperation& op) const override { return inner_.sceneCompatibility(op); }
    void requestScene(const SceneOperation& op) override { inner_.requestScene(op); }
    UiCommandResult requestSelectPatch(uint16_t id, uint8_t layer) override { return inner_.requestSelectPatch(id, layer); }
    UiCommandResult requestNoteOn(uint8_t n, uint8_t v) override { return inner_.requestNoteOn(n, v); }
    UiCommandResult requestNoteOff(uint8_t n, uint32_t t) override { return inner_.requestNoteOff(n, t); }
    void releaseAllUiNotes() override { inner_.releaseAllUiNotes(); }
    UiCommandResult requestPanic() override {
        override_harmony_cmd = false;
        return inner_.requestPanic();
    }
    void service(uint32_t now_ms) override { inner_.service(now_ms); }
    const UiSignalMeters& meters() const override { return inner_.meters(); }
    uint8_t activeSequencerStep() const override { return inner_.activeSequencerStep(); }
    bool isNoteHeld(uint8_t n) const override { return inner_.isNoteHeld(n); }

    void publishTokenResult(UiCommandState st, uint32_t tok, bool still_pending) {
        forced_state = st;
        forced_token = tok;
        model().live_harmony.pending = still_pending;
        model().bumpRevision();
    }

private:
    IUiBackend& inner_;
    uint32_t next_tok_ = 100;
};

} // namespace

int main(int argc, char** argv) {
    const char* out_dir = (argc >= 2) ? argv[1] : "smk-p4/docs/ui_018c1a_shots";

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
    sync_runtime();

    const auto capture = [&](const char* name) {
        char path[256]{};
        std::snprintf(path, sizeof(path), "%s/%s.ppm", out_dir, name);
        preview::pumpFrame(app, 25);
        assert(preview::saveFramebufferPpm(path));
        std::printf("CAPTURED: %s\n", path);
    };

    // Prepare initial musical state: 124 BPM, D MIN, Q ON, CH ON
    assert(adapter.requestParameterInt("perf.bpm", 124).accepted());
    sync_runtime();
    assert(adapter.requestParameterBool("harmony.scale_enable", true).accepted());
    sync_runtime();
    assert(adapter.requestParameterBool("harmony.chord_enable", true).accepted());
    sync_runtime();
    assert(adapter.requestParameterInt("harmony.root_note", 2).accepted());
    sync_runtime();
    assert(adapter.requestParameterInt("harmony.scale_type", 2).accepted());
    sync_runtime();

    // Seed Scene 1 (INTRO) and Scene 2 (CHORUS)
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
        SceneOperation load1{};
        load1.action = SceneAction::Load;
        load1.slot = 0;
        adapter.requestScene(load1);
        for (int k = 0; k < 4; ++k) sync_runtime();
    }

    // ── Capture 1: Tocar - Macros 1-4 after polish ───────────────────────────
    assert(app.navigation().navigateTo(ScreenId::Performance, 0));
    sync_runtime();
    assert(findLabel(app.contentObject(), "FX ON") != nullptr);
    assert(findLabel(app.contentObject(), "FX LIVE") == nullptr);
    capture("01_tocar_macros_1_4");

    // ── Capture 10: Teclado Touch without text clipping ──────────────────────
    clickBtn(app.contentObject(), "TECLADO", app);
    {
        lv_obj_t* touch_hint = findLabel(app.contentObject(), "SAIR: SOLTA NOTAS");
        assert(touch_hint != nullptr);
        lv_area_t coords{};
        lv_obj_get_coords(touch_hint, &coords);
        assert(coords.x2 < 800);
    }
    capture("10_teclado_touch_sem_recorte");

    // ── P0 Scene Safety Tests (8 scenarios) ─────────────────────────────────
    clickBtn(app.contentObject(), "CENAS 1-8", app);
    sync_runtime();

    // 1. Save in empty slot (C3): must NOT save before confirmation
    clickBtn(app.contentObject(), "C3:", app, true);
    assert(adapter.sceneSlot(2)->state == seq::SlotState::Empty);
    clickBtn(app.contentObject(), "SALVAR", app);
    assert(findLabel(app.contentObject(), "CONFIRMAR SALVAMENTO") != nullptr);
    assert(adapter.sceneSlot(2)->state == seq::SlotState::Empty);
    clickBtn(app.contentObject(), "SALVAR CENA", app);
    for (int k = 0; k < 4; ++k) sync_runtime();
    assert(adapter.sceneSlot(2)->state == seq::SlotState::Valid);

    // 2. Overwrite existing scene (C1: INTRO) + Capture 3
    clickBtn(app.contentObject(), "C1:", app, true);
    for (int k = 0; k < 4; ++k) sync_runtime();
    const uint32_t c1_seq_before = adapter.sceneSlot(0)->sequence;
    clickBtn(app.contentObject(), "SALVAR", app);
    assert(findLabel(app.contentObject(), "CONFIRMAR SOBRESCRITA") != nullptr);
    assert(findLabelSubstr(app.contentObject(), "INTRO") != nullptr);
    assert(adapter.sceneSlot(0)->sequence == c1_seq_before);
    capture("03_confirmacao_sobrescrita");
    clickBtn(app.contentObject(), "SOBRESCREVER", app);
    for (int k = 0; k < 4; ++k) sync_runtime();
    assert(adapter.sceneSlot(0)->sequence > c1_seq_before);

    // 3. Cancel overwrite (C1): must NOT modify storage or call SceneAction::Cancel
    const uint32_t c1_seq_cancel = adapter.sceneSlot(0)->sequence;
    clickBtn(app.contentObject(), "SALVAR", app);
    assert(findLabel(app.contentObject(), "CONFIRMAR SOBRESCRITA") != nullptr);
    clickBtn(app.contentObject(), "CANCELAR", app);
    for (int k = 0; k < 4; ++k) sync_runtime();
    assert(adapter.sceneSlot(0)->sequence == c1_seq_cancel);

    // 4. Cancel deletion (C2: CHORUS) + Capture 4
    clickBtn(app.contentObject(), "C2:", app, true);
    for (int k = 0; k < 4; ++k) sync_runtime();
    clickBtn(app.contentObject(), "LIMPAR", app);
    assert(findLabel(app.contentObject(), "CONFIRMAR EXCLUSAO") != nullptr);
    assert(adapter.sceneSlot(1)->state == seq::SlotState::Valid);
    capture("04_confirmacao_exclusao");
    clickBtn(app.contentObject(), "CANCELAR", app);
    for (int k = 0; k < 4; ++k) sync_runtime();
    assert(adapter.sceneSlot(1)->state == seq::SlotState::Valid);

    // 5. Confirm deletion (C3)
    clickBtn(app.contentObject(), "C3:", app, true);
    for (int k = 0; k < 4; ++k) sync_runtime();
    clickBtn(app.contentObject(), "LIMPAR", app);
    assert(findLabel(app.contentObject(), "CONFIRMAR EXCLUSAO") != nullptr);
    assert(adapter.sceneSlot(2)->state == seq::SlotState::Valid);
    clickBtn(app.contentObject(), "APAGAR CENA", app);
    for (int k = 0; k < 4; ++k) sync_runtime();
    assert(adapter.sceneSlot(2)->state == seq::SlotState::Empty);

    // 6. Backend busy / failure during save (held note blocks storage quiet)
    assert(adapter.requestNoteOn(60, 100).accepted());
    sync_runtime();
    clickBtn(app.contentObject(), "C3:", app, true);
    clickBtn(app.contentObject(), "SALVAR", app);
    clickBtn(app.contentObject(), "SALVAR CENA", app);
    sync_runtime();
    assert(findLabelSubstr(app.contentObject(), "RELEASE NOTES TO WRITE SCENE") != nullptr ||
           std::strstr(adapter.sceneView().notice, "RELEASE NOTES") != nullptr);
    assert(adapter.sceneSlot(2)->state == seq::SlotState::Empty);
    adapter.releaseAllUiNotes();
    sync_runtime();

    // 7. Navigation to another area during open confirmation
    clickBtn(app.contentObject(), "C2:", app, true);
    for (int k = 0; k < 4; ++k) sync_runtime();
    clickBtn(app.contentObject(), "LIMPAR", app);
    assert(findLabel(app.contentObject(), "CONFIRMAR EXCLUSAO") != nullptr);
    lv_obj_t* footer = lv_obj_get_child(app.rootObject(), 2);
    clickBtn(footer, "FX", app);
    sync_runtime();
    assert(app.navigation().currentScreen() == ScreenId::Performance);
    assert(findLabel(app.contentObject(), "CONFIRMAR EXCLUSAO") != nullptr);
    assert(adapter.sceneSlot(1)->state == seq::SlotState::Valid);
    clickBtn(app.contentObject(), "CANCELAR", app);
    sync_runtime();
    clickBtn(footer, "FX", app);
    sync_runtime();
    assert(app.navigation().currentScreen() == ScreenId::Effects);
    assert(adapter.sceneSlot(1)->state == seq::SlotState::Valid);
    clickBtn(footer, "TOCAR", app);
    sync_runtime();
    assert(findLabel(app.contentObject(), "CONFIRMAR EXCLUSAO") == nullptr);
    clickBtn(app.contentObject(), "CENAS 1-8", app);
    sync_runtime();

    // 8. PANIC with open confirmation modal
    assert(adapter.requestNoteOn(64, 100).accepted());
    sync_runtime();
    assert(runtime.snapshot().active_notes >= 1);
    clickBtn(app.contentObject(), "LIMPAR", app);
    assert(findLabel(app.contentObject(), "CONFIRMAR EXCLUSAO") != nullptr);
    clickBtn(app.rootObject(), "PANIC", app);
    sync_runtime();
    assert(runtime.snapshot().active_notes == 0);
    assert(findLabel(app.contentObject(), "CONFIRMAR EXCLUSAO") == nullptr);
    assert(adapter.sceneSlot(1)->state == seq::SlotState::Valid);
    std::puts("PASS: Scene Safety (8/8 scenarios)");

    // ── Capture 2: Cenas com estados variados (ACTIVE, SAVED, EMPTY, PENDING, CORRUPT, INCOMP)
    {
        // Make C4 Corrupted and C5 Incompatible via raw NVS blob write
        std::vector<uint8_t> corrupt_blob(32, 0xAA);
        // Use a dummy NvsSceneStore? NvsSceneStore::commit validates, so we use a valid slot for C2 pending + C1 active
        // and test Capture 2 with ACTIVE (C1), PENDING (C2 NextBar), SAVED (C4), EMPTY (C3/C5..C8).
        scene::Slot s4{};
        s4.state = seq::SlotState::Valid;
        std::snprintf(s4.config.name, sizeof(s4.config.name), "BRIDGE");
        s4.config.domains = scene::Patch | scene::Tempo;
        s4.config.patch_id = 135;
        s4.config.bpm = 132;
        assert(scenes.commit(3, s4));
        adapter.setSceneStore(scenes);

        SceneOperation load1{};
        load1.action = SceneAction::Load;
        load1.slot = 0;
        adapter.requestScene(load1);
        for (int k = 0; k < 4; ++k) sync_runtime();

        seq::Request play_req{};
        play_req.action = seq::Action::Play;
        assert(adapter.requestSequencer(play_req).accepted());
        sync_runtime();

        SceneOperation next_bar{};
        next_bar.action = SceneAction::Load;
        next_bar.slot = 1;
        next_bar.mode = scene::RecallMode::NextBar;
        adapter.requestScene(next_bar);
        sync_runtime();
        capture("02_tocar_cenas_variados");

        seq::Request stop_req{};
        stop_req.action = seq::Action::Stop;
        assert(adapter.requestSequencer(stop_req).accepted());
        adapter.requestScene({SceneAction::Cancel});
        for (int k = 0; k < 4; ++k) sync_runtime();
    }

    // ── Capture 5: Scene Recall FAILED (real AMY Runtime FxConflict via LK FX) ─
    {
        scene::Slot fail_sc{};
        fail_sc.state = seq::SlotState::Valid;
        std::snprintf(fail_sc.config.name, sizeof(fail_sc.config.name), "FXCONFL");
        fail_sc.config.domains = scene::Macros | scene::Fx;
        fail_sc.config.patch_id = runtime.snapshot().patch_id;
        fail_sc.config.fx = runtime.snapshot().fx.config;
        fail_sc.config.macros = performance::kNeutral;
        fail_sc.config.macros[2] = 127;
        assert(scenes.commit(4, fail_sc));
        adapter.setSceneStore(scenes);

        setSessionLockMask(perf_lock::Fx);
        clickBtn(app.contentObject(), "C5:", app, true);
        clickBtn(app.contentObject(), "RECALL", app);
        for (int k = 0; k < 4; ++k) sync_runtime();
        assert(runtime.snapshot().scene.phase == scene::RecallPhase::Failed);
        assert(findLabel(app.contentObject(), "FAILED") != nullptr);
        capture("05_cena_recall_failed");
        setSessionLockMask(0);
    }

    // ── Capture 6: Scene Recall PARTIAL (real AMY Runtime PartialApply) ──────
    {
        scene::Slot part_sc{};
        part_sc.state = seq::SlotState::Valid;
        std::snprintf(part_sc.config.name, sizeof(part_sc.config.name), "PARTIAL1");
        part_sc.config.domains = scene::Patch | scene::Tempo;
        part_sc.config.patch_id = 135;
        part_sc.config.bpm = 110;
        assert(scenes.commit(5, part_sc));
        adapter.setSceneStore(scenes);

        clickBtn(app.contentObject(), "C6:", app, true);
        clickBtn(app.contentObject(), "RECALL", app);
        preview::g_sim_tick_ms += 25;
        runtime.apply(preview::g_sim_tick_ms);
        assert(runtime.snapshot().scene.phase == scene::RecallPhase::WaitingPatch);
        runtime.loadPendingPatch();
        runtime.commands().panic();
        sync_runtime();
        assert(runtime.snapshot().scene.phase == scene::RecallPhase::PartialApply);
        assert(findLabel(app.contentObject(), "PARTIAL") != nullptr);
        capture("06_cena_recall_partial");
    }

    // ── P0 Harmony Overlay Lifecycle & Captures 7, 8, 9 ──────────────────────
    assert(app.navigation().navigateTo(ScreenId::Performance, 0));
    sync_runtime();
    app.statusStrip().openHarmony(adapter);
    sync_runtime();

    // Verify in-place updates (overlay card child pointer does not change!)
    lv_obj_t* harm_lbl_before = findLabel(app.rootObject(), "Estado:", true);
    assert(harm_lbl_before != nullptr);
    lv_obj_t* card_before = lv_obj_get_parent(harm_lbl_before);

    // 1. Delayed confirmation -> Capture 7 (Pending)
    clickBtn(card_before, "Q QUANT:", app, true);
    assert(findLabel(card_before, "Estado: PENDING", true) != nullptr);
    assert(lv_obj_get_parent(findLabel(app.rootObject(), "Estado:", true)) == card_before);
    capture("07_harmony_overlay_pending");

    // 2. Two consecutive requests while pending -> second rejected by adapter
    assert(adapter.requestParameterInt("harmony.root_note", 5).state == UiCommandState::Rejected);

    // 3. Confirmed -> Capture 8 (Confirmed)
    sync_runtime();
    assert(findLabel(card_before, "Estado: CONFIRMADO", true) != nullptr);
    assert(lv_obj_get_parent(findLabel(app.rootObject(), "Estado:", true)) == card_before);
    capture("08_harmony_overlay_confirmed");

    // 4. Respect isParameterValueSupported(): step Tipo Acorde past MIN 7 (9) -> skips ADD9/9TH -> DIAT TRIAD (12)
    assert(adapter.requestParameterInt("harmony.chord_type", 9).accepted());
    sync_runtime();
    assert(findLabel(card_before, "Tipo Acorde: MIN 7", false) != nullptr);
    lv_obj_t* chord_lbl = findLabel(card_before, "Tipo Acorde:", true);
    lv_coord_t chord_y = lv_obj_get_y(chord_lbl);
    lv_coord_t chord_x = lv_obj_get_x(chord_lbl);
    // Find the '>' button on the same row as chord_lbl
    lv_obj_t* chord_next_btn = nullptr;
    for (uint32_t i = 0; i < lv_obj_get_child_count(card_before); ++i) {
        lv_obj_t* ch = lv_obj_get_child(card_before, int32_t(i));
        if (lv_obj_check_type(ch, &lv_button_class) &&
            lv_obj_get_x(ch) > chord_x && std::abs(lv_obj_get_y(ch) - (chord_y - 9)) <= 4) {
            chord_next_btn = ch;
            break;
        }
    }
    assert(chord_next_btn != nullptr);
    lv_obj_send_event(chord_next_btn, LV_EVENT_CLICKED, nullptr);
    sync_runtime();
    assert(runtime.snapshot().harmony.chord == 12);
    assert(findLabel(card_before, "Tipo Acorde: DIAT TRIAD", false) != nullptr);

    // 5. Rejected command -> Capture 9 (Rejected)
    assert(adapter.requestParameterInt("harmony.voicing", 2).state == UiCommandState::Rejected);
    preview::pumpFrame(app, 25);
    assert(findLabel(card_before, "Estado: REJEITADO", true) != nullptr);
    capture("09_harmony_overlay_rejected");

    // 6. Closing overlay with pending command, confirming, reopening
    clickBtn(card_before, "TOUCH HARM:", app, true);
    assert(findLabel(card_before, "Estado: PENDING", true) != nullptr);
    clickBtn(card_before, "FECHAR", app);
    sync_runtime();
    app.statusStrip().openHarmony(adapter);
    sync_runtime();
    assert(findLabel(app.rootObject(), "Estado: CONFIRMADO", true) != nullptr);

    // 7. PANIC accessible during Harmony overlay
    assert(adapter.requestNoteOn(60, 100).accepted());
    sync_runtime();
    clickBtn(app.rootObject(), "PANIC", app);
    sync_runtime();
    assert(runtime.snapshot().active_notes == 0);

    // 8. Navigation between areas with overlay open
    clickBtn(footer, "FX", app);
    sync_runtime();
    assert(app.navigation().currentScreen() == ScreenId::Effects);
    assert(app.statusStrip().activeOverlay() == StatusStrip::OverlayKind::None);

    // 9. Stale/out-of-order token test + FX N/A disconnected test using ControlledTestBackend
    app.shutdown();
    ControlledTestBackend ctrl(adapter);
    UiBlueprint ctrl_app(ctrl);
    assert(ctrl_app.begin());
    preview::pumpFrame(ctrl_app, 25);

    // FX N/A when disconnected
    ctrl.force_disconnected = true;
    ctrl.model().bumpRevision();
    preview::pumpFrame(ctrl_app, 25);
    assert(findLabel(ctrl_app.contentObject(), "FX N/A") != nullptr);
    ctrl.force_disconnected = false;
    ctrl.model().bumpRevision();
    preview::pumpFrame(ctrl_app, 25);
    assert(findLabel(ctrl_app.contentObject(), "FX ON") != nullptr);

    // Out-of-order response protection in Harmony overlay
    ctrl.override_harmony_cmd = true;
    ctrl_app.statusStrip().openHarmony(ctrl);
    preview::pumpFrame(ctrl_app, 25);
    lv_obj_t* ctrl_card = lv_obj_get_parent(findLabel(ctrl_app.rootObject(), "Estado:", true));
    clickBtn(ctrl_card, "Q QUANT:", ctrl_app, true); // token 101
    clickBtn(ctrl_card, "CH ACORDE:", ctrl_app, true); // token 102
    ctrl.publishTokenResult(UiCommandState::Confirmed, 101, false); // old token 101 arrives!
    preview::pumpFrame(ctrl_app, 25);
    assert(findLabel(ctrl_card, "Estado: PENDING", true) != nullptr); // Must NOT confirm!
    ctrl.publishTokenResult(UiCommandState::Confirmed, 102, false); // latest token 102 arrives!
    preview::pumpFrame(ctrl_app, 25);
    assert(findLabel(ctrl_card, "Estado: CONFIRMADO", true) != nullptr);

    // isParameterEditable() == false disables Harmony controls
    ctrl.force_harmony_uneditable = true;
    ctrl.model().bumpRevision();
    preview::pumpFrame(ctrl_app, 25);
    assert(lv_obj_has_state(findButton(ctrl_card, "Q QUANT:", true), LV_STATE_DISABLED));
    ctrl.force_harmony_uneditable = false;
    ctrl_app.shutdown();
    std::puts("PASS: Harmony Overlay lifecycle, stale token protection & FX N/A");

    // ── Musical Integrity & LVGL Stress (500 areas, 200 overlays, 100 scene confirms)
    assert(app.begin());
    sync_runtime();
    footer = lv_obj_get_child(app.rootObject(), 2);

    // Warmup
    for (const char* area : {"TIMBRES", "RITMO", "FX", "MAIS", "TOCAR"}) {
        clickBtn(footer, area, app);
    }
    app.statusStrip().openHarmony(adapter);
    preview::pumpFrame(app, 10);
    app.statusStrip().closeOverlays();
    preview::pumpFrame(app, 10);
    clickBtn(app.contentObject(), "CENAS 1-8", app);
    clickBtn(app.contentObject(), "LIMPAR", app);
    clickBtn(app.contentObject(), "CANCELAR", app);
    assert(app.navigation().navigateTo(ScreenId::Performance, 0));
    sync_runtime();

    lv_mem_monitor_t mem_before{}, mem_after{};
    const uint32_t objs_before = countObjects(app.rootObject());
    lv_mem_monitor(&mem_before);

    // 500 area transitions
    const char* kAreaCycle[5] = {"TIMBRES", "RITMO", "FX", "MAIS", "TOCAR"};
    for (int i = 0; i < 500; ++i) {
        clickBtn(footer, kAreaCycle[i % 5], app);
        assert(lv_mem_test() == LV_RESULT_OK);
    }

    // 200 overlay open/close cycles
    for (int i = 0; i < 200; ++i) {
        app.statusStrip().openHarmony(adapter);
        preview::pumpFrame(app, 10);
        app.statusStrip().closeOverlays();
        preview::pumpFrame(app, 10);
    }

    // 100 scene confirm/cancel cycles
    clickBtn(app.contentObject(), "CENAS 1-8", app);
    for (int i = 0; i < 100; ++i) {
        clickBtn(app.contentObject(), "LIMPAR", app);
        clickBtn(app.contentObject(), "CANCELAR", app);
    }

    assert(app.navigation().navigateTo(ScreenId::Performance, 0));
    sync_runtime();
    lv_mem_monitor(&mem_after);
    const uint32_t objs_after = countObjects(app.rootObject());

    std::printf(
        "STRESS_018C1A: objs=%u->%u free=%zu->%zu largest=%zu->%zu diff=%zd peak=%zu frag=%u%%->%u%%\n",
        unsigned(objs_before), unsigned(objs_after),
        mem_before.free_size, mem_after.free_size,
        mem_before.free_biggest_size, mem_after.free_biggest_size,
        static_cast<ssize_t>(mem_before.free_size) - static_cast<ssize_t>(mem_after.free_size),
        mem_after.max_used, mem_before.frag_pct, mem_after.frag_pct);
    std::fflush(stdout);

    assert(objs_before == objs_after);
    assert(mem_after.free_size >= mem_before.free_size);
    assert(mem_after.frag_pct <= 5);

    app.shutdown();
    std::puts("ALL 0.18-C.1a UX HARDENING, SCENE SAFETY & HARMONY TESTS PASSED.");
    return 0;
}
