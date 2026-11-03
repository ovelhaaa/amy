#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>

namespace smk::p4::ui {

namespace {

constexpr const char* kHarmonyTabs[2] = {
    "SCALE QUANTIZER & KEYBOARD",
    "CHORD ASSIST & VOICING"
};

constexpr const char* kPitchNames[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

void formatMidiNote(uint8_t note, char* out, size_t cap) {
    std::snprintf(out, cap, "%s%d (%u)", kPitchNames[note % 12], int(note / 12) - 1, note);
}

class HarmonyScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::Harmony; }

    bool build(lv_obj_t* parent, UiDemoBackend& backend, UiNavigationManager& nav) override {
        const uint8_t tab = nav.subTab(ScreenId::Harmony) % 2;

        // 1. Sub-tab selector (y = 4..40)
        createSubTabBar(parent, kHarmonyTabs, 2, tab, ScreenId::Harmony, nav, 8, 4, 480, 36);

        // Quick Root Note badge + DEMO calculation indicator
        lv_obj_t* top_badge = lv_obj_create(parent);
        applyPanelStyle(top_badge, true);
        lv_obj_set_pos(top_badge, 496, 4);
        lv_obj_set_size(top_badge, 296, 36);
        lv_obj_set_style_pad_all(top_badge, 4, 0);
        createLabel(top_badge, "DEMO PREVIEW (NO LIVE MIDI TX)", 6, 4, 280,
                    &lv_font_montserrat_14, palette::kAccentPrimary);

        // 2. Controls Row (y = 44..220)
        if (tab == 0) {
            // Scale Quantizer Controls
            createParameterCard(parent, backend, "harmony.scale_enable",     8,   44, 190, 86);
            createParameterCard(parent, backend, "harmony.root_note",        206, 44, 190, 86);
            createParameterCard(parent, backend, "harmony.scale_type",       404, 44, 190, 86);
            createParameterCard(parent, backend, "harmony.quant_mode",       602, 44, 190, 86);

            createParameterCard(parent, backend, "harmony.preserve_outside", 8,   134, 190, 86);
            createParameterCard(parent, backend, "harmony.chord_enable",     206, 134, 190, 86);
            createParameterCard(parent, backend, "harmony.chord_type",       404, 134, 190, 86);
            createParameterCard(parent, backend, "harmony.dest_layer",       602, 134, 190, 86);
        } else {
            // Chord Assist Detailed Voicing Controls
            createParameterCard(parent, backend, "harmony.chord_enable", 8,   44, 190, 86);
            createParameterCard(parent, backend, "harmony.root_note",    206, 44, 190, 86);
            createParameterCard(parent, backend, "harmony.chord_type",   404, 44, 190, 86);
            createParameterCard(parent, backend, "harmony.voicing",      602, 44, 190, 86);

            createParameterCard(parent, backend, "harmony.inversion",    8,   134, 190, 86);
            createParameterCard(parent, backend, "harmony.spread",       206, 134, 190, 86);
            createParameterCard(parent, backend, "harmony.vel_behavior", 404, 134, 190, 86);
            createParameterCard(parent, backend, "harmony.dest_layer",   602, 134, 190, 86);
        }

        // 3. Live Harmony Calculation Inspector Strip (y = 224..266)
        const HarmonyPreviewResult harm = backend.model().computeHarmony(backend.model().lastPreviewNote());
        lv_obj_t* insp = lv_obj_create(parent);
        applyPanelStyle(insp, false);
        lv_obj_set_pos(insp, 8, 224);
        lv_obj_set_size(insp, 784, 42);
        lv_obj_set_style_pad_all(insp, 4, 0);

        char in_s[20], q_s[20], chord_s[96]{};
        formatMidiNote(harm.input_note, in_s, sizeof(in_s));
        formatMidiNote(harm.quantized_note, q_s, sizeof(q_s));

        size_t pos = 0;
        for (uint8_t i = 0; i < harm.chord_count; ++i) {
            const uint8_t cn = harm.chord_notes[i];
            const int written = std::snprintf(
                chord_s + pos, sizeof(chord_s) - pos, "%s%s%d",
                (i > 0) ? " + " : "", kPitchNames[cn % 12], int(cn / 12) - 1);
            if (written > 0) pos += static_cast<size_t>(written);
        }

        char summary[160];
        std::snprintf(summary, sizeof(summary),
                      "IN: %s [%s]  ->  QUANTIZED: %s  ->  CHORD (%u): %s   [TAP KEYS BELOW TO TEST]",
                      in_s, harm.input_in_scale ? "IN SCALE" : "CORRECTED",
                      q_s, harm.chord_count, chord_s);
        createLabel(insp, summary, 6, 6, 768, &lv_font_montserrat_14,
                    harm.input_in_scale ? palette::kActive : palette::kAccentPrimary);

        // 4. 2-Octave Harmony Visualizer Touch Keyboard (y = 270..388)
        // Colors: Green = Quantized Root, Amber = Generated Chord Tones, Red = Out-of-Scale Input, Light Blue = Scale Degrees
        createVirtualKeyboard(parent, backend, nav, KeyboardDisplayMode::HarmonyView, 8, 270, 784, 118);

        return true;
    }

    void refresh(UiDemoBackend&) override {}
};

} // namespace

ScreenView* createHarmonyScreen() {
    return new HarmonyScreen();
}

} // namespace smk::p4::ui
