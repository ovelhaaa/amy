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
    TurnKnob1,
    TurnKnob2,
    TurnKnob3,
    TurnKnob4,
    TurnKnob5,
    TurnKnob6,
    TurnKnob7,
    TurnKnob8,
    PressPad1,
    PressPad2,
    PressPad3,
    PressPad4,
    PressPad5,
    PressPad6,
    PressPad7,
    PressPad8,
    PressOctUp,
    PressOctDown,
    PressPlay,
    PressStop,
    PressRec,
    PressBt,
    PressArp,
    PressScCh,
    PressKnobB,
    PressPadB,
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
};

} // namespace smk
