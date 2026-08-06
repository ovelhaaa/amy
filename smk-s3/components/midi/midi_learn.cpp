#include "midi_learn.h"
#include "esp_log.h"

static const char* TAG = "MidiLearn";

namespace smk {

MidiLearn::MidiLearn() {}

void MidiLearn::begin(ControllerProfile* profile_out) {
    target_profile_ = profile_out;
    current_step_ = LearnStep::Idle;
}

void MidiLearn::startWizard() {
    if (!target_profile_) return;
    ESP_LOGI(TAG, "Starting MIDI Learn Wizard");
    current_step_ = LearnStep::PressKey;
}

void MidiLearn::cancel() {
    ESP_LOGI(TAG, "MIDI Learn Wizard Cancelled");
    current_step_ = LearnStep::Idle;
}

const char* MidiLearn::currentStepName() const {
    switch (current_step_) {
        case LearnStep::PressKey:        return "Press any Key";
        case LearnStep::MovePitch:       return "Move Pitch Bend";
        case LearnStep::MoveModulation:  return "Move Modulation";
        case LearnStep::TurnKnob1:       return "Turn Knob 1";
        case LearnStep::TurnKnob2:       return "Turn Knob 2";
        case LearnStep::TurnKnob3:       return "Turn Knob 3";
        case LearnStep::TurnKnob4:       return "Turn Knob 4";
        case LearnStep::TurnKnob5:       return "Turn Knob 5";
        case LearnStep::TurnKnob6:       return "Turn Knob 6";
        case LearnStep::TurnKnob7:       return "Turn Knob 7";
        case LearnStep::TurnKnob8:       return "Turn Knob 8";
        case LearnStep::PressPad1:       return "Press Pad 1";
        case LearnStep::PressPad2:       return "Press Pad 2";
        case LearnStep::PressPad3:       return "Press Pad 3";
        case LearnStep::PressPad4:       return "Press Pad 4";
        case LearnStep::PressPad5:       return "Press Pad 5";
        case LearnStep::PressPad6:       return "Press Pad 6";
        case LearnStep::PressPad7:       return "Press Pad 7";
        case LearnStep::PressPad8:       return "Press Pad 8";
        case LearnStep::PressOctUp:      return "Press Oct+ Button";
        case LearnStep::PressOctDown:    return "Press Oct- Button";
        case LearnStep::PressPlay:       return "Press Play Button";
        case LearnStep::PressStop:       return "Press Stop Button";
        case LearnStep::PressRec:        return "Press Rec Button";
        case LearnStep::PressBt:         return "Press BT Button";
        case LearnStep::PressArp:        return "Press Arp Button";
        case LearnStep::PressScCh:       return "Press SC/CH Button";
        case LearnStep::PressKnobB:      return "Press KNOB-B Button";
        case LearnStep::PressPadB:       return "Press PAD-B Button";
        case LearnStep::Complete:        return "Learn Complete!";
        default:                         return "Idle";
    }
}

void MidiLearn::recordBinding(MidiBinding& binding, uint8_t msg_type, uint8_t channel, uint16_t number, TargetAction action) {
    binding.msg_type = msg_type;
    binding.channel = channel;
    binding.number = number;
    binding.target_action = static_cast<uint16_t>(action);
    binding.min_val = 0;
    binding.max_val = 127;
    binding.flags = 0;
}

void MidiLearn::advanceStep() {
    uint8_t step_idx = static_cast<uint8_t>(current_step_);
    if (step_idx < static_cast<uint8_t>(LearnStep::Complete)) {
        current_step_ = static_cast<LearnStep>(step_idx + 1);
    }
    if (current_step_ == LearnStep::Complete && target_profile_) {
        target_profile_->crc32 = calculateProfileCrc32(*target_profile_);
        ESP_LOGI(TAG, "MIDI Learn Wizard Completed Successfully! (CRC32: 0x%08X)", target_profile_->crc32);
    }
}

bool MidiLearn::processIncomingMidi(uint8_t msg_type, uint8_t channel, uint16_t number, int32_t value) {
    if (!isLearning() || !target_profile_) return false;

    switch (current_step_) {
        case LearnStep::PressKey:
            if (msg_type == 0) { // Note On
                recordBinding(target_profile_->keys, msg_type, channel, number, TargetAction::Note);
                ESP_LOGI(TAG, "Recorded Keys: msg_type=%d, ch=%d, num=%d", msg_type, channel, number);
                advanceStep();
                return true;
            }
            break;
        case LearnStep::MovePitch:
            if (msg_type == 2) { // Pitch Bend
                recordBinding(target_profile_->pitch, msg_type, channel, number, TargetAction::PitchBend);
                ESP_LOGI(TAG, "Recorded Pitch: msg_type=%d", msg_type);
                advanceStep();
                return true;
            }
            break;
        case LearnStep::MoveModulation:
            if (msg_type == 1) { // CC
                recordBinding(target_profile_->modulation, msg_type, channel, number, TargetAction::Modulation);
                ESP_LOGI(TAG, "Recorded Modulation: CC #%d", number);
                advanceStep();
                return true;
            }
            break;
        case LearnStep::TurnKnob1:
        case LearnStep::TurnKnob2:
        case LearnStep::TurnKnob3:
        case LearnStep::TurnKnob4:
        case LearnStep::TurnKnob5:
        case LearnStep::TurnKnob6:
        case LearnStep::TurnKnob7:
        case LearnStep::TurnKnob8:
            if (msg_type == 1) { // CC
                uint8_t knob_idx = static_cast<uint8_t>(current_step_) - static_cast<uint8_t>(LearnStep::TurnKnob1);
                TargetAction act = static_cast<TargetAction>(static_cast<uint16_t>(TargetAction::Macro1) + knob_idx);
                recordBinding(target_profile_->knobs[knob_idx], msg_type, channel, number, act);
                ESP_LOGI(TAG, "Recorded Knob #%d: CC #%d", knob_idx + 1, number);
                advanceStep();
                return true;
            }
            break;
        case LearnStep::PressPad1:
        case LearnStep::PressPad2:
        case LearnStep::PressPad3:
        case LearnStep::PressPad4:
        case LearnStep::PressPad5:
        case LearnStep::PressPad6:
        case LearnStep::PressPad7:
        case LearnStep::PressPad8:
            if (msg_type == 0 || msg_type == 1) { // Note or CC
                uint8_t pad_idx = static_cast<uint8_t>(current_step_) - static_cast<uint8_t>(LearnStep::PressPad1);
                TargetAction act = static_cast<TargetAction>(static_cast<uint16_t>(TargetAction::Pad1) + pad_idx);
                recordBinding(target_profile_->pads[pad_idx], msg_type, channel, number, act);
                ESP_LOGI(TAG, "Recorded Pad #%d: num #%d", pad_idx + 1, number);
                advanceStep();
                return true;
            }
            break;
        case LearnStep::PressOctUp:
        case LearnStep::PressOctDown:
        case LearnStep::PressPlay:
        case LearnStep::PressStop:
        case LearnStep::PressRec:
        case LearnStep::PressBt:
        case LearnStep::PressArp:
        case LearnStep::PressScCh:
        case LearnStep::PressKnobB:
        case LearnStep::PressPadB:
            if (msg_type == 1 || msg_type == 0) {
                uint8_t btn_idx = static_cast<uint8_t>(current_step_) - static_cast<uint8_t>(LearnStep::PressOctUp);
                TargetAction act = static_cast<TargetAction>(static_cast<uint16_t>(TargetAction::OctaveUp) + btn_idx);
                recordBinding(target_profile_->buttons[btn_idx], msg_type, channel, number, act);
                ESP_LOGI(TAG, "Recorded Button #%d: num #%d", btn_idx, number);
                advanceStep();
                return true;
            }
            break;
        default:
            break;
    }
    return false;
}

} // namespace smk
