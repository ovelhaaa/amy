#include "ui_screens.h"
#include "ui_widgets.h"
#include <cstdio>

namespace smk::p4::ui {

namespace {

class ArpeggiatorScreen final : public ScreenView {
public:
    ScreenId id() const override { return ScreenId::Arpeggiator; }

    bool build(lv_obj_t* parent, IUiBackend& backend, UiNavigationManager& nav) override {
        // 1. Row 1: Enable, Latch, Tempo BPM, Clock Source (y = 4..104)
        createParameterCard(parent, backend, "arp.enable",       8,   4, 190, 98);
        createParameterCard(parent, backend, "arp.latch",        206, 4, 190, 98);
        createParameterCard(parent, backend, "perf.bpm",         404, 4, 190, 98);
        createParameterCard(parent, backend, "arp.clock_source", 602, 4, 190, 98);

        // 2. Row 2: Rate, Direction, Octave Range, Gate Length, Swing (y = 108..216)
        createParameterCard(parent, backend, "arp.rate",      8,   108, 152, 106);
        createParameterCard(parent, backend, "arp.direction", 166, 108, 152, 106);
        createParameterCard(parent, backend, "arp.octaves",   324, 108, 152, 106);
        createParameterCard(parent, backend, "arp.gate_pct",  482, 108, 152, 106);
        createParameterCard(parent, backend, "arp.swing_pct", 640, 108, 152, 106);

        // 3. Visual Arpeggiator Pattern & Step Indicator (y = 222..386)
        lv_obj_t* viz = lv_obj_create(parent);
        applyPanelStyle(viz, true);
        lv_obj_set_pos(viz, 8, 222);
        lv_obj_set_size(viz, 784, 54);
        lv_obj_set_style_pad_all(viz, 6, 0);

        createLabel(viz,
                    "ARP VISUALIZER (DEMO)  |  S3 ARPEGGIATOR ENGINE EXISTS (PLANNED FOR P4 RUNTIME)",
                    6, 4, 760, &lv_font_montserrat_14, palette::kAccentPrimary);
        createLabel(viz,
                    "Tap any step below to preview gate rhythm in the UI model. No real MIDI notes are emitted.",
                    6, 24, 760, &lv_font_montserrat_14, palette::kTextSecondary);

        createStepGrid(parent, backend, nav, StepGridKind::ArpPreview, 8, 282, 784, 104);
        return true;
    }

    void refresh(IUiBackend&) override {}
};

} // namespace

ScreenView* createArpeggiatorScreen() {
    return new ArpeggiatorScreen();
}

} // namespace smk::p4::ui
