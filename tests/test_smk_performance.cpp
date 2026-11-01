#include "performance_format.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include <thread>

using namespace smk;
using Bytes = std::array<uint8_t, kPerformanceBytes>;
static NoteIdentity key(uint8_t note = 60, uint8_t channel = 0) {
    return {NoteSource::Keyboard, 0, channel, note, 0};
}
static Bytes bytes(const PerformanceConfig& config) {
    Bytes result{};
    assert(encodePerformance(config, result.data(), result.size()));
    return result;
}
static void routing() {
    PerformanceConfig c;
    assert(validPerformance(c));
    for (unsigned note = 1; note < 128; ++note)
        for (unsigned ch = 0; ch < 16; ++ch) {
            auto r = routePerformance(c, key(note, ch), 100, true, true);
            assert(r.notes.count == 1 && r.notes.outputs[0].instrument == 1);
            assert(r.notes.outputs[0].note == note && r.notes.velocity == 100);
        }
    c.layers[1].enabled = true;
    assert(routePerformance(c, key(), 100, true, true).notes.count == 1); // Single.
    c.mode = PerformanceMode::Layer;
    auto r = routePerformance(c, key(), 100, true, true);
    assert(r.notes.count == 2 && r.notes.outputs[1].instrument == 2);
    assert(r.notes.outputs[0].note == 60 && r.notes.outputs[1].note == 60);
    assert(routePerformance(c, key(), 100, true, false).notes.count == 0);
    assert(routePerformance(c, key(), 100, false, true).notes.count == 0);
    assert(routePerformance(c, key(), 100, false, false).notes.count == 0);
    c.layers[0].zone = {0, 59, 1, 127, 0xffff};
    c.layers[1].zone = {60, 127, 1, 127, 0xffff};
    c.mode = PerformanceMode::Split;
    assert(routePerformance(c, key(59), 100, true, true).notes.outputs[0].instrument == 1);
    assert(routePerformance(c, key(60), 100, true, true).notes.outputs[0].instrument == 2);
    // An unavailable nonmatching split destination does not suppress A.
    assert(routePerformance(c, key(59), 100, true, false).notes.count == 1);
    assert(routePerformance(c, key(60), 100, true, false).notes.count == 0);
    c.layers[0].zone.high_note = 64; // Overlap is intentional.
    assert(routePerformance(c, key(), 100, true, true).notes.count == 2);
    assert(routePerformance(c, key(), 100, true, false).notes.count == 0);
    c.layers[1].mute = true;
    assert(routePerformance(c, key(), 100, true, false).notes.count == 1);
    c.layers[1].mute = false;
    c.layers[0].zone.high_note = 58;
    c.layers[1].zone.low_note = 61;
    assert(routePerformance(c, key(), 100, true, true).notes.count == 0); // Gap.
    c.layers[0].zone = {};
    c.layers[1].zone = {0, 127, 64, 100, 0x8000};
    for (unsigned ch = 0; ch < 16; ++ch)
        for (unsigned vel = 1; vel < 128; ++vel) {
            r = routePerformance(c, key(60, ch), vel, true, true);
            assert(r.notes.count == ((ch == 15 && vel >= 64 && vel <= 100) ? 2 : 1));
        }
    c.layers[1].zone = {};
    c.layers[0].transpose = -24;
    c.layers[1].transpose = 24;
    for (unsigned note = 0; note < 128; ++note) {
        r = routePerformance(c, key(note), 100, true, true);
        assert(r.notes.count == (note > 24 ? 1 : 0) + (note <= 103 ? 1 : 0));
        for (unsigned i = 0; i < r.notes.count; ++i)
            assert(r.notes.outputs[i].note > 0 && r.notes.outputs[i].note <= 127);
    }
    assert(routePerformance(c, key(), 0, true, true).notes.count == 0);
    assert(routePerformance(c, key(), 128, true, true).notes.count == 0);
    assert(routePerformance(c, key(128), 100, true, true).notes.count == 0);
    assert(routePerformance(c, key(60, 16), 100, true, true).notes.count == 0);
    c.layers[0].mute = true;
    assert(routePerformance(c, key(), 100, true, true).notes.count == 1);
    c.layers[1].enabled = false;
    assert(routePerformance(c, key(), 100, true, true).notes.count == 0);
}

static void commands() {
    PerformanceConfig c;
    uint8_t selected = 0;
    auto apply = [&](PerformanceAction a, uint8_t layer, int32_t value) {
        return applyPerformanceCommand(c, selected, {a, layer, value, {}});
    };
    assert(apply(PerformanceAction::SelectLayer, 1, 0) && selected == 1);
    assert(apply(PerformanceAction::SetSplitPoint, 0, 60));
    assert(c.mode == PerformanceMode::Split && c.layers[0].zone.high_note == 59 &&
           c.layers[1].zone.low_note == 60 && !c.layers[1].enabled);
    assert(apply(PerformanceAction::SetEnabled, 1, 1));
    assert(apply(PerformanceAction::SetVolume, 1, 64));
    assert(apply(PerformanceAction::SetTranspose, 1, -12));
    assert(apply(PerformanceAction::SetPatch, 1, 65535)); // Backend validates ID.
    auto r = routePerformance(c, key(), 100, true, true);
    assert(r.notes.count == 1 && r.notes.outputs[0].note == 48);
    assert(r.volumes[0] == 64 && r.patches[0] == 65535);
    assert(apply(PerformanceAction::SetMute, 1, 1));
    const auto original = bytes(c);
    const PerformanceCommand bad[] = {
        {PerformanceAction::SetMode, 0, -1, {}},
        {PerformanceAction::SetMode, 0, 3, {}},
        {PerformanceAction::SetSplitPoint, 0, 0, {}},
        {PerformanceAction::SetSplitPoint, 0, 128, {}},
        {PerformanceAction::SetEnabled, 1, 2, {}},
        {PerformanceAction::SetMute, 1, -1, {}},
        {PerformanceAction::SetVolume, 1, 256, {}},
        {PerformanceAction::SetTranspose, 1, -25, {}},
        {PerformanceAction::SetTranspose, 1, 25, {}},
        {PerformanceAction::SetPatch, 1, 65536, {}},
        {PerformanceAction::SetPatch, 1, -1, {}},
        {PerformanceAction::SelectLayer, 2, 0, {}},
        {static_cast<PerformanceAction>(255), 0, 0, {}},
        {PerformanceAction::SetZone, 0, 0, {70, 60, 1, 127, 0xffff}},
        {PerformanceAction::SetZone, 0, 0, {0, 127, 0, 127, 0xffff}},
        {PerformanceAction::SetZone, 0, 0, {0, 127, 1, 127, 0}}
    };
    for (const auto& command : bad) {
        assert(!applyPerformanceCommand(c, selected, command));
        assert(bytes(c) == original && selected == 1);
    }
    static_assert(std::is_trivially_copyable<PerformanceState>::value, "snapshot must copy by value");
    PerformanceState state;
    state.config = c;
    auto copy = state;
    copy.config.layers[0].volume = 0;
    assert(copy.config.layers[0].volume == 0 && state.config.layers[0].volume == 127);
}

static void release_captured_route() {
    PerformanceConfig c;
    c.layers[0].transpose = 12;
    NoteOwnership owner;
    unsigned on = 0, off = 0;
    const auto r = routePerformance(c, key(), 100, true, false);
    assert(owner.on(r.notes, [&](NoteOutput output, uint8_t) {
        assert(output.note == 72 && output.instrument == 1); ++on;
    }) == NoteOwnership::Result::Accepted);
    owner.pedal(0, true, [&](NoteOutput) { assert(false); });
    c.mode = PerformanceMode::Split;
    c.layers[0].zone.high_note = 40;
    c.layers[0].transpose = -12;
    c.layers[0].mute = true;
    c.layers[0].enabled = false;
    assert(routePerformance(c, key(), 100, true, false).notes.count == 0);
    owner.off(key(), [&](NoteOutput) { assert(false); });
    assert(owner.size() == 1);
    owner.pedal(0, false, [&](NoteOutput output) {
        assert(output.note == 72 && output.instrument == 1); ++off;
    });
    assert(on == 1 && off == 1 && owner.size() == 0);
    owner.off(key(), [&](NoteOutput) { assert(false); });
    // Gate is deliberate: production ownership still rejects synth 2.
    c = PerformanceConfig{}; c.mode = PerformanceMode::Layer; c.layers[1].enabled = true;
    assert(owner.on(routePerformance(c, key(), 100, true, true).notes,
                    [&](NoteOutput, uint8_t) { assert(false); }) == NoteOwnership::Result::Invalid);
}

static void persistence() {
    assert(performanceCrc32(reinterpret_cast<const uint8_t*>("123456789"), 9) == 0xcbf43926u);
    PerformanceConfig c;
    c.mode = PerformanceMode::Layer;
    c.layers[1].enabled = true;
    c.layers[0].transpose = -24;
    c.layers[1].transpose = 24;
    c.layers[1].patch = 0x1234;
    c.layers[1].zone.midi_channel_mask = 0x8001;
    auto encoded = bytes(c);
    assert(encoded[0] == 'S' && encoded[1] == 'M' && encoded[2] == 'P' && encoded[3] == 'F');
    assert(encoded[4] == 1 && encoded[5] == 0 && encoded[6] == 25 && encoded[7] == 0);
    assert(encoded[21] == 0 && encoded[33] == 48);
    assert(encoded[35] == 0x34 && encoded[36] == 0x12);
    PerformanceConfig out;
    assert(decodePerformance(encoded.data(), encoded.size(), out));
    assert(bytes(out) == encoded);
    const auto saved = bytes(out);
    // Every single-bit corruption and every interrupted/truncated write fail
    // without exposing a partial configuration or changing caller state.
    for (size_t i = 0; i < encoded.size(); ++i) for (unsigned bit = 0; bit < 8; ++bit) {
        auto corrupt = encoded; corrupt[i] ^= 1u << bit;
        assert(!decodePerformance(corrupt.data(), corrupt.size(), out));
        assert(bytes(out) == saved);
    }
    for (size_t n = 0; n < encoded.size(); ++n)
        assert(!decodePerformance(encoded.data(), n, out));
    assert(!decodePerformance(encoded.data(), encoded.size() + 1, out));
    assert(!decodePerformance(nullptr, encoded.size(), out));
    // Valid CRC cannot make invalid fields safe.
    const std::pair<size_t, uint8_t> invalid[] = {{12, 3}, {13, 2}, {14, 2}, {15, 128},
        {17, 0}, {19, 0}, {21, 49}, {22, 128}};
    for (const auto& field : invalid) {
        auto corrupt = encoded;
        corrupt[field.first] = field.second;
        if (field.first == 19) corrupt[20] = 0; // Empty mask.
        performance_format::put(corrupt.data() + 8,
            performanceCrc32(corrupt.data() + 12, kPerformancePayloadBytes), 4);
        assert(!decodePerformance(corrupt.data(), corrupt.size(), out));
        assert(bytes(out) == saved);
    }
    auto legacy = performanceForLegacyPatch(230);
    assert(legacy.mode == PerformanceMode::Single && legacy.layers[0].patch == 230 &&
           !legacy.layers[1].enabled && legacy.layers[1].transpose == 0);
    assert(bytes(PerformanceConfig{}) == bytes(performanceForLegacyPatch(config::kDefaultPatchNumber)));
    auto bad = c; bad.layers[0].zone.high_velocity = 0;
    auto destination = encoded;
    assert(!encodePerformance(bad, destination.data(), destination.size()) && destination == encoded);
}

static void publication() {
    PerformanceMailbox mailbox;
    PerformanceState out;
    assert(!mailbox.tryRead(out));
    PerformanceState initial;
    initial.config.layers[0].patch = 230;
    assert(mailbox.tryPublish(initial));
    initial.config.layers[0].patch = 0;
    assert(!mailbox.tryPublish(initial));
    assert(mailbox.tryRead(out) && out.config.layers[0].patch == 230);
    assert(!mailbox.tryRead(out));
    std::thread producer([&] {
        for (uint32_t i = 1; i <= 10000; ++i) {
            PerformanceState state;
            state.allocation_refusals = i;
            state.config.layers[0].patch = static_cast<uint16_t>(i);
            state.note_references[0] = static_cast<uint16_t>(i);
            state.config.layers[1].transpose = static_cast<int8_t>(static_cast<int>(i % 49) - 24);
            while (!mailbox.tryPublish(state)) std::this_thread::yield();
        }
    });
    for (uint32_t i = 1; i <= 10000; ++i) {
        while (!mailbox.tryRead(out)) std::this_thread::yield();
        assert(out.allocation_refusals == i && out.config.layers[0].patch == i &&
               out.note_references[0] == i &&
               out.config.layers[1].transpose == static_cast<int>(i % 49) - 24);
    }
    producer.join();
}

int main() {
    routing(); commands(); release_captured_route(); persistence(); publication();
    assert(fitsPerformanceOscillators(8, 6, 0, 0));
    assert(!fitsPerformanceOscillators(8, 6, 8, 8));
    assert(!fitsPerformanceOscillators(8, 8, 8, 8));
    assert(fitsPerformanceOscillators(4, 8, 4, 8)); // Arithmetic only.
    assert(!fitsPerformanceOscillators(0, 6, 4, 8));
    assert(!fitsPerformanceOscillators(8, 0, 4, 8));
    assert(!fitsPerformanceOscillators(8, 6, 4, 0));
    assert(!fitsPerformanceOscillators(33, 1, 0, 0));
    std::printf("PASS: portable performance routes, commands, captured releases, resource gate and v1 CRC codec (%zu bytes)\n", kPerformanceBytes);
    std::printf("Host sizeof: config=%zu state=%zu mailbox=%zu route=%zu; no runtime instance added\n",
                sizeof(PerformanceConfig), sizeof(PerformanceState), sizeof(PerformanceMailbox), sizeof(PerformanceRoute));
}
