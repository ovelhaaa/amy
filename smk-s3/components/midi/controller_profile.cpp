#include "controller_profile.h"
#include <cstring>

namespace smk {

uint32_t calculateProfileCrc32(const ControllerProfile& profile) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&profile);
    size_t len = sizeof(ControllerProfile) - sizeof(uint32_t); // Exclude the crc32 field itself
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= p[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

ControllerProfile ProfileManager::createDefaultSmk25Profile() {
    ControllerProfile prof = {};
    strncpy(prof.name, "M-VAVE SMK25 V2", sizeof(prof.name) - 1);

    // Keys: Any note message
    prof.keys = { 0, 0xFF, 0, (uint16_t)TargetAction::Note, 0, 127, 0 };

    // Pitch: PitchBend message
    prof.pitch = { 2, 0xFF, 0, (uint16_t)TargetAction::PitchBend, -8192, 8191, 0 };

    // Modulation: CC #1
    prof.modulation = { 1, 0xFF, 1, (uint16_t)TargetAction::Modulation, 0, 127, 0 };

    // Knobs: CC #1 .. CC #8 -> Macro1 .. Macro8
    for (uint8_t i = 0; i < 8; ++i) {
        prof.knobs[i] = { 1, 0xFF, (uint16_t)(1 + i), (uint16_t)((uint16_t)TargetAction::Macro1 + i), 0, 127, 0 };
    }

    // Pads: Note #36 .. Note #43 (or CC #36..#43) -> Pad1 .. Pad8
    for (uint8_t i = 0; i < 8; ++i) {
        prof.pads[i] = { 0, 0xFF, (uint16_t)(36 + i), (uint16_t)((uint16_t)TargetAction::Pad1 + i), 0, 127, 0 };
    }

    // Buttons (Oct+, Oct-, Play, Stop, Rec, BT, Arp, SC/CH, KNOB-B, PAD-B)
    prof.buttons[0] = { 1, 0xFF, 102, (uint16_t)TargetAction::OctaveUp,   0, 127, 0 };
    prof.buttons[1] = { 1, 0xFF, 103, (uint16_t)TargetAction::OctaveDown, 0, 127, 0 };
    prof.buttons[2] = { 1, 0xFF, 114, (uint16_t)TargetAction::Play,       0, 127, 0 };
    prof.buttons[3] = { 1, 0xFF, 115, (uint16_t)TargetAction::Stop,       0, 127, 0 };
    prof.buttons[4] = { 1, 0xFF, 117, (uint16_t)TargetAction::Rec,        0, 127, 0 };
    prof.buttons[5] = { 1, 0xFF, 118, (uint16_t)TargetAction::Bt,         0, 127, 0 };
    prof.buttons[6] = { 1, 0xFF, 119, (uint16_t)TargetAction::Arp,        0, 127, 0 };
    prof.buttons[7] = { 1, 0xFF, 120, (uint16_t)TargetAction::ScCh,       0, 127, 0 };
    prof.buttons[8] = { 1, 0xFF, 121, (uint16_t)TargetAction::KnobB,      0, 127, 0 };
    prof.buttons[9] = { 1, 0xFF, 122, (uint16_t)TargetAction::PadB,       0, 127, 0 };

    prof.crc32 = calculateProfileCrc32(prof);
    return prof;
}

TargetAction ProfileManager::matchBinding(const ControllerProfile& profile, uint8_t msg_type, uint8_t channel, uint16_t number) {
    // Check pitch bend
    if (msg_type == 2 && profile.pitch.msg_type == 2) {
        return TargetAction::PitchBend;
    }

    // Check knobs
    if (msg_type == 1) { // CC
        if (profile.modulation.msg_type == 1 && profile.modulation.number == number) {
            return TargetAction::Modulation;
        }
        for (uint8_t i = 0; i < 8; ++i) {
            if (profile.knobs[i].msg_type == 1 && profile.knobs[i].number == number) {
                return static_cast<TargetAction>(profile.knobs[i].target_action);
            }
        }
        for (uint8_t i = 0; i < 10; ++i) {
            if (profile.buttons[i].msg_type == 1 && profile.buttons[i].number == number) {
                return static_cast<TargetAction>(profile.buttons[i].target_action);
            }
        }
    }

    // Check pads vs keys
    if (msg_type == 0) { // Note
        for (uint8_t i = 0; i < 8; ++i) {
            if (profile.pads[i].msg_type == 0 && profile.pads[i].number == number) {
                return static_cast<TargetAction>(profile.pads[i].target_action);
            }
        }
        return TargetAction::Note;
    }

    return TargetAction::Unmapped;
}

} // namespace smk
