#include "fake_nvs.h"
#include "pattern_nvs.h"
#include "performance_scene_nvs.h"
#include "ui_blueprint_performance_adapter.h"
#include "local_performance_backend.h"
#include "performance_factory_routes.h"
#include "../ui-preview/preview_harness.h"
#include "../components/amy_runtime/fm_topology.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>

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

class ControlledHarmonyBackend final : public IUiBackend {
public:
    explicit ControlledHarmonyBackend(IUiBackend& inner) : inner_(inner) {}

    bool override_harmony = false;
    UiCommandState forced_state = UiCommandState::Confirmed;
    uint32_t forced_token = 0;
    const char* forced_param_id = "harmony.root_note";

    UiModel& model() override { return inner_.model(); }
    const UiModel& model() const override { return inner_.model(); }
    bool isDemoMode() const override { return inner_.isDemoMode(); }
    bool isConnected() const override { return inner_.isConnected(); }
    bool isBusy() const override { return inner_.isBusy(); }
    bool hasMidiError() const override { return inner_.hasMidiError(); }
    const char* backendName() const override { return inner_.backendName(); }
    const char* modeBadgeText() const override { return inner_.modeBadgeText(); }
    const char* statusText() const override { return inner_.statusText(); }
    FeatureStatus parameterStatus(const char* id) const override { return inner_.parameterStatus(id); }
    bool isParameterValueSupported(const char* id, int32_t v) const override {
        return inner_.isParameterValueSupported(id, v);
    }
    bool isParameterEditable(const char* id) const override { return inner_.isParameterEditable(id); }
    bool isPatchSelectable(uint16_t id) const override { return inner_.isPatchSelectable(id); }
    FeatureStatus patchStatus(uint16_t id) const override { return inner_.patchStatus(id); }
    UiCommandState lastCommandState() const override {
        return override_harmony ? forced_state : inner_.lastCommandState();
    }
    const char* lastCommandParameterId() const override {
        return override_harmony ? forced_param_id : inner_.lastCommandParameterId();
    }
    uint32_t lastCommandToken() const override {
        return override_harmony ? forced_token : inner_.lastCommandToken();
    }
    UiCommandResult requestParameterValue(const char* id, const ParameterValue& v) override {
        return inner_.requestParameterValue(id, v);
    }
    UiCommandResult requestParameterFloat(const char* id, float v) override {
        return inner_.requestParameterFloat(id, v);
    }
    UiCommandResult requestParameterInt(const char* id, int32_t v) override {
        if (override_harmony && id && std::strncmp(id, "harmony.", 8) == 0) {
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
        if (override_harmony && id && std::strncmp(id, "harmony.", 8) == 0) {
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
        override_harmony = false;
        return inner_.requestPanic();
    }
    void service(uint32_t now_ms) override { inner_.service(now_ms); }
    const UiSignalMeters& meters() const override { return inner_.meters(); }
    uint8_t activeSequencerStep() const override { return inner_.activeSequencerStep(); }
    bool isNoteHeld(uint8_t n) const override { return inner_.isNoteHeld(n); }

    void publish(UiCommandState st, uint32_t tok, bool still_pending) {
        forced_state = st;
        forced_token = tok;
        model().live_harmony.pending = still_pending;
        model().bumpRevision();
    }

private:
    IUiBackend& inner_;
    uint32_t next_tok_ = 200;
};

} // namespace

int main(int argc, char** argv) {
    const char* out_dir = (argc >= 2) ? argv[1] : "smk-p4/docs/ui_018c2_shots";

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

    bool sim_usb_connected = true;
    auto sync_runtime = [&](uint32_t dt_ms = 25) {
        preview::g_sim_tick_ms += dt_ms;
        runtime.apply(preview::g_sim_tick_ms);
        assert(runtime.render());
        perf.publish(runtime.snapshot(), sim_usb_connected, 120, 0);
        preview::pumpFrame(app, dt_ms);
    };
    sync_runtime();

    const auto capture = [&](const char* name) {
        char path[256]{};
        std::snprintf(path, sizeof(path), "%s/%s.ppm", out_dir, name);
        // Captures are real LVGL + AMY renders with injected host MIDI packets.
        // Label them explicitly so they cannot be mistaken for physical USB tests.
        auto* mode = findLabel(app.rootObject(), "P4 LIVE");
        if (mode) lv_label_set_text_static(mode, "HOST SIM");
        preview::pumpFrame(app, 25);
        assert(preview::saveFramebufferPpm(path));
        std::printf("CAPTURED: %s\n", path);
        if (mode) lv_label_set_text_static(mode, "P4 LIVE");
    };

    // ── C.2-A.1 & C.2-A.3: Scene selection without recall, explicit recall, active vs failed separation
    setSessionLockMask(0);
    assert(adapter.requestParameterInt("perf.bpm", 124).accepted());
    sync_runtime();
    assert(adapter.requestParameterBool("harmony.scale_enable", true).accepted());
    sync_runtime();
    assert(adapter.requestParameterBool("harmony.chord_enable", true).accepted());
    sync_runtime();

    {
        scene::Slot s1{};
        s1.state = seq::SlotState::Valid;
        std::snprintf(s1.config.name, sizeof(s1.config.name), "INTRO");
        s1.config.domains = scene::Patch | scene::Tempo | scene::Macros;
        s1.config.patch_id = 128;
        s1.config.bpm = 124;
        s1.config.macros = performance::kNeutral;
        assert(scenes.commit(0, s1));

        scene::Slot s2{};
        s2.state = seq::SlotState::Valid;
        std::snprintf(s2.config.name, sizeof(s2.config.name), "CHORUS");
        s2.config.domains = scene::Patch | scene::Tempo | scene::Fx | scene::Harmony | scene::Arp | scene::Macros;
        s2.config.patch_id = 135;
        s2.config.bpm = 138;
        s2.config.fx = runtime.snapshot().fx.config;
        if (s2.config.fx.delay_time_ms < 20.0f) s2.config.fx.delay_time_ms = 350.0f;
        s2.config.fx.delay_mix = 0.55f;
        s2.config.harmony = runtime.snapshot().harmony;
        s2.config.harmony.scale_on = false;
        s2.config.arp = runtime.snapshot().arp.config;
        s2.config.arp.enabled = true;
        s2.config.macros = performance::kNeutral;
        s2.config.macros[0] = 110;
        if (!scenes.commit(1, s2)) {
            std::fprintf(stderr, "S2 COMMIT FAIL: fx=%d harm=%d arp=%d mac=%d\n",
                        int(fx::valid(s2.config.fx)), int(harmony::valid(s2.config.harmony)),
                        int(scene::validArp(s2.config.arp)), int(performance::valid(s2.config.macros)));
            assert(false);
        }

        scene::Slot s5{};
        s5.state = seq::SlotState::Valid;
        std::snprintf(s5.config.name, sizeof(s5.config.name), "FXCONFL");
        s5.config.domains = scene::Macros | scene::Fx;
        s5.config.patch_id = 128;
        s5.config.fx = runtime.snapshot().fx.config;
        s5.config.macros = performance::kNeutral;
        s5.config.macros[2] = 127;
        assert(scenes.commit(4, s5));

        adapter.setSceneStore(scenes);
        SceneOperation load1{};
        load1.action = SceneAction::Load;
        load1.slot = 0;
        adapter.requestScene(load1);
        for (int k = 0; k < 4; ++k) sync_runtime();
    }

    clickBtn(app.contentObject(), "CENAS 1-8", app);
    sync_runtime();
    assert(runtime.snapshot().scene.active == 0);
    assert(runtime.snapshot().patch_id == 128);
    assert(runtime.snapshot().arp.tempo.bpm == 124);

    // Tap C2 card: must ONLY select C2, NOT recall C2!
    clickBtn(app.contentObject(), "C2:", app, true);
    for (int k = 0; k < 4; ++k) sync_runtime();
    assert(runtime.snapshot().scene.active == 0);
    assert(runtime.snapshot().patch_id == 128);
    assert(runtime.snapshot().arp.tempo.bpm == 124);
    assert(runtime.snapshot().harmony.scale_on == true);
    assert(runtime.snapshot().arp.config.enabled == false);
    assert(runtime.snapshot().fx.config.delay_mix < 0.05f);
    assert(adapter.model().getInt("perf.macro.1", 64) == 64);
    assert(findLabel(app.contentObject(), "ATIVA: INTRO") != nullptr);
    assert(findLabel(app.contentObject(), "SEL: C2", true) != nullptr);
    capture("18_cena_selecionada_sem_recall");

    // Explicit RECALL must load C2
    clickBtn(app.contentObject(), "RECALL", app);
    sync_runtime();
    runtime.loadPendingPatch();
    for (int k = 0; k < 4; ++k) sync_runtime();
    assert(runtime.snapshot().scene.active == 1);
    assert(runtime.snapshot().patch_id == 135);
    assert(runtime.snapshot().arp.config.enabled == true);
    assert(runtime.snapshot().fx.config.delay_mix > 0.5f);

    // Reload C1 (INTRO), then attempt C5 recall with LK FX -> C1 remains active, C5 fails separately!
    clickBtn(app.contentObject(), "C1:", app, true);
    clickBtn(app.contentObject(), "RECALL", app);
    sync_runtime();
    runtime.loadPendingPatch();
    for (int k = 0; k < 4; ++k) sync_runtime();
    assert(runtime.snapshot().scene.active == 0);

    setSessionLockMask(perf_lock::Fx);
    clickBtn(app.contentObject(), "C5:", app, true);
    clickBtn(app.contentObject(), "RECALL", app);
    for (int k = 0; k < 4; ++k) sync_runtime();
    assert(runtime.snapshot().scene.active == 0);
    assert(runtime.snapshot().scene.phase == scene::RecallPhase::Failed);
    assert(findLabel(app.contentObject(), "ATIVA: INTRO | RECALL C5: FALHOU") != nullptr);
    assert(findLabel(app.contentObject(), "CENA ATIVA: INTRO | FAILED") == nullptr);
    capture("20_cena_ativa_falha_outro_slot");
    setSessionLockMask(0);
    std::puts("PASS: C.2-A Scene selection vs recall & active/error separation");

    // ── C.2-A.2: Independent Q / CH indicators ──────────────────────────────
    assert(adapter.requestParameterBool("harmony.scale_enable", true).accepted());
    sync_runtime();
    assert(adapter.requestParameterBool("harmony.chord_enable", true).accepted());
    sync_runtime();
    app.shutdown();
    ControlledHarmonyBackend ctrl(adapter);
    UiBlueprint ctrl_app(ctrl);
    assert(ctrl_app.begin());
    preview::pumpFrame(ctrl_app, 25);

    // Ensure Q ON and CH ON initially
    assert(ctrl.model().getBool("harmony.scale_enable", false) == true);
    assert(ctrl.model().getBool("harmony.chord_enable", false) == true);
    assert(findLabel(ctrl_app.rootObject(), "Q ON") != nullptr);
    assert(findLabel(ctrl_app.rootObject(), "CH ON") != nullptr);

    // Pending harmony.root_note must NOT turn Q or CH into Q WAIT / CH WAIT
    ctrl.override_harmony = true;
    ctrl_app.statusStrip().openHarmony(ctrl);
    preview::pumpFrame(ctrl_app, 25);
    lv_obj_t* harm_card = lv_obj_get_parent(findLabel(ctrl_app.rootObject(), "Estado:", true));
    assert(ctrl.requestParameterInt("harmony.root_note", 4).state == UiCommandState::Requested);
    preview::pumpFrame(ctrl_app, 25);
    assert(findLabel(ctrl_app.rootObject(), "Q ON") != nullptr);
    assert(findLabel(ctrl_app.rootObject(), "CH ON") != nullptr);
    assert(findLabel(ctrl_app.rootObject(), "Q WAIT") == nullptr);
    assert(findLabel(ctrl_app.rootObject(), "CH WAIT") == nullptr);
    assert(findLabel(harm_card, "Estado: PENDING [ROOT]", true) != nullptr);

    // Rejected harmony.voicing must NOT turn Q or CH into Q REJ / CH REJ
    ctrl.publish(UiCommandState::Rejected, 201, false);
    preview::pumpFrame(ctrl_app, 25);
    assert(findLabel(ctrl_app.rootObject(), "Q ON") != nullptr);
    assert(findLabel(ctrl_app.rootObject(), "CH ON") != nullptr);
    assert(findLabel(ctrl_app.rootObject(), "Q REJ") == nullptr);
    assert(findLabel(ctrl_app.rootObject(), "CH REJ") == nullptr);
    assert(findLabel(harm_card, "Estado: REJEITADO [ROOT]", true) != nullptr);

    // Q pending leaves CH ON untouched
    clickBtn(ctrl_app.rootObject(), "Q ON", ctrl_app);
    assert(findLabel(ctrl_app.rootObject(), "Q WAIT") != nullptr);
    assert(findLabel(ctrl_app.rootObject(), "CH ON") != nullptr);
    capture("19_q_ch_independentes");

    // Stale token 201 cannot confirm newer token 202
    ctrl.publish(UiCommandState::Confirmed, 201, true);
    preview::pumpFrame(ctrl_app, 25);
    assert(findLabel(ctrl_app.rootObject(), "Q WAIT") != nullptr);
    ctrl.publish(UiCommandState::Confirmed, 202, false);
    preview::pumpFrame(ctrl_app, 25);
    assert(findLabel(ctrl_app.rootObject(), "Q ON") != nullptr);

    ctrl_app.shutdown();
    std::puts("PASS: C.2-A Independent Q/CH indicators & stale token guard");

    // ── C.2-B: Patch Browser (list, pre-select, pending load, discard protection)
    assert(app.begin());
    sync_runtime();
    if (adapter.patchNeedsDiscardConfirmation()) {
        assert(adapter.confirmPatchDiscard(128, 0, 0).accepted());
        sync_runtime();
        runtime.loadPendingPatch();
        for (int k = 0; k < 4; ++k) sync_runtime();
    }
    assert(!adapter.patchNeedsDiscardConfirmation());
    assert(runtime.snapshot().patch_id == 128);

    assert(app.navigation().navigateTo(ScreenId::PatchBrowser));
    sync_runtime();
    assert(findLabel(app.contentObject(), "PATCH ATIVO") != nullptr);
    capture("01_biblioteca_lista");

    // Filter to AMY DX7 and go to page 2 where patch 135 lives, then pre-select 135 (must NOT load yet!)
    clickBtn(app.contentObject(), "AMY DX7", app);
    clickBtn(app.contentObject(), "PAGE >", app);
    clickBtn(app.contentObject(), "135 ", app, true);
    for (int k = 0; k < 4; ++k) sync_runtime();
    assert(runtime.snapshot().patch_id == 128);
    assert(findLabel(app.contentObject(), "PRE-SELECAO") != nullptr);
    capture("02_biblioteca_selecionado");

    // Request quick load -> enters pending loading state before runtime.loadPendingPatch()
    clickBtn(app.contentObject(), "CARREGAR JA", app);
    if (!adapter.isPatchLoading()) {
        std::fprintf(stderr, "NOT LOADING: busy=%d dirty=%d notice='%s'\n",
                     int(adapter.isBusy()), int(adapter.patchNeedsDiscardConfirmation()),
                     adapter.model().lastNotice());
        assert(false);
    }
    assert(findLabel(app.contentObject(), "CARREGANDO PATCH...") != nullptr);
    capture("03_biblioteca_carregando");

    sync_runtime();
    runtime.loadPendingPatch();
    for (int k = 0; k < 4; ++k) sync_runtime();
    assert(runtime.snapshot().patch_id == 135);
    assert(!adapter.isPatchLoading());

    // Edit a parameter so patch becomes dirty, then try to load 128 -> discard dialog must appear
    assert(adapter.requestParameterFloat("amp.level", 85.0f).accepted());
    for (int k = 0; k < 4; ++k) sync_runtime();
    assert(adapter.patchNeedsDiscardConfirmation());
    clickBtn(app.contentObject(), "< PAGE", app);
    clickBtn(app.contentObject(), "128 ", app, true);
    clickBtn(app.contentObject(), "CARREGAR JA", app);
    assert(findLabel(app.contentObject(), "Unsaved changes to the current patch will be lost.") != nullptr);
    clickBtn(app.contentObject(), "CANCEL", app);
    for (int k = 0; k < 4; ++k) sync_runtime();
    assert(runtime.snapshot().patch_id == 135);

    app.shutdown();
    std::puts("PASS: C.2-B Patch Browser selection, loading, and discard protection");

    // ── C.2-C: Sound Editor (contextual modules, FM vs Subtractive, Compare/Revert/Save As)
    assert(app.begin());
    sync_runtime();
    // Ensure patch 4 (Subtractive Juno MOVING STRINGS) is loaded cleanly
    if (adapter.patchNeedsDiscardConfirmation() || runtime.snapshot().patch_id != 4) {
        assert(adapter.confirmPatchDiscard(4, 0, 0).accepted());
        sync_runtime();
        runtime.loadPendingPatch();
        for (int k = 0; k < 4; ++k) sync_runtime();
    }
    assert(runtime.snapshot().patch_id == 4);
    assert(app.navigation().navigateTo(ScreenId::SoundEditor, 0));
    sync_runtime();
    assert(findLabel(app.contentObject(), "MOTOR: SUBTRATIVO") != nullptr);
    assert(findLabel(app.contentObject(), "OSCILADORES") != nullptr);
    capture("04_sintese_geral");

    clickBtn(app.contentObject(), "OSCILADORES", app);
    sync_runtime();
    assert(findLabel(app.contentObject(), "MIX DE OSCILADORES (SUBTRATIVO)") != nullptr);
    capture("05_sintese_subtrativo");

    clickBtn(app.contentObject(), "FILTRO / ENV", app);
    sync_runtime();
    assert(findLabel(app.contentObject(), "MOTOR FM:", true) == nullptr);
    capture("07_sintese_filtro_env");

    clickBtn(app.contentObject(), "LFO / MOD", app);
    sync_runtime();
    capture("08_sintese_lfo_mod");

    // Switch to FM patch 135 -> sidebar & modules must adapt to FM 6-OP
    assert(adapter.requestSelectPatch(135, 0, 0).accepted());
    sync_runtime();
    runtime.loadPendingPatch();
    for (int k = 0; k < 4; ++k) sync_runtime();
    assert(runtime.snapshot().patch_id == 135);
    assert(findLabel(app.contentObject(), "MOTOR: FM 6-OP") != nullptr);
    clickBtn(app.contentObject(), "FM / OPS", app);
    sync_runtime();
    capture("06_sintese_fm");

    // Edit FM depth, test module switch preservation, Compare, Save As, Revert
    assert(adapter.requestParameterFloat("fm.depth", 0.75f).accepted());
    sync_runtime();
    clickBtn(app.contentObject(), "GERAL", app);
    sync_runtime();
    clickBtn(app.contentObject(), "FM / OPS", app);
    sync_runtime();
    assert(std::abs(adapter.model().getFloat("fm.depth", 0.0f) - 0.75f) < 0.05f);

    clickBtn(app.contentObject(), "COMPARE", app);
    sync_runtime();
    assert(runtime.snapshot().edit.compare == true);
    capture("09_sintese_compare");
    clickBtn(app.contentObject(), "COMPARE", app);
    sync_runtime();
    assert(runtime.snapshot().edit.compare == false);

    clickBtn(app.contentObject(), "USER / SAVE AS", app);
    sync_runtime();
    assert(findLabel(lv_screen_active(), "USER PRESETS | SAVE ONLY WITH NOTES RELEASED") != nullptr);
    capture("10_sintese_save_as");
    clickBtn(lv_screen_active(), "CLOSE", app);
    sync_runtime();

    // Revert with confirmation dialog
    clickBtn(app.contentObject(), "REVERT", app);
    assert(findLabel(app.contentObject(), "CONFIRMAR REVERT") != nullptr);
    clickBtn(app.contentObject(), "REVERTER", app);
    for (int k = 0; k < 4; ++k) sync_runtime();
    assert(!adapter.patchNeedsDiscardConfirmation());

    app.shutdown();
    std::puts("PASS: C.2-C Sound Editor contextual modules, FM/Sub adaptation, Compare/Revert/Save As");

    // ── C.2-D: Modulation, MIDI / SMK25, Preferencias, Diagnosticos ─────────
    struct RamProfileStore final : performance::ControllerProfileStore {
        performance::ControllerProfile stored = performance::smk25Defaults();
        bool load(performance::ControllerProfile& out) override { out = stored; return true; }
        bool save(const performance::ControllerProfile& in) override { stored = in; return true; }
    } profile_store;
    adapter.setMidiInput(runtime.usbInput());
    adapter.setControllerProfileStore(profile_store);

    assert(app.begin());
    sync_runtime();

    // 11 & 12. Modulation overview + Routing Editor
    assert(app.navigation().navigateTo(ScreenId::Modulation, 0));
    sync_runtime();
    assert(findLabel(app.contentObject(), "MATRIZ DE MODULACAO", true) != nullptr);
    capture("11_modulacao_visao_geral");

    clickBtn(app.contentObject(), "ROUTING EDITOR", app);
    sync_runtime();
    assert(findLabel(app.contentObject(), "APPLY ROUTES") != nullptr);
    capture("12_modulacao_routing_editor");
    clickBtn(app.contentObject(), "CANCEL / BACK", app);
    sync_runtime();

    // 13, 14, 15. MIDI / SMK25 (Conexao, Controles/CC, MIDI Learn & Cancel)
    assert(app.navigation().navigateTo(ScreenId::MidiSettings, 0));
    sync_runtime();
    assert(findLabel(app.contentObject(), "ESTADO DO HOST USB MIDI (ESP32-P4)") != nullptr);
    capture("13_midi_conexao");

    clickBtn(app.contentObject(), "CONTROLES / CC", app);
    sync_runtime();
    assert(findLabel(app.contentObject(), "DESTINATION") != nullptr);
    capture("14_midi_controles_cc");

    clickBtn(app.contentObject(), "LEARN", app);
    sync_runtime();
    capture("15_midi_learn");
    clickBtn(app.contentObject(), "CANCEL", app);
    sync_runtime();

    // 16. Preferencias (System + Performance Locks + Storage PLANNED)
    assert(app.navigation().navigateTo(ScreenId::GlobalSettings, 0));
    sync_runtime();
    capture("16_preferencias");
    clickBtn(app.contentObject(), "PERFORMANCE", app);
    sync_runtime();
    assert(findLabel(app.contentObject(), "PERFORMANCE LOCKS DE SESSAO") != nullptr);
    clickBtn(app.contentObject(), "ARMAZENAMENTO", app);
    sync_runtime();
    assert(lv_obj_has_state(findButton(app.contentObject(), "EXPORT BACKUP (PLANNED)"), LV_STATE_DISABLED));

    // 17. Diagnosticos (Audio, Memoria, USB/MIDI, Sistema + in-place refresh)
    assert(app.navigation().navigateTo(ScreenId::Diagnostics, 0));
    sync_runtime();
    assert(findLabel(app.contentObject(), "TELEMETRIA DE AUDIO & AMY DSP") != nullptr);
    capture("17_diagnosticos");
    clickBtn(app.contentObject(), "MEMORIA", app);
    sync_runtime();
    assert(findLabel(app.contentObject(), "MEMORIA LVGL, HEAP INTERNO & PSRAM") != nullptr);
    for (int i = 0; i < 10; ++i) sync_runtime(); // periodic refresh without rebuild

    std::puts("PASS: C.2-D Modulation, MIDI/SMK25, Preferencias & Diagnosticos");

    // ── C.2-E: Cross-feature safety & LVGL Stress ───────────────────────────
    // 14 & 15 & 16: USB sustained note + Sustain CC64 + Seq active during area navigation, then Arp + PANIC in overlay
    const uint8_t usb_c4[4] = {0x09, 0x90, 60, 100};
    const uint8_t sus_on[4] = {0x0B, 0xB0, 64, 127};
    assert(runtime.usbInput().transfer(usb_c4, sizeof(usb_c4), preview::g_sim_tick_ms));
    assert(runtime.usbInput().transfer(sus_on, sizeof(sus_on), preview::g_sim_tick_ms));
    sync_runtime();
    assert(runtime.snapshot().active_notes >= 1 && runtime.snapshot().sustain_active);
    seq::Request seq_play{};
    seq_play.action = seq::Action::Play;
    assert(adapter.requestSequencer(seq_play).accepted());
    sync_runtime();

    lv_obj_t* footer = lv_obj_get_child(app.rootObject(), 2);
    clickBtn(footer, "TIMBRES", app);
    clickBtn(footer, "MAIS", app);
    clickBtn(footer, "FX", app);
    clickBtn(footer, "TOCAR", app);
    sync_runtime();
    assert(runtime.snapshot().active_notes >= 1 && runtime.snapshot().sustain_active);
    assert(runtime.snapshot().sequencer.playing == true);

    assert(adapter.requestParameterBool("arp.enable", true).accepted());
    sync_runtime();
    clickBtn(footer, "TIMBRES", app);
    clickBtn(footer, "TOCAR", app);
    sync_runtime();
    assert(runtime.snapshot().arp.config.enabled == true);
    assert(runtime.snapshot().sequencer.playing == true);

    // PANIC accessible with Harmony overlay open
    app.statusStrip().openHarmony(adapter);
    preview::pumpFrame(app, 25);
    clickBtn(app.rootObject(), "PANIC", app);
    sync_runtime();
    assert(runtime.snapshot().active_notes == 0);
    app.statusStrip().closeOverlays();
    preview::pumpFrame(app, 25);

    // Warmup before LVGL stress
    for (const char* area : {"TIMBRES", "RITMO", "FX", "MAIS", "TOCAR"}) {
        clickBtn(footer, area, app);
    }
    assert(app.navigation().navigateTo(ScreenId::SoundEditor, 0));
    sync_runtime();
    assert(app.navigation().navigateTo(ScreenId::Performance, 0));
    sync_runtime();

    lv_mem_monitor_t mem_before{}, mem_after{};
    const uint32_t objs_before = countObjects(app.rootObject());
    lv_mem_monitor(&mem_before);

    // 500 area switches
    const char* kAreas[5] = {"TIMBRES", "RITMO", "FX", "MAIS", "TOCAR"};
    for (int i = 0; i < 500; ++i) {
        clickBtn(footer, kAreas[i % 5], app);
        assert(lv_mem_test() == LV_RESULT_OK);
    }

    // 500 contextual module switches (Settings + Diagnostics + Midi)
    assert(app.navigation().navigateTo(ScreenId::GlobalSettings, 0));
    preview::pumpFrame(app, 10);
    for (int i = 0; i < 500; ++i) {
        assert(app.navigation().navigateTo(ScreenId::GlobalSettings, uint8_t(i % 4)));
        preview::pumpFrame(app, 10);
    }

    // 200 synthesis panel switches
    for (int i = 0; i < 200; ++i) {
        assert(app.navigation().navigateTo(ScreenId::SoundEditor, uint8_t(i % 5)));
        preview::pumpFrame(app, 10);
    }

    // 100 MIDI Learn enter/exit
    assert(app.navigation().navigateTo(ScreenId::MidiSettings, 1));
    preview::pumpFrame(app, 10);
    for (int i = 0; i < 100; ++i) {
        adapter.midiPerformanceAction(IUiBackend::MidiAction::Learn, 0);
        preview::pumpFrame(app, 10);
        adapter.midiPerformanceAction(IUiBackend::MidiAction::Cancel, 0);
        preview::pumpFrame(app, 10);
    }

    // 100 dialog open/close
    assert(app.navigation().navigateTo(ScreenId::Performance, 5));
    preview::pumpFrame(app, 10);
    for (int i = 0; i < 100; ++i) {
        clickBtn(app.contentObject(), "LIMPAR", app);
        clickBtn(app.contentObject(), "CANCELAR", app);
    }

    // Periodic diagnostics refresh
    assert(app.navigation().navigateTo(ScreenId::Diagnostics, 1));
    for (int i = 0; i < 50; ++i) sync_runtime();

    assert(app.navigation().navigateTo(ScreenId::Performance, 0));
    sync_runtime();
    lv_mem_monitor(&mem_after);
    const uint32_t objs_after = countObjects(app.rootObject());

    std::printf(
        "STRESS_018C2: objs=%u->%u free=%zu->%zu largest=%zu->%zu diff=%zd peak=%zu frag=%u%%->%u%%\n",
        unsigned(objs_before), unsigned(objs_after),
        mem_before.free_size, mem_after.free_size,
        mem_before.free_biggest_size, mem_after.free_biggest_size,
        static_cast<ssize_t>(mem_before.free_size) - static_cast<ssize_t>(mem_after.free_size),
        mem_after.max_used, mem_before.frag_pct, mem_after.frag_pct);
    std::fflush(stdout);

    assert(objs_before == objs_after);
    assert(mem_after.free_size >= mem_before.free_size);
    assert(mem_after.frag_pct <= 5);

    // ── C.2a: 32 FM Algorithms Geometry, FM/MIDI Ergonomics, Lifecycle & 16 Captures ──
    auto rectsOverlap = [](const lv_area_t& a, const lv_area_t& b) {
        return a.x1 <= b.x2 && a.x2 >= b.x1 && a.y1 <= b.y2 && a.y2 >= b.y1;
    };
    auto auditCardGeometry = [&](lv_obj_t* root) {
        lv_obj_update_layout(root);
        auto walk = [&](auto& self, lv_obj_t* obj) -> void {
            if (!obj || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
            lv_area_t oc{};
            lv_obj_get_coords(obj, &oc);
            assert(oc.x1 >= 0 && oc.y1 >= 0 && oc.x2 < 800 && oc.y2 < 480);
            if (obj != root) {
                lv_area_t pc{};
                lv_obj_get_coords(lv_obj_get_parent(obj), &pc);
                if (!(oc.x1 >= pc.x1 && oc.y1 >= pc.y1 && oc.x2 <= pc.x2 && oc.y2 <= pc.y2)) {
                    std::printf("GEOMETRY_FAIL child=(%ld,%ld,%ld,%ld) parent=(%ld,%ld,%ld,%ld) text=%s\n",
                                long(oc.x1),long(oc.y1),long(oc.x2),long(oc.y2),
                                long(pc.x1),long(pc.y1),long(pc.x2),long(pc.y2),
                                lv_obj_check_type(obj,&lv_label_class) ? lv_label_get_text(obj) : "[object]");
                    std::fflush(stdout);
                    assert(false);
                }
            }

            // Check no badge overlaps sibling labels inside the same card
            const uint32_t cnt = lv_obj_get_child_count(obj);
            lv_obj_t* badge = nullptr;
            for (uint32_t i = 0; i < cnt; ++i) {
                lv_obj_t* ch = lv_obj_get_child(obj, int32_t(i));
                if (lv_obj_get_child_count(ch) == 1 && !lv_obj_check_type(ch, &lv_button_class)) {
                    lv_obj_t* inner = lv_obj_get_child(ch, 0);
                    if (lv_obj_check_type(inner, &lv_label_class)) {
                        const char* txt = lv_label_get_text(inner);
                        if (txt && (std::strcmp(txt, "AVAIL") == 0 || std::strcmp(txt, "PLAN") == 0 ||
                                    std::strcmp(txt, "N/A") == 0 || std::strcmp(txt, "DEMO") == 0)) {
                            badge = ch;
                        }
                    }
                }
            }
            if (badge) {
                lv_area_t bc{};
                lv_obj_get_coords(badge, &bc);
                assert(bc.x1 >= oc.x1 && bc.x2 <= oc.x2 && bc.y1 >= oc.y1 && bc.y2 <= oc.y2);
                for (uint32_t i = 0; i < cnt; ++i) {
                    lv_obj_t* ch = lv_obj_get_child(obj, int32_t(i));
                    if (ch != badge && lv_obj_check_type(ch, &lv_label_class)) {
                        lv_area_t lc{};
                        lv_obj_get_coords(ch, &lc);
                        assert(!rectsOverlap(bc, lc));
                    }
                }
            }
            for (uint32_t i = 0; i < cnt; ++i) {
                self(self, lv_obj_get_child(obj, int32_t(i)));
            }
        };
        walk(walk, root);
    };

    // 1. Verify all 32 FM algorithms in both compact (614x66) and expanded (706x258) layouts
    for (int algo = 1; algo <= 32; ++algo) {
        const auto topo = smk::p4::fm::deriveTopology(algo);
        assert(topo.carrierCount() >= 1);
        assert(topo.carrierCount() + topo.modulatorCount() == 6);
        for (int16_t mode = 0; mode < 2; ++mode) {
            const int16_t aw = (mode == 0) ? 614 : 706;
            const int16_t ah = (mode == 0) ? 66 : 258;
            const auto g = smk::p4::fm::generateLayout(topo, aw, ah);
            for (unsigned i = 0; i < 6; ++i) {
                assert(g.nodes[i].op == 6 - i);
                assert(g.nodes[i].x >= 0 && g.nodes[i].y >= 0);
                assert(g.nodes[i].x + g.nodes[i].w <= aw);
                assert(g.nodes[i].y + g.nodes[i].h <= ah);
            }
            assert(g.out_node.x >= 0 && g.out_node.y >= 0);
            assert(g.out_node.x + g.out_node.w <= aw);
            assert(g.out_node.y + g.out_node.h <= ah);
            assert(g.out_node.x > g.nodes[5].x + g.nodes[5].w);
            unsigned expected_edges = topo.carrierCount() + (topo.feedback_in != 0 ? 1 : 0);
            bool modulation_seen[6][6]{};
            bool out_seen[6]{};
            unsigned feedback_seen = 0;
            for (unsigned src = 1; src <= 6; ++src)
                for (unsigned dst = 1; dst <= 6; ++dst)
                    if (topo.connectsTo(src, dst)) ++expected_edges;
            assert(g.edge_count == expected_edges);
            for (unsigned e = 0; e < g.edge_count; ++e) {
                const auto& edge = g.edges[e];
                assert(edge.src_op >= 1 && edge.src_op <= 6);
                if (edge.type == smk::p4::fm::EdgeType::Modulation) {
                    assert(topo.connectsTo(edge.src_op, edge.dst_op));
                    assert(!modulation_seen[edge.src_op - 1][edge.dst_op - 1]);
                    modulation_seen[edge.src_op - 1][edge.dst_op - 1] = true;
                } else if (edge.type == smk::p4::fm::EdgeType::AudioOut) {
                    assert(edge.dst_op == 0 && topo.isCarrier(edge.src_op));
                    assert(!out_seen[edge.src_op - 1]);
                    out_seen[edge.src_op - 1] = true;
                } else {
                    assert(edge.src_op == topo.feedback_out && edge.dst_op == topo.feedback_in);
                    ++feedback_seen;
                }
                assert(g.edges[e].point_count >= 2 && g.edges[e].point_count <= 4);
                for (uint8_t p = 0; p < g.edges[e].point_count; ++p) {
                    assert(g.edges[e].points[p].x >= 0 && g.edges[e].points[p].x <= aw);
                    assert(g.edges[e].points[p].y >= 0 && g.edges[e].points[p].y <= ah);
                }
            }
            assert(feedback_seen == (topo.feedback_in != 0 ? 1u : 0u));
        }
    }
    std::puts("PASS: C.2a 32 FM algorithms compact & expanded geometry verified");

    // 2. FM Editor captures & operator/algorithm verification
    assert(adapter.requestSelectPatch(135, 0, 0).accepted());
    sync_runtime();
    runtime.loadPendingPatch();
    for (int k = 0; k < 4; ++k) sync_runtime();
    assert(app.navigation().navigateTo(ScreenId::SoundEditor, 1));
    sync_runtime();
    auditCardGeometry(app.contentObject());

    // Exercise real mounted widgets for every topology and every selection.
    // The summary and expanded graph must always highlight confirmed state.
    for (int algo = 1; algo <= 32; ++algo) {
        assert(adapter.requestParameterInt("fm.algo", algo).accepted());
        sync_runtime();
        assert(adapter.model().getInt("fm.algo", 0) == algo);
        clickBtn(app.contentObject(), "EXPAND", app);
        sync_runtime();
        for (int op = 1; op <= 6; ++op) {
            char label[16];
            std::snprintf(label, sizeof(label), "OP %d", op);
            auto* node_label = findLabel(lv_screen_active(), label, true);
            assert(node_label);
            // The editor's six top selectors have the same prefix; send the
            // command through the adapter and inspect all graph node borders.
            assert(adapter.requestParameterInt("fm.selected_op", op).accepted());
            sync_runtime();
            assert(adapter.model().getInt("fm.selected_op", 0) == op);
            auto* header = findLabel(lv_screen_active(), "FM TOPOLOGY:", true);
            assert(header);
            auto* modal = lv_obj_get_parent(header);
            auto* selected = findLabel(modal, label, true);
            assert(selected);
            assert(lv_obj_get_style_border_width(lv_obj_get_parent(selected), 0) == 3);
            auditCardGeometry(modal);
        }
        clickBtn(lv_screen_active(), "CLOSE", app);
    }

    clickBtn(app.contentObject(), "OP 1", app);
    sync_runtime();
    assert(adapter.model().getInt("fm.selected_op", 1) == 1);
    capture("02_fm_op1");

    clickBtn(app.contentObject(), "OP 6", app);
    sync_runtime();
    assert(adapter.model().getInt("fm.selected_op", 1) == 6);
    capture("03_fm_op6");

    clickBtn(app.contentObject(), "GLOBAL", app);
    sync_runtime();
    auditCardGeometry(app.contentObject());
    capture("01_fm_visao_global");
    clickBtn(app.contentObject(), "OPERADOR", app);
    sync_runtime();

    for (const auto& algo_shot : std::initializer_list<std::pair<int, const char*>>{
             {1, "04_fm_algo_01"}, {9, "05_fm_algo_09"}, {16, "06_fm_algo_16"}, {32, "07_fm_algo_32"}}) {
        assert(adapter.requestParameterInt("fm.algo", algo_shot.first).accepted());
        sync_runtime();
        auditCardGeometry(app.contentObject());
        capture(algo_shot.second);
    }

    clickBtn(app.contentObject(), "EXPAND", app);
    sync_runtime();
    capture("08_fm_topologia_expandida");
    clickBtn(lv_screen_active(), "CLOSE", app);
    sync_runtime();

    clickBtn(app.contentObject(), "ALGO ", app, true);
    sync_runtime();
    capture("09_fm_selecao_algoritmo");

    // Held note causes algorithm change rejection in modal
    const uint8_t hold_note[4] = {0x09, 0x90, 62, 100};
    assert(runtime.usbInput().transfer(hold_note, sizeof(hold_note), preview::g_sim_tick_ms));
    sync_runtime();
    clickBtn(lv_screen_active(), "05", app);
    sync_runtime();
    assert(adapter.model().getInt("fm.algo", 0) == 32);
    assert(adapter.fmAlgorithmResult().state == UiCommandState::Rejected);
    capture("10_fm_mudanca_rejeitada");
    clickBtn(lv_screen_active(), "PANIC", app);
    sync_runtime();
    closeFmModals(false);
    preview::pumpFrame(app, 25);

    clickBtn(app.contentObject(), "USER / SAVE AS", app);
    sync_runtime();
    capture("16_user_presets_save_as");
    clickBtn(lv_screen_active(), "CLOSE", app);
    sync_runtime();

    // 3. MIDI Learn ergonomics & captures
    assert(app.navigation().navigateTo(ScreenId::MidiSettings, 1));
    sync_runtime();
    auditCardGeometry(app.contentObject());
    capture("11_midi_lista_knobs");

    for (unsigned knob = 1; knob <= 8; ++knob) {
        char row[8], header[40];
        std::snprintf(row, sizeof(row), "K%u ", knob);
        std::snprintf(header, sizeof(header), "KNOB K%u SELECIONADO", knob);
        clickBtn(app.contentObject(), row, app, true);
        assert(findLabel(app.contentObject(), header));
    }

    clickBtn(app.contentObject(), "K3 ", app, true);
    sync_runtime();
    assert(findLabel(app.contentObject(), "KNOB K3 SELECIONADO") != nullptr);
    capture("12_midi_knob_selecionado");

    // Simulate USB connected + MIDI Learn waiting + CC received + Confirm
    sim_usb_connected = true;
    sync_runtime();
    clickBtn(app.contentObject(), "LEARN", app);
    sync_runtime();
    capture("13_midi_learn_aguardando");

    const uint8_t cc_msg[4] = {0x0B, 0xB0, 74, 90};
    assert(runtime.usbInput().transfer(cc_msg, sizeof(cc_msg), preview::g_sim_tick_ms));
    sync_runtime();
    clickBtn(app.contentObject(), "CONFIRM K3", app);
    sync_runtime();
    assert(std::strstr(adapter.midiLearnStatus(), "K3 BOUND TO CH1 CC74"));
    capture("14_midi_associacao_confirmada");

    clickBtn(app.contentObject(), "DESTINATION", app);
    sync_runtime();
    capture("15_midi_destino_controle");
    clickBtn(app.contentObject(), "CANCEL / BACK", app);
    sync_runtime();

    clickBtn(app.contentObject(), "LEARN + CC1", app);
    sync_runtime();
    const uint8_t cc1_msg[4] = {0x0B, 0xB0, 1, 55};
    assert(runtime.usbInput().transfer(cc1_msg, sizeof(cc1_msg), preview::g_sim_tick_ms));
    sync_runtime();
    clickBtn(app.contentObject(), "CONFIRM K3", app);
    sync_runtime();
    assert(std::strstr(adapter.midiLearnStatus(), "K3 BOUND TO CH1 CC1"));
    clickBtn(app.contentObject(), "LEARN", app);
    clickBtn(app.contentObject(), "K4 ", app, true);
    assert(std::strstr(adapter.midiLearnStatus(), "CANCELLED"));
    clickBtn(app.contentObject(), "LEARN", app);
    clickBtn(app.contentObject(), "TOCAR >", app);
    sync_runtime();
    assert(app.navigation().currentScreen() == ScreenId::Performance);
    assert(std::strstr(adapter.midiLearnStatus(), "CANCELLED"));

    // Simulated disconnect triggers Panic cleanly
    sim_usb_connected = false;
    runtime.usbInput().disconnect();
    sync_runtime();
    assert(runtime.snapshot().active_notes == 0);

    // 4. Lifecycle stress: 200 FM modal cycles, 100 User Presets cycles, 100 MIDI Learn cycles
    assert(app.navigation().navigateTo(ScreenId::SoundEditor, 1));
    sync_runtime();
    for (int i = 0; i < 200; ++i) {
        clickBtn(app.contentObject(), (i % 2 == 0) ? "EXPAND" : "ALGO ", app, i % 2 != 0);
        clickBtn(lv_screen_active(), "CLOSE", app);
    }
    for (int i = 0; i < 100; ++i) {
        clickBtn(app.contentObject(), "USER / SAVE AS", app);
        clickBtn(lv_screen_active(), "CLOSE", app);
    }
    assert(app.navigation().navigateTo(ScreenId::MidiSettings, 1));
    sync_runtime();
    for (int i = 0; i < 100; ++i) {
        clickBtn(app.contentObject(), "LEARN", app);
        clickBtn(app.contentObject(), "CANCEL", app);
    }
    assert(app.navigation().navigateTo(ScreenId::Performance, 0));
    sync_runtime();
    lv_mem_monitor_t mem_c2a{};
    lv_mem_monitor(&mem_c2a);
    const uint32_t objs_c2a = countObjects(app.rootObject());
    std::printf("STRESS_018C2A: objs=%u->%u free=%zu->%zu diff=%zd peak=%zu frag=%u%%\n",
                unsigned(objs_before), unsigned(objs_c2a),
                mem_before.free_size, mem_c2a.free_size,
                static_cast<ssize_t>(mem_before.free_size) - static_cast<ssize_t>(mem_c2a.free_size),
                mem_c2a.max_used, mem_c2a.frag_pct);
    assert(objs_c2a == objs_before);
    assert(mem_c2a.free_size >= mem_before.free_size);
    assert(mem_c2a.frag_pct <= 5);

    app.shutdown();
    std::puts("ALL 0.18-C.2 TIMBRES & MAIS TESTS AND STRESS PASSED.");
    return 0;
}
