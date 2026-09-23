#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include <algorithm>

namespace smk {

/**
 * @brief Chord Memory utility (EXPERIMENTAL / NOT-YET-WIRED to live MIDI stream)
 * 
 * Generates polyphonic harmonic chords from monophonic MIDI note input.
 */
enum class ChordType : uint8_t {
    Off       = 0,
    Octave    = 1,
    Fifth     = 2,
    Major     = 3,
    Minor     = 4,
    Sus4      = 5,
    Dom7      = 6,
    Min7      = 7,
    Maj7      = 8,
    Custom    = 9
};

class ChordMemory {
public:
    static constexpr size_t kMaxChordNotes = 6;

    ChordMemory() {
        custom_intervals_[0] = 0;
        custom_count_ = 1;
    }

    void setChordType(ChordType type) {
        type_ = type;
    }

    ChordType chordType() const {
        return type_;
    }

    bool isEnabled() const {
        return type_ != ChordType::Off;
    }

    static const char* chordName(ChordType type) {
        switch (type) {
            case ChordType::Off:    return "OFF";
            case ChordType::Octave: return "OCTAVE";
            case ChordType::Fifth:  return "FIFTH";
            case ChordType::Major:  return "MAJOR";
            case ChordType::Minor:  return "MINOR";
            case ChordType::Sus4:   return "SUS4";
            case ChordType::Dom7:   return "DOM 7";
            case ChordType::Min7:   return "MIN 7";
            case ChordType::Maj7:   return "MAJ 7";
            case ChordType::Custom: return "CUSTOM";
            default:                return "UNKNOWN";
        }
    }

    // Chord capture workflow
    void startCapture() {
        capturing_ = true;
        captured_notes_.clear();
    }

    void addCaptureNote(uint8_t note) {
        if (!capturing_) return;
        if (std::find(captured_notes_.begin(), captured_notes_.end(), note) == captured_notes_.end()) {
            captured_notes_.push_back(note);
        }
    }

    void finishCapture() {
        capturing_ = false;
        if (captured_notes_.empty()) return;

        std::sort(captured_notes_.begin(), captured_notes_.end());
        uint8_t root = captured_notes_[0];
        custom_count_ = 0;

        for (size_t i = 0; i < captured_notes_.size() && i < kMaxChordNotes; ++i) {
            int interval = (int)captured_notes_[i] - (int)root;
            if (interval >= 0 && interval <= 48) {
                custom_intervals_[custom_count_++] = static_cast<uint8_t>(interval);
            }
        }
        if (custom_count_ == 0) {
            custom_intervals_[0] = 0;
            custom_count_ = 1;
        }
        type_ = ChordType::Custom;
    }

    bool isCapturing() const {
        return capturing_;
    }

    /**
     * @brief Generates chord notes given a root note.
     * @return Number of notes placed in out_notes (1 if off, up to kMaxChordNotes)
     */
    uint8_t getChordNotes(uint8_t root_note, uint8_t* out_notes, uint8_t max_notes) const {
        if (!out_notes || max_notes == 0) return 0;

        if (type_ == ChordType::Off) {
            out_notes[0] = root_note;
            return 1;
        }

        const uint8_t* intervals = nullptr;
        uint8_t count = 0;

        static const uint8_t kOctave[] = { 0, 12 };
        static const uint8_t kFifth[]  = { 0, 7 };
        static const uint8_t kMajor[]  = { 0, 4, 7 };
        static const uint8_t kMinor[]  = { 0, 3, 7 };
        static const uint8_t kSus4[]   = { 0, 5, 7 };
        static const uint8_t kDom7[]   = { 0, 4, 7, 10 };
        static const uint8_t kMin7[]   = { 0, 3, 7, 10 };
        static const uint8_t kMaj7[]   = { 0, 4, 7, 11 };

        switch (type_) {
            case ChordType::Octave: intervals = kOctave; count = 2; break;
            case ChordType::Fifth:  intervals = kFifth;  count = 2; break;
            case ChordType::Major:  intervals = kMajor;  count = 3; break;
            case ChordType::Minor:  intervals = kMinor;  count = 3; break;
            case ChordType::Sus4:   intervals = kSus4;   count = 3; break;
            case ChordType::Dom7:   intervals = kDom7;   count = 4; break;
            case ChordType::Min7:   intervals = kMin7;   count = 4; break;
            case ChordType::Maj7:   intervals = kMaj7;   count = 4; break;
            case ChordType::Custom: intervals = custom_intervals_.data(); count = custom_count_; break;
            default:
                out_notes[0] = root_note;
                return 1;
        }

        uint8_t written = 0;
        for (uint8_t i = 0; i < count && written < max_notes; ++i) {
            int note = (int)root_note + intervals[i];
            if (note <= 127) {
                out_notes[written++] = static_cast<uint8_t>(note);
            }
        }
        return written > 0 ? written : 1;
    }

private:
    ChordType type_ = ChordType::Off;
    bool capturing_ = false;
    std::vector<uint8_t> captured_notes_;
    std::array<uint8_t, kMaxChordNotes> custom_intervals_{0};
    uint8_t custom_count_ = 1;
};

} // namespace smk
