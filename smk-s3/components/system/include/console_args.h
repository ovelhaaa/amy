#pragma once

#include <cstdint>

namespace smk {

// Result of validating a console command's numeric arguments. Kept free of
// esp_console so the exact same rules can be unit-tested on the host.
enum class ArgStatus : uint8_t {
    Ok = 0,
    Missing, // A required argument was not supplied.
    Invalid, // Non-numeric, overflow, trailing garbage or out of range.
};

struct NoteOnArgs {
    uint8_t note;
    uint8_t velocity;
    uint8_t channel;
};

struct NoteOffArgs {
    uint8_t note;
    uint8_t channel;
};

// Strict base-10 unsigned integer parse. Rejects null/empty text, an explicit
// sign, leading or trailing whitespace, any non-digit character, overflow and
// any result outside [minimum, maximum]. Unlike atoi(), "60abc", " 60", "-1"
// and "99999999999999999999" are all rejected instead of silently accepted.
bool parseDecimal(const char* text, long minimum, long maximum, long& out);

// note_on <note 0..127> <velocity 1..127> [channel 0..15]
ArgStatus parseNoteOnArgs(int argc, char** argv, NoteOnArgs& out);

// note_off <note 0..127> [channel 0..15]
ArgStatus parseNoteOffArgs(int argc, char** argv, NoteOffArgs& out);

} // namespace smk
