#pragma once
#include <cstdint>
#include <cstddef>

namespace smk {

constexpr uint32_t kProfileMagic = 0x53334D31; // "S3M1"
constexpr uint16_t kProfileFormatVersion = 2;
constexpr size_t kTransportBindingCount = 3;

enum class TargetAction : uint16_t {
    Unmapped     = 0,
    Note         = 1,
    PitchBend    = 2,
    Modulation   = 3,
    // 16 Physical Knobs (Knobs 1..8 Bank A, Knobs 9..16 Bank B)
    Knob1        = 4,
    Knob2        = 5,
    Knob3        = 6,
    Knob4        = 7,
    Knob5        = 8,
    Knob6        = 9,
    Knob7        = 10,
    Knob8        = 11,
    Knob9        = 12,
    Knob10       = 13,
    Knob11       = 14,
    Knob12       = 15,
    Knob13       = 16,
    Knob14       = 17,
    Knob15       = 18,
    Knob16       = 19,
    // 16 Pads (Pads 1..8 Bank A, Pads 9..16 Bank B)
    Pad1         = 20,
    Pad2         = 21,
    Pad3         = 22,
    Pad4         = 23,
    Pad5         = 24,
    Pad6         = 25,
    Pad7         = 26,
    Pad8         = 27,
    Pad9         = 28,
    Pad10        = 29,
    Pad11        = 30,
    Pad12        = 31,
    Pad13        = 32,
    Pad14        = 33,
    Pad15        = 34,
    Pad16        = 35,
    // Transport Buttons
    Play         = 36,
    Stop         = 37,
    Rec          = 38,
    Bt           = 39
};

struct ProfileHeader {
    uint32_t magic;          // 0x53334D31
    uint16_t format_version; // Version 2
    uint16_t data_size;      // Payload size
    uint32_t crc32;          // Payload CRC32 checksum
};

struct MidiBinding {
    uint8_t  msg_type;       // 0=Note, 1=CC, 2=PitchBend, 3=ProgramChange
    uint8_t  channel;        // 0..15 or 0xFF (any channel)
    uint16_t number;         // Note number or CC number
    uint16_t target_action;  // TargetAction enum value
    int16_t  min_val;
    int16_t  max_val;
    uint8_t  flags;          // Bit 0: Toggle mode, Bit 1: Inverted
};

struct ControllerProfile {
    char        name[24];
    MidiBinding keys;
    MidiBinding pitch;
    MidiBinding modulation;
    MidiBinding knobs[16];  // 16 physical knobs (0..7 Bank A, 8..15 Bank B)
    MidiBinding pads[16];   // 16 pads (0..7 Bank A, 8..15 Bank B)
    // Play, Stop and Rec emit MIDI. BT does not and is intentionally unmapped.
    MidiBinding buttons[4];
    uint32_t    crc32;
};

uint32_t calculateProfileCrc32(const ControllerProfile& profile);

class ProfileManager {
public:
    static ControllerProfile createDefaultSmk25Profile();
    static TargetAction matchBinding(const ControllerProfile& profile, uint8_t msg_type, uint8_t channel, uint16_t number);
};

} // namespace smk
