#pragma once
#include "synth_config.h"
#include <cstddef>
#include <cstdint>

namespace smk {

enum class NoteSource : uint8_t { Keyboard, Pad, Arpeggiator, Sequencer, Internal, ExternalMidi, Count };
struct NoteIdentity {
    NoteSource source = NoteSource::Internal;
    uint8_t instance = 0;
    uint8_t channel = 0;
    uint8_t input_note = 0;
    // Zero: physical MIDI (idempotent while held). Generated notes use a
    // monotonically increasing token, never reset with transport/patch state.
    uint64_t token = 0;
};
struct NoteOutput {
    uint8_t instrument = 1;
    uint8_t note = 0;
};
struct NoteRoute {
    NoteIdentity identity{};
    NoteOutput outputs[config::kNoteFanOut]{};
    uint8_t count = 0;
    uint8_t velocity = 0;
};

// Single writer: synthesis owner. Callbacks reach the existing AMY adapter,
// which aggregates physical voices by instrument/pitch. No heap, locks or I/O.
class NoteOwnership {
public:
    enum class Result { Accepted, Duplicate, Stale, Full, Invalid };
    static bool valid(const NoteIdentity& key) {
        return key.source < NoteSource::Count && key.instance < config::kNoteSourceInstances &&
               key.channel < 16 && key.input_note < 128;
    }
    template<class On> Result on(const NoteRoute& route, On emit) {
        const auto& key = route.identity;
        if (!valid(key) || !route.count || route.count > config::kNoteFanOut ||
            !route.velocity || route.velocity > 127) return Result::Invalid;
        for (size_t i = 0; i < route.count; ++i)
            if ((route.outputs[i].instrument != 1 && route.outputs[i].instrument != 10) ||
                route.outputs[i].note == 0 || route.outputs[i].note > 127) return Result::Invalid;
        auto& watermark = last_token_[static_cast<size_t>(key.source)][key.instance];
        if (key.token && key.token <= watermark) return Result::Stale;
        // An idempotent physical press cannot accrue unmatched references.
        for (const auto& entry : entries_)
            if (entry.held && same(entry.route.identity, key)) return Result::Duplicate;
        // Consume tokens even on capacity rejection: a late replay cannot revive it.
        if (key.token) watermark = key.token;
        for (auto& entry : entries_) if (!entry.used) {
            entry.route = route; entry.used = entry.held = true;
            for (size_t i = 0; i < route.count; ++i) emit(route.outputs[i], route.velocity);
            return Result::Accepted;
        }
        return Result::Full;
    }
    template<class Off> void off(const NoteIdentity& key, Off emit) {
        if (!valid(key)) return;
        for (auto& entry : entries_) if (entry.held && same(entry.route.identity, key)) {
            entry.held = false;
            if (!sustain_[key.channel]) release(entry, emit);
            return;
        }
    }
    template<class Off> void pedal(uint8_t channel, bool down, Off emit) {
        if (channel >= 16) return;
        sustain_[channel] = down;
        if (!down) for (auto& entry : entries_)
            if (entry.used && !entry.held && entry.route.identity.channel == channel) release(entry, emit);
    }
    template<class Off> void cancel(NoteSource source, Off emit) {
        for (auto& entry : entries_)
            if (entry.used && entry.route.identity.source == source) release(entry, emit);
    }
    void clear() {
        for (auto& entry : entries_) entry.used = entry.held = false;
        for (auto& pedal : sustain_) pedal = false;
        // Preserve replay protection across Panic, patch and transport changes.
    }
    size_t size() const { size_t n = 0; for (const auto& entry : entries_) n += entry.used; return n; }
private:
    struct Entry { NoteRoute route{}; bool used = false, held = false; };
    Entry entries_[config::kNoteIdentityCapacity]{};
    uint64_t last_token_[static_cast<size_t>(NoteSource::Count)][config::kNoteSourceInstances]{};
    bool sustain_[16]{};
    static bool same(const NoteIdentity& a, const NoteIdentity& b) {
        return a.source == b.source && a.instance == b.instance && a.channel == b.channel &&
               a.input_note == b.input_note && a.token == b.token;
    }
    template<class Off> static void release(Entry& entry, Off emit) {
        for (size_t i = 0; i < entry.route.count; ++i) emit(entry.route.outputs[i]);
        entry.used = entry.held = false;
    }
};
} // namespace smk
