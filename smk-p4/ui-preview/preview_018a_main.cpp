#include "ui_prototype_018a.h"
#include "preview_harness.h"
#include "ui_demo_backend.h"
#include "ui_blueprint_performance_adapter.h"
#include "local_performance_backend.h"
#include "fake_nvs.h"
#include "pattern_nvs.h"
#include "performance_scene_nvs.h"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace smk::p4;
using namespace smk::p4::ui;
using namespace smk::p4::ui::proto018a;

namespace {

bool captureScreen(const char* out_dir, const char* filename) {
    char full_path[512];
    std::snprintf(full_path, sizeof(full_path), "%s/%s.ppm", out_dir, filename);
    const bool ok = preview::saveFramebufferPpm(full_path);
    if (ok) {
        std::printf("CAPTURED: %s\n", full_path);
    }
    return ok;
}

}  // namespace

int main(int argc, char** argv) {
    const char* out_dir = (argc > 1 && argv[1] && argv[1][0]) ? argv[1] : "build/ui_018a_shots";
    preview::initHostLvgl();

    // 1. Live Adapter Captures (real AMY Runtime + LocalAmyPerformanceBackend)
    {
        amy::Runtime runtime;
        assert(runtime.begin());
        LocalAmyPerformanceBackend perf_backend(runtime.commands());
        UiBlueprintPerformanceAdapter adapter(perf_backend);

        NvsSceneStore scenes;
        assert(scenes.begin());
        scene::Slot sc0{};
        sc0.state = seq::SlotState::Valid;
        std::strcpy(sc0.config.name, "INTRO PAD");
        sc0.config.domains = scene::Patch | scene::Fx | scene::Harmony | scene::Macros;
        sc0.config.patch_id = runtime.snapshot().patch_id;
        sc0.config.harmony = runtime.snapshot().harmony;
        assert(scenes.commit(0, sc0));

        scene::Slot sc1{};
        sc1.state = seq::SlotState::Valid;
        std::strcpy(sc1.config.name, "CHORUS LD");
        sc1.config.domains = scene::Patch | scene::Arp | scene::Macros;
        sc1.config.patch_id = 128;
        assert(scenes.commit(1, sc1));
        adapter.setSceneStore(scenes);

        UiShell018A shell(adapter);
        assert(shell.begin());

        auto step = [&]() {
            preview::g_sim_tick_ms += 20;
            runtime.apply(preview::g_sim_tick_ms);
            runtime.loadPendingPatch();
            assert(runtime.render());
            perf_backend.publish(runtime.snapshot(), true, 0, 0);
            perf_backend.publishMidi(true, false, runtime.usbInput().events.load());
            shell.service(preview::g_sim_tick_ms);
            lv_timer_handler();
            lv_refr_now(nullptr);
        };

        step();

        // 01: Tocar + Header Variant A (800x44)
        assert(captureScreen(out_dir, "01_tocar_hdr_a_live"));

        // 02: Tocar + Header Variant B (800x68)
        shell.setHeaderVariant(HeaderVariant::ExpandedTwoRow);
        adapter.requestParameterBool("harmony.scale_enable", true);
        step();
        assert(captureScreen(out_dir, "02_tocar_hdr_b_live"));

        // Back to Header Variant A for full content shots
        shell.setHeaderVariant(HeaderVariant::CompactOverlay);
        adapter.requestParameterBool("harmony.scale_enable", false);
        step();

        // 03: Tocar Macros 5-8
        shell.selectTocarMode(TocarMode::Macros5To8);
        step();
        assert(captureScreen(out_dir, "03_tocar_macros_5_8"));

        // 04: Tocar Scenes Quick (show active slot 0)
        SceneOperation rec0{};
        rec0.action = SceneAction::Load;
        rec0.slot = 0;
        adapter.requestScene(rec0);
        shell.selectTocarMode(TocarMode::ScenesQuick);
        step();
        assert(captureScreen(out_dir, "04_tocar_scenes_states"));

        // 05: Tocar Encadear Wireframe (NAO IMPLEMENTADO)
        shell.selectTocarMode(TocarMode::SceneChainProposal);
        step();
        assert(captureScreen(out_dir, "05_tocar_encadear_plan"));

        // 06: Tocar Teclado Touch
        shell.selectTocarMode(TocarMode::VirtualKeys);
        step();
        assert(captureScreen(out_dir, "06_tocar_teclado_touch"));

        // 07: Overlay Harmonico over Tocar
        shell.selectTocarMode(TocarMode::Macros1To4);
        adapter.requestParameterBool("harmony.scale_enable", true);
        adapter.requestParameterBool("harmony.chord_enable", true);
        shell.openOverlay(OverlayKind::HarmonyContext);
        step();
        assert(captureScreen(out_dir, "07_overlay_harmony_q_ch"));

        // 08: Overlay BPM
        shell.openOverlay(OverlayKind::TempoBpm);
        step();
        assert(captureScreen(out_dir, "08_overlay_bpm"));

        // 09: Overlay Scene Selective Modal
        shell.openOverlay(OverlayKind::SceneSelectiveModal, 0);
        step();
        assert(captureScreen(out_dir, "09_overlay_scene_selective"));
        shell.closeOverlay();
        step();

        // 10: FX Delay Sync Locked + Bypass N/A
        adapter.requestParameterValue("fx.delay.mode", ParameterValue::makeEnum(1));
        step();
        shell.selectArea(AreaId::Fx);
        shell.selectFxModule(FxModule::Delay);
        step();
        assert(captureScreen(out_dir, "10_fx_delay_sync"));

        // 11: FX Reverb
        shell.selectFxModule(FxModule::Reverb);
        shell.selectParameter("fx.reverb.mix");
        step();
        assert(captureScreen(out_dir, "11_fx_reverb"));

        // 12: FX Chorus OFF
        shell.selectFxModule(FxModule::Chorus);
        step();
        assert(captureScreen(out_dir, "12_fx_chorus_off"));

        // 13: FX Master Gaps
        shell.selectFxModule(FxModule::Master);
        step();
        assert(captureScreen(out_dir, "13_fx_master_gaps"));

        // 14: Ritmo Sequenciador PLAY MODE
        seq::Request rplay{};
        rplay.action = seq::Action::ToggleStepEnabled;
        rplay.index = 0;
        adapter.requestSequencer(rplay);
        step();
        rplay.action = seq::Action::ToggleStepEnabled;
        rplay.index = 4;
        adapter.requestSequencer(rplay);
        step();
        rplay.action = seq::Action::Play;
        adapter.requestSequencer(rplay);
        step();
        rplay.action = seq::Action::SetQuantize;
        rplay.value = 1;
        adapter.requestSequencer(rplay);
        shell.selectArea(AreaId::Ritmo);
        shell.selectRitmoModule(RitmoModule::Sequenciador);
        shell.selectSeqViewMode(SeqViewMode::Play);
        step();
        assert(captureScreen(out_dir, "14_ritmo_seq_play"));

        // 15: Ritmo Sequenciador EDIT MODE
        shell.selectSeqViewMode(SeqViewMode::Edit);
        step();
        assert(captureScreen(out_dir, "15_ritmo_seq_edit"));

        // 16: Ritmo Arpejador
        adapter.requestParameterBool("arp.enable", true);
        adapter.requestArpTransport(true);
        shell.selectRitmoModule(RitmoModule::Arpejador);
        step();
        assert(captureScreen(out_dir, "16_ritmo_arp"));

        // 17: Timbres Sintese (select FM patch 128 if available)
        adapter.requestSelectPatch(128, 0);
        step();
        shell.selectArea(AreaId::Timbres);
        shell.selectTimbresModule(TimbresModule::Sintese);
        shell.selectSynthSubTab(SynthSubTab::Motor);
        step();
        assert(captureScreen(out_dir, "17_timbres_sintese_fm"));

        // 19: Mais Diagnosticos
        shell.selectArea(AreaId::Mais);
        shell.selectMaisModule(MaisModule::Diagnosticos);
        step();
        assert(captureScreen(out_dir, "19_mais_diagnosticos"));

        shell.teardown();
        lv_timer_handler();
    }

    // 2. Explicit UI Demo Backend Mode capture (18)
    {
        UiDemoBackend demo_backend;
        UiShell018A demo_shell(demo_backend);
        assert(demo_shell.begin());
        preview::g_sim_tick_ms += 20;
        demo_shell.service(preview::g_sim_tick_ms);
        lv_timer_handler();
        lv_refr_now(nullptr);
        assert(captureScreen(out_dir, "18_tocar_demo_mode"));
        demo_shell.teardown();
    }

    std::puts("PASS: all 19 0.18-A prototype preview captures generated at 800x480 RGB565.");
    return 0;
}
