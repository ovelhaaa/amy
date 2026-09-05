#pragma once
#include "controller_profile.h"
#include "synth_event.h"
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
    // Bank B Knobs 9..16 (activated via KNOB-B button on controller)
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
    // Bank B Pads 9..16 (activated via PAD-B button on controller)
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
    Complete
};

class MidiLearn {
public:
    MidiLearn();
    
    void begin(ControllerProfile* profile_out);
    void startWizard();
    void finishAndSave();
    void cancel();
    void skipStep();
    void reset() { current_step_ = LearnStep::Idle; }
    
    bool processIncomingMidi(uint8_t msg_type, uint8_t channel, uint16_t number, int32_t value);
    bool processEvent(const SynthEvent& event);
    
    LearnStep currentStep() const { return current_step_; }
    uint8_t currentStepNumber() const { return static_cast<uint8_t>(current_step_); }
    uint8_t totalSteps() const { return static_cast<uint8_t>(LearnStep::Complete) - 1; }
    const char* currentStepName() const;
    const char* currentStepHint() const;
    bool isLearning() const { return current_step_ != LearnStep::Idle && current_step_ != LearnStep::Complete; }
    bool isComplete() const { return current_step_ == LearnStep::Complete; }

    uint8_t  lastCapturedType() const { return last_captured_type_; }
    uint8_t  lastCapturedChannel() const { return last_captured_channel_; }
    uint16_t lastCapturedNumber() const { return last_recorded_number_; }
    int32_t  lastCapturedValue() const { return last_captured_value_; }
    uint32_t lastCapturedTimeMs() const { return last_recorded_time_ms_; }
    ControllerProfile* targetProfile() const { return target_profile_; }

private:
    void advanceStep();
    void recordBinding(MidiBinding& binding, uint8_t msg_type, uint8_t channel, uint16_t number, TargetAction action);

    LearnStep          current_step_ = LearnStep::Idle;
    ControllerProfile* target_profile_ = nullptr;
    uint32_t           last_recorded_time_ms_ = 0;
    uint16_t           last_recorded_number_ = 0xFFFF;
    uint8_t            last_captured_type_ = 0;
    uint8_t            last_captured_channel_ = 0;
    int32_t            last_captured_value_ = 0;
};

} // namespace smk
