#include "console_args.h"

namespace smk {

bool parseDecimal(const char* text, long minimum, long maximum, long& out) {
    if (!text || !*text || maximum < minimum) return false;
    long value = 0;
    for (const char* p = text; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
        const long digit = *p - '0';
        // Reject before multiplying. This also detects overflow for arbitrarily
        // long digit strings without ever invoking signed overflow.
        if (value > (maximum - digit) / 10) return false;
        value = value * 10 + digit;
    }
    if (value < minimum) return false;
    out = value;
    return true;
}

ArgStatus parseNoteOnArgs(int argc, char** argv, NoteOnArgs& out) {
    if (argc < 3) return ArgStatus::Missing;
    long note = 0;
    long velocity = 0;
    long channel = 0;
    if (!parseDecimal(argv[1], 0, 127, note)) return ArgStatus::Invalid;
    if (!parseDecimal(argv[2], 1, 127, velocity)) return ArgStatus::Invalid;
    if (argc >= 4 && !parseDecimal(argv[3], 0, 15, channel)) return ArgStatus::Invalid;
    out.note = static_cast<uint8_t>(note);
    out.velocity = static_cast<uint8_t>(velocity);
    out.channel = static_cast<uint8_t>(channel);
    return ArgStatus::Ok;
}

ArgStatus parseNoteOffArgs(int argc, char** argv, NoteOffArgs& out) {
    if (argc < 2) return ArgStatus::Missing;
    long note = 0;
    long channel = 0;
    if (!parseDecimal(argv[1], 0, 127, note)) return ArgStatus::Invalid;
    if (argc >= 3 && !parseDecimal(argv[2], 0, 15, channel)) return ArgStatus::Invalid;
    out.note = static_cast<uint8_t>(note);
    out.channel = static_cast<uint8_t>(channel);
    return ArgStatus::Ok;
}

} // namespace smk
