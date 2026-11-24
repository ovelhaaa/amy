#include "fake_nvs.h"
#include "pattern_nvs.h"
#include "performance_scene_nvs.h"
#include "ui_blueprint_performance_adapter.h"
#include "local_performance_backend.h"
#include "performance_factory_routes.h"
#include "../ui-preview/preview_harness.h"
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

    app.shutdown();
    std::puts("ALL 0.18-C.2 TIMBRES & MAIS TESTS AND STRESS PASSED.");
    return 0;
}
