#pragma once
#include <cstddef>
#include <cstdint>

namespace smk {
struct MidiEndpoint {
    uint8_t interface_number = 0;
    uint8_t address = 0;
    uint16_t packet_bytes = 0;
};

// The caller supplies the accessible byte span, never an unchecked end pointer.
// Only alternate setting zero, USB MIDI 1.0 class-compliant bulk IN is accepted.
inline bool findMidiEndpoint(const uint8_t* data, size_t size, MidiEndpoint& result) {
    if (!data || size < 9 || data[0] < 9 || data[1] != 2) return false;
    const size_t total = data[2] | (size_t(data[3]) << 8);
    if (total < data[0] || total > size) return false;
    bool midi = false, found = false;
    MidiEndpoint candidate{};
    uint8_t interface_number = 0;
    for (size_t offset = 0; offset < total;) {
        const size_t remaining = total - offset;
        if (remaining < 2) return false;
        const uint8_t* p = data + offset;
        const size_t length = p[0];
        if (length < 2 || length > remaining) return false;
        if (p[1] == 4) {
            if (length < 9) return false;
            midi = p[3] == 0 && p[4] > 0 && p[5] == 1 && p[6] == 3 && p[7] == 0;
            interface_number = p[2];
        } else if (p[1] == 5) {
            if (length < 7) return false;
            const uint16_t mps = p[4] | (uint16_t(p[5]) << 8);
            // S3 full-speed bulk endpoints: legal MPS, no reserved address bits.
            if (midi && (p[2] & 0x80) && (p[2] & 0x0f) && !(p[2] & 0x70) &&
                (p[3] & 3) == 2 && (mps == 8 || mps == 16 || mps == 32 || mps == 64) && !found) {
                candidate = {interface_number, p[2], mps};
                found = true;
            }
        }
        offset += length;
    }
    if (found) result = candidate;
    return found;
}
} // namespace smk
