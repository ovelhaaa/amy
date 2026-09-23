#pragma once
#include <cstdint>
#include <array>
#include <cmath>
#include <algorithm>

namespace smk {

/**
 * @brief Scale Quantizer utility (EXPERIMENTAL / NOT-YET-WIRED to live MIDI stream)
 * 
 * Provides musical scale quantization for raw MIDI note pitches.
 */
enum class ScaleType : uint8_t {
    Chromatic       = 0,
    Major           = 1, // Ionian
    NaturalMinor    = 2, // Aeolian
    HarmonicMinor   = 3,
    Dorian          = 4,
    PentatonicMajor = 5,
    PentatonicMinor = 6,
    Blues           = 7,
    Mixolydian      = 8
};

class ScaleQuantizer {
public:
    static constexpr size_t kNumScales = 9;

    ScaleQuantizer() = default;

    void setScale(ScaleType scale) {
        scale_ = scale;
    }

    ScaleType scale() const {
        return scale_;
    }

    void setRootNote(uint8_t root) {
        root_note_ = root % 12;
    }

    uint8_t rootNote() const {
        return root_note_;
    }

    void setEnabled(bool enabled) {
        enabled_ = enabled;
    }

    bool isEnabled() const {
        return enabled_;
    }

    static const char* scaleName(ScaleType type) {
        switch (type) {
            case ScaleType::Chromatic:       return "CHROMATIC";
            case ScaleType::Major:           return "MAJOR";
            case ScaleType::NaturalMinor:    return "NAT MINOR";
            case ScaleType::HarmonicMinor:   return "HARM MINOR";
            case ScaleType::Dorian:          return "DORIAN";
            case ScaleType::PentatonicMajor: return "PENTAMAJOR";
            case ScaleType::PentatonicMinor: return "PENTAMINOR";
            case ScaleType::Blues:           return "BLUES";
            case ScaleType::Mixolydian:      return "MIXOLYDIAN";
            default:                         return "UNKNOWN";
        }
    }

    static const char* noteName(uint8_t note) {
        static const char* const kNotes[12] = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        return kNotes[note % 12];
    }

    bool isNoteInScale(uint8_t note) const {
        if (scale_ == ScaleType::Chromatic) return true;
        uint8_t pc = (note + 12 - root_note_) % 12;
        const auto& mask = getScaleMask(scale_);
        return (mask & (1 << pc)) != 0;
    }

    uint8_t quantize(uint8_t note) const {
        if (!enabled_ || scale_ == ScaleType::Chromatic) return note;

        uint8_t octave = note / 12;
        int pc = (note + 12 - root_note_) % 12;
        uint16_t mask = getScaleMask(scale_);

        if (mask & (1 << pc)) {
            return note; // Already in scale
        }

        // Find nearest scale degree, preferring upward rounding if equidistant
        for (int dist = 1; dist <= 6; ++dist) {
            int up_pc = (pc + dist) % 12;
            if (mask & (1 << up_pc)) {
                int quantized = (int)note + dist;
                return (uint8_t)std::clamp(quantized, 0, 127);
            }
            int down_pc = (pc - dist + 12) % 12;
            if (mask & (1 << down_pc)) {
                int quantized = (int)note - dist;
                return (uint8_t)std::clamp(quantized, 0, 127);
            }
        }
        return note;
    }

private:
    static uint16_t getScaleMask(ScaleType type) {
        // Bitmask of active pitch classes (bit 0 = tonic/root)
        switch (type) {
            case ScaleType::Chromatic:
                return 0x0FFF; // All 12 notes
            case ScaleType::Major:
                return 0x0AB5; // 0, 2, 4, 5, 7, 9, 11 -> 101010110101b
            case ScaleType::NaturalMinor:
                return 0x05AD; // 0, 2, 3, 5, 7, 8, 10 -> 010110101101b
            case ScaleType::HarmonicMinor:
                return 0x09AD; // 0, 2, 3, 5, 7, 8, 11 -> 100110101101b
            case ScaleType::Dorian:
                return 0x06AD; // 0, 2, 3, 5, 7, 9, 10 -> 011010101101b
            case ScaleType::PentatonicMajor:
                return 0x02A5; // 0, 2, 4, 7, 9        -> 001010100101b
            case ScaleType::PentatonicMinor:
                return 0x04A9; // 0, 3, 5, 7, 10       -> 010010101001b
            case ScaleType::Blues:
                return 0x04E9; // 0, 3, 5, 6, 7, 10    -> 010011101001b
            case ScaleType::Mixolydian:
                return 0x06B5; // 0, 2, 4, 5, 7, 9, 10 -> 011010110101b
            default:
                return 0x0FFF;
        }
    }

    ScaleType scale_ = ScaleType::Chromatic;
    uint8_t root_note_ = 0; // 0 = C
    bool enabled_ = false;
};

} // namespace smk
