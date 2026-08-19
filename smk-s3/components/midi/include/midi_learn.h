#pragma once
#include "controller_profile.h"
#include <cstdint>
#include <cstddef>

namespace smk {

enum class LearnStep : uint8_t {
    Idle = 0,
    PressKey,
    MovePitch,
    MoveModulation,
    // Bank A Knobs 1..8
    TurnKnob1,
    TurnKnob2,
    TurnKnob3,
    TurnKnob4,
    TurnKnob5,
    TurnKnob6,
    TurnKnob7,
    TurnKnob8,
    // Bank B Knobs 9..16 (after pressing KNOB-B button on controller)
    TurnKnob9,
    TurnKnob10,
    TurnKnob11,
    TurnKnob12,
    TurnKnob13,
    TurnKnob14,
    TurnKnob15,
    TurnKnob16,
    // Bank A Pads 1..8
    PressPad1,
    PressPad2,
    PressPad3,
    PressPad4,
    PressPad5,
    PressPad6,
    PressPad7,
    PressPad8,
    // Bank B Pads 9..16 (after pressing PAD-B button on controller)
    PressPad9,
    PressPad10,
    PressPad11,
    PressPad12,
    PressPad13,
    PressPad14,
    PressPad15,
    PressPad16,
    // Transport Buttons
    PressPlay,
    PressStop,
    PressRec,
    PressBt,
    Complete
};

class MidiLearn {
public:
    MidiLearn();
    
    void begin(ControllerProfile* profile_out);
    void startWizard();
    void cancel();
    
    bool processIncomingMidi(uint8_t msg_type, uint8_t channel, uint16_t number, int32_t value);
    
    LearnStep currentStep() const { return current_step_; }
    const char* currentStepName() const;
    bool isLearning() const { return current_step_ != LearnStep::Idle && current_step_ != LearnStep::Complete; }

private:
    void advanceStep();
    void recordBinding(MidiBinding& binding, uint8_t msg_type, uint8_t channel, uint16_t number, TargetAction action);

    LearnStep          current_step_ = LearnStep::Idle;
    ControllerProfile* target_profile_ = nullptr;
    uint32_t           last_recorded_time_ms_ = 0;
    uint16_t           last_recorded_number_ = 0xFFFF;
};

} // namespace smk
