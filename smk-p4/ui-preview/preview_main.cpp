#include "preview_harness.h"
#include <cstdio>

using namespace smk::p4::ui;
using namespace smk::p4::ui::preview;

int main(int argc, char** argv) {
    const char* out_dir = (argc > 1 && argv[1] && argv[1][0] != '\0') ? argv[1] : "build/ui_preview_shots";
    initHostLvgl();
    UiDemoBackend backend;
    UiBlueprint app(backend);
    if (!app.begin()) {
        std::fprintf(stderr, "FAIL: UiBlueprint::begin() failed\n");
        return 1;
    }

    struct CaptureSpec {
        ScreenId    screen;
        uint8_t     sub_tab;
        const char* filename;
    };

    static constexpr CaptureSpec kCaptures[] = {
        {ScreenId::Performance,    0, "01_performance_macros.ppm"},
        {ScreenId::Performance,    1, "01b_performance_keys.ppm"},
        {ScreenId::PatchBrowser,   0, "02_patch_browser.ppm"},
        {ScreenId::SoundEditor,    0, "03a_sound_editor_osc.ppm"},
        {ScreenId::SoundEditor,    1, "03b_sound_editor_fm.ppm"},
        {ScreenId::SoundEditor,    2, "03c_sound_editor_filter.ppm"},
        {ScreenId::SoundEditor,    4, "03d_sound_editor_env.ppm"},
        {ScreenId::Layers,         0, "04_layers_split.ppm"},
        {ScreenId::Mixer,          0, "05_mixer.ppm"},
        {ScreenId::Modulation,     0, "06_modulation.ppm"},
        {ScreenId::Effects,        0, "07_effects.ppm"},
        {ScreenId::Arpeggiator,    0, "08_arpeggiator.ppm"},
        {ScreenId::Sequencer,      0, "09_sequencer.ppm"},
        {ScreenId::Drums,          0, "10_drums.ppm"},
        {ScreenId::Harmony,        0, "11a_harmony_scale.ppm"},
        {ScreenId::Harmony,        1, "11b_harmony_chord.ppm"},
        {ScreenId::MidiSettings,   0, "12a_midi_channels.ppm"},
        {ScreenId::MidiSettings,   1, "12b_midi_cc_learn.ppm"},
        {ScreenId::GlobalSettings, 0, "13_global_settings.ppm"},
        {ScreenId::Diagnostics,    0, "14_diagnostics.ppm"}
    };

    // Trigger a non-scale note (C#4 = 61) so Harmony visualizer shows quantization & chord tones
    backend.triggerNoteOn(61, 100);

    for (const auto& cap : kCaptures) {
        if (!app.navigation().navigateTo(cap.screen, cap.sub_tab)) {
            std::fprintf(stderr, "FAIL: navigateTo(%u, %u)\n", unsigned(cap.screen), unsigned(cap.sub_tab));
            return 1;
        }
        pumpFrame(app, 25);
        char path[512];
        std::snprintf(path, sizeof(path), "%s/%s", out_dir, cap.filename);
        if (!saveFramebufferPpm(path)) {
            std::fprintf(stderr, "FAIL: could not write %s\n", path);
            return 1;
        }
        std::printf("CAPTURED: %s\n", path);
    }

    app.shutdown();
    std::puts("PASS: all 20 screen/tab preview frames rendered at 800x480 RGB565");
    return 0;
}
