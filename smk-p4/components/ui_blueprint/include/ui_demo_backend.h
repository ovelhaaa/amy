#pragma once
#include "ui_model.h"
#include <cstdint>

namespace smk::p4::ui {

// Purely in-memory demonstrative backend for the SMK-P4 UI Blueprint.
// Never initializes or touches AMY, I2S, USB-MIDI, NVS, or UART.
class UiDemoBackend {
public:
    UiDemoBackend();

    UiModel& model() { return model_; }
    const UiModel& model() const { return model_; }

    void service(uint32_t now_ms);

    // Explicit demo flag — always true for UiDemoBackend
    bool isDemoMode() const { return true; }
    const char* modeBadgeText() const { return "UI DEMO"; }

    // Musical demo actions (strictly RAM-only)
    void triggerNoteOn(uint8_t note, uint8_t velocity);
    void triggerNoteOff(uint8_t note);
    void triggerPanic();

    // Simulated level meters (explicitly tagged as demo simulation, not real audio)
    struct SimulatedMeters {
        bool    is_simulated = true;
        uint8_t layer_a_l    = 0;
        uint8_t layer_a_r    = 0;
        uint8_t layer_b_l    = 0;
        uint8_t layer_b_r    = 0;
        uint8_t drums_l      = 0;
        uint8_t drums_r      = 0;
        uint8_t master_l     = 0;
        uint8_t master_r     = 0;
    };
    const SimulatedMeters& meters() const { return meters_; }

    uint8_t activeDemoStep() const { return active_demo_step_; }
    bool isNoteHeld(uint8_t note) const { return (note < 128) ? held_notes_[note] : false; }

private:
    void recomputeMeters();

    UiModel         model_{};
    SimulatedMeters meters_{};
    bool            held_notes_[128]{};
    uint8_t         active_demo_step_ = 0;
    uint32_t        last_step_ms_     = 0;
};

} // namespace smk::p4::ui
