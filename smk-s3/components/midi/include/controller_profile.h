#pragma once
#include <cstdint>
#include <cstddef>

namespace smk {

constexpr uint32_t kProfileMagic = 0x53334D31; // "S3M1"
constexpr uint16_t kProfileFormatVersion = 1;

enum class TargetAction : uint16_t {
    Unmapped     = 0,
    Note         = 1,
    PitchBend    = 2,
    Modulation   = 3,
    Macro1       = 4,
    Macro2       = 5,
    Macro3       = 6,
    Macro4       = 7,
    Macro5       = 8,
    Macro6       = 9,
    Macro7       = 10,
    Macro8       = 11,
    Pad1         = 12,
    Pad2         = 13,
    Pad3         = 14,
    Pad4         = 15,
    Pad5         = 16,
    Pad6         = 17,
    Pad7         = 18,
    Pad8         = 19,
    OctaveUp     = 20,
    OctaveDown   = 21,
    Play         = 22,
    Stop         = 23,
    Rec          = 24,
    Bt           = 25,
    Arp          = 26,
    ScCh         = 27,
    KnobB        = 28,
    PadB         = 29
};

struct ProfileHeader {
    uint32_t magic;          // 0x53334D31
    uint16_t format_version; // Version 1
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
    MidiBinding knobs[8];
    MidiBinding pads[8];
    MidiBinding buttons[10]; // OctUp, OctDown, Play, Stop, Rec, Bt, Arp, ScCh, KnobB, PadB
    uint32_t    crc32;
};

uint32_t calculateProfileCrc32(const ControllerProfile& profile);

class ProfileManager {
public:
    static ControllerProfile createDefaultSmk25Profile();
    static TargetAction matchBinding(const ControllerProfile& profile, uint8_t msg_type, uint8_t channel, uint16_t number);
};

} // namespace smk
