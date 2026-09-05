#include "midi_learn.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "MidiLearn";

namespace smk {

MidiLearn::MidiLearn() {}

void MidiLearn::begin(ControllerProfile* profile_out) {
    target_profile_ = profile_out;
    current_step_ = LearnStep::Idle;
}

void MidiLearn::startWizard() {
    if (!target_profile_) return;
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "Starting MIDI Learn Wizard");
    current_step_ = LearnStep::PressKey;
    ESP_LOGI(TAG, ">>> STEP [1/%u]: %s <<<", static_cast<unsigned>(LearnStep::Complete), currentStepName());
    ESP_LOGI(TAG, "==================================================");
}

void MidiLearn::finishAndSave() {
    if (!target_profile_) return;
    
    // Fill in any bindings that were not learned with standard SMK25 defaults
    smk::ControllerProfile defaults = smk::ProfileManager::createDefaultSmk25Profile();
    
    if (target_profile_->keys.target_action == 0) {
        target_profile_->keys = defaults.keys;
    }
    if (target_profile_->pitch.target_action == 0) {
        target_profile_->pitch = defaults.pitch;
    }
    if (target_profile_->modulation.target_action == 0) {
        target_profile_->modulation = defaults.modulation;
    }
    for (size_t i = 0; i < 16; ++i) {
        if (target_profile_->knobs[i].target_action == 0) {
            target_profile_->knobs[i] = defaults.knobs[i];
        }
    }
    for (size_t i = 0; i < 16; ++i) {
        if (target_profile_->pads[i].target_action == 0) {
            target_profile_->pads[i] = defaults.pads[i];
        }
    }
    for (size_t i = 0; i < kTransportBindingCount; ++i) {
        if (target_profile_->buttons[i].target_action == 0) {
            target_profile_->buttons[i] = defaults.buttons[i];
        }
    }

    target_profile_->crc32 = calculateProfileCrc32(*target_profile_);
    current_step_ = LearnStep::Complete;
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "MIDI Learn Finished and Saved (CRC32: 0x%08X)", target_profile_->crc32);
    ESP_LOGI(TAG, "==================================================");
}

void MidiLearn::cancel() {
    ESP_LOGI(TAG, "MIDI Learn Wizard Cancelled");
    current_step_ = LearnStep::Idle;
}

void MidiLearn::skipStep() {
    if (!isLearning()) return;
    ESP_LOGI(TAG, "Skipped step [%u]: %s", static_cast<unsigned>(current_step_), currentStepName());
    advanceStep();
}

const char* MidiLearn::currentStepName() const {
    switch (current_step_) {
        case LearnStep::PressKey:        return "Press any Key";
        case LearnStep::MovePitch:       return "Move Pitch Bend";
        case LearnStep::MoveModulation:  return "Move Modulation";
        case LearnStep::TurnKnob1:       return "Turn Knob 1 (Bank A)";
        case LearnStep::TurnKnob2:       return "Turn Knob 2 (Bank A)";
        case LearnStep::TurnKnob3:       return "Turn Knob 3 (Bank A)";
        case LearnStep::TurnKnob4:       return "Turn Knob 4 (Bank A)";
        case LearnStep::TurnKnob5:       return "Turn Knob 5 (Bank A)";
        case LearnStep::TurnKnob6:       return "Turn Knob 6 (Bank A)";
        case LearnStep::TurnKnob7:       return "Turn Knob 7 (Bank A)";
        case LearnStep::TurnKnob8:       return "Turn Knob 8 (Bank A)";
        case LearnStep::TurnKnob9:       return "[KNOB-B] Ative KNOB-B, Gire Knob 1";
        case LearnStep::TurnKnob10:      return "Turn Knob 2 (Bank B)";
        case LearnStep::TurnKnob11:      return "Turn Knob 3 (Bank B)";
        case LearnStep::TurnKnob12:      return "Turn Knob 4 (Bank B)";
        case LearnStep::TurnKnob13:      return "Turn Knob 5 (Bank B)";
        case LearnStep::TurnKnob14:      return "Turn Knob 6 (Bank B)";
        case LearnStep::TurnKnob15:      return "Turn Knob 7 (Bank B)";
        case LearnStep::TurnKnob16:      return "Turn Knob 8 (Bank B)";
        case LearnStep::PressPad1:       return "Press Pad 1 (Bank A)";
        case LearnStep::PressPad2:       return "Press Pad 2 (Bank A)";
        case LearnStep::PressPad3:       return "Press Pad 3 (Bank A)";
        case LearnStep::PressPad4:       return "Press Pad 4 (Bank A)";
        case LearnStep::PressPad5:       return "Press Pad 5 (Bank A)";
        case LearnStep::PressPad6:       return "Press Pad 6 (Bank A)";
        case LearnStep::PressPad7:       return "Press Pad 7 (Bank A)";
        case LearnStep::PressPad8:       return "Press Pad 8 (Bank A)";
        case LearnStep::PressPad9:       return "[PAD-B] Ative PAD-B, Toque Pad 1";
        case LearnStep::PressPad10:      return "Press Pad 2 (Bank B)";
        case LearnStep::PressPad11:      return "Press Pad 3 (Bank B)";
        case LearnStep::PressPad12:      return "Press Pad 4 (Bank B)";
        case LearnStep::PressPad13:      return "Press Pad 5 (Bank B)";
        case LearnStep::PressPad14:      return "Press Pad 6 (Bank B)";
        case LearnStep::PressPad15:      return "Press Pad 7 (Bank B)";
        case LearnStep::PressPad16:      return "Press Pad 8 (Bank B)";
        case LearnStep::PressPlay:       return "Press Play Button";
        case LearnStep::PressStop:       return "Press Stop Button";
        case LearnStep::PressRec:        return "Press Rec Button";
        case LearnStep::Complete:        return "Learn Complete!";
        default:                         return "Idle";
    }
}

const char* MidiLearn::currentStepHint() const {
    switch (current_step_) {
        case LearnStep::PressKey:        return "Toque qualquer tecla no teclado";
        case LearnStep::MovePitch:       return "Mova Pitch Wheel p/ calibrar ou toque tecla p/ SALVAR";
        case LearnStep::MoveModulation:  return "Mova a roda/fita de Modulacao";
        case LearnStep::TurnKnob1:       return "Gire o Knob 1 (Banco A)";
        case LearnStep::TurnKnob2:       return "Gire o Knob 2 (Banco A)";
        case LearnStep::TurnKnob3:       return "Gire o Knob 3 (Banco A)";
        case LearnStep::TurnKnob4:       return "Gire o Knob 4 (Banco A)";
        case LearnStep::TurnKnob5:       return "Gire o Knob 5 (Banco A)";
        case LearnStep::TurnKnob6:       return "Gire o Knob 6 (Banco A)";
        case LearnStep::TurnKnob7:       return "Gire o Knob 7 (Banco A)";
        case LearnStep::TurnKnob8:       return "Gire o Knob 8 (Banco A)";
        case LearnStep::TurnKnob9:       return "Ative KNOB-B no teclado e gire Knob 1";
        case LearnStep::TurnKnob10:      return "Gire o Knob 2 (Banco B)";
        case LearnStep::TurnKnob11:      return "Gire o Knob 3 (Banco B)";
        case LearnStep::TurnKnob12:      return "Gire o Knob 4 (Banco B)";
        case LearnStep::TurnKnob13:      return "Gire o Knob 5 (Banco B)";
        case LearnStep::TurnKnob14:      return "Gire o Knob 6 (Banco B)";
        case LearnStep::TurnKnob15:      return "Gire o Knob 7 (Banco B)";
        case LearnStep::TurnKnob16:      return "Gire o Knob 8 (Banco B)";
        case LearnStep::PressPad1:       return "Pressione o Pad 1 (Banco A)";
        case LearnStep::PressPad2:       return "Pressione o Pad 2 (Banco A)";
        case LearnStep::PressPad3:       return "Pressione o Pad 3 (Banco A)";
        case LearnStep::PressPad4:       return "Pressione o Pad 4 (Banco A)";
        case LearnStep::PressPad5:       return "Pressione o Pad 5 (Banco A)";
        case LearnStep::PressPad6:       return "Pressione o Pad 6 (Banco A)";
        case LearnStep::PressPad7:       return "Pressione o Pad 7 (Banco A)";
        case LearnStep::PressPad8:       return "Pressione o Pad 8 (Banco A)";
        case LearnStep::PressPad9:       return "Ative PAD-B no teclado e toque Pad 1";
        case LearnStep::PressPad10:      return "Pressione o Pad 2 (Banco B)";
        case LearnStep::PressPad11:      return "Pressione o Pad 3 (Banco B)";
        case LearnStep::PressPad12:      return "Pressione o Pad 4 (Banco B)";
        case LearnStep::PressPad13:      return "Pressione o Pad 5 (Banco B)";
        case LearnStep::PressPad14:      return "Pressione o Pad 6 (Banco B)";
        case LearnStep::PressPad15:      return "Pressione o Pad 7 (Banco B)";
        case LearnStep::PressPad16:      return "Pressione o Pad 8 (Banco B)";
        case LearnStep::PressPlay:       return "Pressione o Botao PLAY";
        case LearnStep::PressStop:       return "Pressione o Botao STOP";
        case LearnStep::PressRec:        return "Pressione o Botao REC";
        case LearnStep::Complete:        return "Mapeamento Completo!";
        default:                         return "Aguardando...";
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
    last_recorded_time_ms_ = static_cast<uint32_t>(esp_timer_get_time() / 1000);

    if (current_step_ == LearnStep::Complete && target_profile_) {
        target_profile_->crc32 = calculateProfileCrc32(*target_profile_);
        ESP_LOGI(TAG, "==================================================");
        ESP_LOGI(TAG, "MIDI Learn Wizard Completed Successfully! (CRC32: 0x%08X)", target_profile_->crc32);
        ESP_LOGI(TAG, "==================================================");
    } else if (current_step_ != LearnStep::Idle) {
        ESP_LOGI(TAG, ">>> STEP [%u/%u]: %s <<<", static_cast<unsigned>(current_step_), static_cast<unsigned>(LearnStep::Complete), currentStepName());
    }
}

bool MidiLearn::processEvent(const SynthEvent& event) {
    if (event.source != EventSource::UsbMidi || bypassMidiLearn(event)) return false;
    uint8_t msg_type;
    switch (event.type) {
        case EventType::NoteOn: msg_type = 0; break;
        case EventType::ControlChange:
        case EventType::Modulation: msg_type = 1; break;
        case EventType::PitchBend: msg_type = 2; break;
        default: return false;
    }
    return processIncomingMidi(msg_type, event.channel, event.id, event.value);
}

bool MidiLearn::processIncomingMidi(uint8_t msg_type, uint8_t channel, uint16_t number, int32_t value) {
    if (!isLearning() || !target_profile_) return false;

    // Enforce 250ms refractory cooldown delay between recorded steps
    uint32_t now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    if (now_ms - last_recorded_time_ms_ < 250) {
        return false;
    }

    // Require distinct ID if received within 600ms
    if (number == last_recorded_number_ && (now_ms - last_recorded_time_ms_ < 600)) {
        return false;
    }

    bool recorded = false;

    switch (current_step_) {
        case LearnStep::PressKey:
            if (msg_type == 0 && value > 0) { // Note On with velocity > 0
                recordBinding(target_profile_->keys, msg_type, channel, number, TargetAction::Note);
                ESP_LOGI(TAG, "Recorded Keys: msg_type=%d, ch=%d, num=%d", msg_type, channel, number);
                recorded = true;
            }
            break;
        case LearnStep::MovePitch:
            if (msg_type == 2) { // Pitch Bend moved -> calibrate and continue
                recordBinding(target_profile_->pitch, msg_type, channel, number, TargetAction::PitchBend);
                ESP_LOGI(TAG, "Recorded Pitch: msg_type=%d -> Continuing Full Wizard", msg_type);
                recorded = true;
            } else if (msg_type == 0 && value > 0) {
                // Key pressed instead of moving pitch wheel -> finish and save default profile!
                ESP_LOGI(TAG, "Key pressed at MovePitch -> Finalizing and saving profile by default");
                finishAndSave();
                return true;
            }
            break;
        case LearnStep::MoveModulation:
            if (msg_type == 1) { // CC
                recordBinding(target_profile_->modulation, msg_type, channel, number, TargetAction::Modulation);
                ESP_LOGI(TAG, "Recorded Modulation: CC #%d", number);
                recorded = true;
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
        case LearnStep::TurnKnob9:
        case LearnStep::TurnKnob10:
        case LearnStep::TurnKnob11:
        case LearnStep::TurnKnob12:
        case LearnStep::TurnKnob13:
        case LearnStep::TurnKnob14:
        case LearnStep::TurnKnob15:
        case LearnStep::TurnKnob16:
            if (msg_type == 1) { // CC
                if (number == target_profile_->modulation.number) return false;
                uint8_t knob_idx = static_cast<uint8_t>(current_step_) - static_cast<uint8_t>(LearnStep::TurnKnob1);
                if (knob_idx > 0 && number == target_profile_->knobs[knob_idx - 1].number) return false;
                TargetAction act = static_cast<TargetAction>(static_cast<uint16_t>(TargetAction::Knob1) + knob_idx);
                recordBinding(target_profile_->knobs[knob_idx], msg_type, channel, number, act);
                if (knob_idx < 8) {
                    ESP_LOGI(TAG, "Recorded Knob #%d (Bank A): CC #%d", knob_idx + 1, number);
                } else {
                    ESP_LOGI(TAG, "Recorded Knob #%d (Bank B): CC #%d", (knob_idx - 8) + 1, number);
                }
                recorded = true;
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
        case LearnStep::PressPad9:
        case LearnStep::PressPad10:
        case LearnStep::PressPad11:
        case LearnStep::PressPad12:
        case LearnStep::PressPad13:
        case LearnStep::PressPad14:
        case LearnStep::PressPad15:
        case LearnStep::PressPad16:
            if ((msg_type == 0 && value > 0) || (msg_type == 1 && value > 64)) {
                uint8_t pad_idx = static_cast<uint8_t>(current_step_) - static_cast<uint8_t>(LearnStep::PressPad1);
                TargetAction act = static_cast<TargetAction>(static_cast<uint16_t>(TargetAction::Pad1) + pad_idx);
                recordBinding(target_profile_->pads[pad_idx], msg_type, channel, number, act);
                ESP_LOGI(TAG, "Recorded Pad #%d: num #%d", pad_idx + 1, number);
                recorded = true;
            }
            break;
        case LearnStep::PressPlay:
        case LearnStep::PressStop:
        case LearnStep::PressRec:
            if ((msg_type == 1 && value > 64 && number != target_profile_->modulation.number) || (msg_type == 0 && value > 0)) {
                uint8_t btn_idx = static_cast<uint8_t>(current_step_) - static_cast<uint8_t>(LearnStep::PressPlay);
                TargetAction act = static_cast<TargetAction>(static_cast<uint16_t>(TargetAction::Play) + btn_idx);
                recordBinding(target_profile_->buttons[btn_idx], msg_type, channel, number, act);
                ESP_LOGI(TAG, "Recorded Transport Button #%d: num #%d", btn_idx + 1, number);
                recorded = true;
            }
            break;
        default:
            break;
    }

    if (recorded) {
        last_recorded_number_ = number;
        last_captured_type_ = msg_type;
        last_captured_channel_ = channel;
        last_captured_value_ = value;
        advanceStep();
        return true;
    }
    return false;
}

} // namespace smk
