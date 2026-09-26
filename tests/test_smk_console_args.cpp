// Run with tests/run_smk_safety.ps1. Pure host test, no ESP dependencies.
#include "console_args.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <vector>

using namespace smk;

static ArgStatus noteOn(std::initializer_list<const char*> args, NoteOnArgs& out) {
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (const char* arg : args) argv.push_back(const_cast<char*>(arg));
    return parseNoteOnArgs(static_cast<int>(argv.size()), argv.data(), out);
}

static ArgStatus noteOff(std::initializer_list<const char*> args, NoteOffArgs& out) {
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (const char* arg : args) argv.push_back(const_cast<char*>(arg));
    return parseNoteOffArgs(static_cast<int>(argv.size()), argv.data(), out);
}

int main() {
    long value = 0;
    // parseDecimal: strict, bounded and overflow-safe.
    assert(parseDecimal("0", 0, 127, value) && value == 0);
    assert(parseDecimal("127", 0, 127, value) && value == 127);
    assert(parseDecimal("007", 0, 127, value) && value == 7);
    assert(!parseDecimal("128", 0, 127, value));
    assert(!parseDecimal("-1", 0, 127, value));
    assert(!parseDecimal("+1", 0, 127, value));
    assert(!parseDecimal("", 0, 127, value));
    assert(!parseDecimal(nullptr, 0, 127, value));
    assert(!parseDecimal(" 1", 0, 127, value));
    assert(!parseDecimal("1 ", 0, 127, value));
    assert(!parseDecimal("1.0", 0, 127, value));
    assert(!parseDecimal("12abc", 0, 127, value));
    assert(!parseDecimal("999999999999999999999999", 0, 127, value));

    NoteOnArgs on{};
    // Missing required arguments.
    assert(noteOn({}, on) == ArgStatus::Missing);
    assert(noteOn({"note_on"}, on) == ArgStatus::Missing);
    assert(noteOn({"note_on", "60"}, on) == ArgStatus::Missing);
    // Valid: channel defaults to 0.
    assert(noteOn({"note_on", "60", "100"}, on) == ArgStatus::Ok);
    assert(on.note == 60 && on.velocity == 100 && on.channel == 0);
    // Valid boundary values.
    assert(noteOn({"note_on", "0", "1", "0"}, on) == ArgStatus::Ok);
    assert(on.note == 0 && on.velocity == 1 && on.channel == 0);
    assert(noteOn({"note_on", "127", "127", "15"}, on) == ArgStatus::Ok);
    assert(on.note == 127 && on.velocity == 127 && on.channel == 15);
    // note: 128 / -1 / non-numeric / overflow rejected.
    assert(noteOn({"note_on", "128", "100"}, on) == ArgStatus::Invalid);
    assert(noteOn({"note_on", "-1", "100"}, on) == ArgStatus::Invalid);
    assert(noteOn({"note_on", "abc", "100"}, on) == ArgStatus::Invalid);
    assert(noteOn({"note_on", "99999999999999999999", "100"}, on) == ArgStatus::Invalid);
    // velocity: 0 / 128 / trailing garbage rejected.
    assert(noteOn({"note_on", "60", "0"}, on) == ArgStatus::Invalid);
    assert(noteOn({"note_on", "60", "128"}, on) == ArgStatus::Invalid);
    assert(noteOn({"note_on", "60", "1x"}, on) == ArgStatus::Invalid);
    // channel: 16 / -1 rejected.
    assert(noteOn({"note_on", "60", "100", "16"}, on) == ArgStatus::Invalid);
    assert(noteOn({"note_on", "60", "100", "-1"}, on) == ArgStatus::Invalid);

    NoteOffArgs off{};
    assert(noteOff({}, off) == ArgStatus::Missing);
    assert(noteOff({"note_off"}, off) == ArgStatus::Missing);
    assert(noteOff({"note_off", "60"}, off) == ArgStatus::Ok);
    assert(off.note == 60 && off.channel == 0);
    assert(noteOff({"note_off", "127", "15"}, off) == ArgStatus::Ok);
    assert(off.note == 127 && off.channel == 15);
    assert(noteOff({"note_off", "128"}, off) == ArgStatus::Invalid);
    assert(noteOff({"note_off", "-1"}, off) == ArgStatus::Invalid);
    assert(noteOff({"note_off", "60.5"}, off) == ArgStatus::Invalid);
    assert(noteOff({"note_off", "60", "16"}, off) == ArgStatus::Invalid);
    assert(noteOff({"note_off", "60", "nope"}, off) == ArgStatus::Invalid);

    std::puts("PASS: strict note_on/note_off argument parsing (missing, invalid, overflow, out-of-range)");
    return 0;
}
