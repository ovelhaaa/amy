#pragma once
#include "note_routing.h"
#include <atomic>

namespace smk {

// Portable requested state. This does not allocate instruments or acknowledge
// engine changes. Only a backend with validated resources may activate B.
enum class PerformanceMode : uint8_t { Single, Layer, Split };
struct KeyboardZone {
    uint8_t low_note = 0, high_note = 127;
    uint8_t low_velocity = 1, high_velocity = 127;
    uint16_t midi_channel_mask = 0xffff; // Bit 0 = MIDI channel 1.
};
struct LayerConfig {
    bool enabled = true;
    bool mute = false; // Suppress new onsets; accepted notes keep their release.
    KeyboardZone zone{};
    int8_t transpose = 0;
    uint8_t volume = 127; // Independent linear gain request, not velocity scaling.
    uint16_t patch = config::kDefaultPatchNumber;
};
struct PerformanceConfig {
    PerformanceMode mode = PerformanceMode::Single;
    LayerConfig layers[2]{};
    PerformanceConfig() { layers[1].enabled = false; }
};

inline bool validPerformance(const PerformanceConfig& config) {
    if (config.mode > PerformanceMode::Split) return false;
    for (const auto& layer : config.layers) {
        const auto& z = layer.zone;
        if (z.low_note > z.high_note || z.high_note > 127 ||
            z.low_velocity < 1 || z.low_velocity > z.high_velocity ||
            z.high_velocity > 127 || !z.midi_channel_mask ||
            layer.transpose < -24 || layer.transpose > 24 || layer.volume > 127)
            return false;
    }
    // Patch IDs are opaque uint16 keys; the backend must validate availability.
    return true;
}

enum class PerformanceAction : uint8_t {
    SelectLayer, SetMode, SetSplitPoint, SetEnabled, SetMute,
    SetVolume, SetTranspose, SetPatch, SetZone
};
struct PerformanceCommand {
    PerformanceAction action = PerformanceAction::SelectLayer;
    uint8_t layer = 0;
    int32_t value = 0; // Validate before narrowing console/UI input.
    KeyboardZone zone{};
};

// One control owner calls this function. All changes are transactional and
// affect new notes only. An engine patch change needs a separate backend ACK.
inline bool applyPerformanceCommand(PerformanceConfig& config, uint8_t& selected,
                                    const PerformanceCommand& command) {
    if (!validPerformance(config) || selected > 1 || command.layer > 1) return false;
    auto next = config;
    auto& layer = next.layers[command.layer];
    const int32_t value = command.value;
    switch (command.action) {
        case PerformanceAction::SelectLayer:
            selected = command.layer;
            return true;
        case PerformanceAction::SetMode:
            if (value < 0 || value > 2) return false;
            next.mode = static_cast<PerformanceMode>(value);
            break;
        case PerformanceAction::SetSplitPoint:
            if (value < 1 || value > 127) return false;
            next.mode = PerformanceMode::Split;
            next.layers[0].zone.low_note = 0;
            next.layers[0].zone.high_note = static_cast<uint8_t>(value - 1);
            next.layers[1].zone.low_note = static_cast<uint8_t>(value);
            next.layers[1].zone.high_note = 127;
            break;
        case PerformanceAction::SetEnabled:
        case PerformanceAction::SetMute:
            if (value < 0 || value > 1) return false;
            if (command.action == PerformanceAction::SetEnabled) layer.enabled = value != 0;
            else layer.mute = value != 0;
            break;
        case PerformanceAction::SetVolume:
            if (value < 0 || value > 127) return false;
            layer.volume = static_cast<uint8_t>(value);
            break;
        case PerformanceAction::SetTranspose:
            if (value < -24 || value > 24) return false;
            layer.transpose = static_cast<int8_t>(value);
            break;
        case PerformanceAction::SetPatch:
            if (value < 0 || value > UINT16_MAX) return false;
            layer.patch = static_cast<uint16_t>(value);
            break;
        case PerformanceAction::SetZone: layer.zone = command.zone; break;
        default: return false;
    }
    if (!validPerformance(next)) return false;
    config = next;
    return true;
}

// Copy-only snapshot contract. Calling a getter concurrently on mutable state
// is NOT a publication mechanism; use the SPSC mailbox below.
struct PerformanceState {
    PerformanceConfig config{};
    uint8_t selected_layer = 0;
    bool transport_running = false;
    bool ready[2]{false, false};
    uint16_t note_references[2]{}; // Logical ownership, including sustain.
    uint8_t allocated_voices[2]{}; // Physical slots, not active-note count.
    uint32_t allocation_refusals = 0;
};

// Exactly one control producer and one UI consumer. Ownership of the slot
// changes only via release/acquire. Full snapshots are never overwritten;
// producer retries with its latest state on a later tick. No wait/spin or heap.
class PerformanceMailbox {
public:
    bool tryPublish(const PerformanceState& state) {
        if (full_.load(std::memory_order_acquire)) return false;
        state_ = state;
        full_.store(true, std::memory_order_release);
        return true;
    }
    bool tryRead(PerformanceState& out) {
        if (!full_.load(std::memory_order_acquire)) return false;
        out = state_;
        full_.store(false, std::memory_order_release);
        return true;
    }
private:
    static_assert(std::atomic<bool>::is_always_lock_free, "snapshot flag must be lock-free");
    PerformanceState state_{};
    std::atomic<bool> full_{false};
};

struct PerformanceRoute {
    NoteRoute notes{};
    // Metadata for a future backend; the current AMY adapter cannot consume
    // this plan as a dual-layer transaction. Do not discard this distinction.
    uint8_t volumes[2]{};
    uint16_t patches[2]{};
};

// Explicit melodic route only. Drums, pad chords and sequencer tracks keep
// their existing routes; a caller must deliberately opt them into performance.
// Ready bits come from verified backend allocation, never just config.enabled.
inline PerformanceRoute routePerformance(const PerformanceConfig& config,
                                         const NoteIdentity& identity, uint8_t velocity,
                                         bool a_ready, bool b_ready) {
    PerformanceRoute result{};
    result.notes.identity = identity;
    result.notes.velocity = velocity;
    if (!validPerformance(config) || !NoteOwnership::valid(identity) ||
        !velocity || velocity > 127) return result;
    const bool ready[2]{a_ready, b_ready};
    for (uint8_t i = 0; i < 2; ++i) {
        const auto& layer = config.layers[i];
        const auto& z = layer.zone;
        if ((i && config.mode == PerformanceMode::Single) ||
            !layer.enabled || layer.mute || identity.input_note < z.low_note ||
            identity.input_note > z.high_note || velocity < z.low_velocity ||
            velocity > z.high_velocity || !(z.midi_channel_mask & (1u << identity.channel))) continue;
        const int note = static_cast<int>(identity.input_note) + layer.transpose;
        // AMY uses note 0 as sentinel. Preserve the existing adapter restriction.
        if (note <= 0 || note > 127) continue;
        // A matching requested destination must exist before publishing any
        // part of this onset. Never silently degrade an overlap to one layer.
        if (!ready[i]) {
            PerformanceRoute rejected{};
            rejected.notes.identity = identity;
            rejected.notes.velocity = velocity;
            return rejected;
        }
        const auto out = result.notes.count++;
        result.notes.outputs[out] = {static_cast<uint8_t>(i + 1), static_cast<uint8_t>(note)};
        result.volumes[out] = layer.volume;
        result.patches[out] = layer.patch;
    }
    return result;
}

// Necessary arithmetic check, not a certification of allocator contiguity,
// heap, render deadline or patch validity. Zero/unknown costs fail closed.
inline bool fitsPerformanceOscillators(uint8_t a_voices, uint16_t a_oscs_per_voice,
                                      uint8_t b_voices, uint16_t b_oscs_per_voice) {
    if (!a_voices || a_voices > config::kPerformanceMaxVoicesPerLayer || !a_oscs_per_voice ||
        b_voices > config::kPerformanceMaxVoicesPerLayer ||
        (b_voices && !b_oscs_per_voice)) return false;
    const uint32_t required = config::kDrumOscillatorBudget + static_cast<uint32_t>(a_voices) * a_oscs_per_voice +
                              static_cast<uint32_t>(b_voices) * b_oscs_per_voice;
    return required <= config::kMaxOscillators;
}
} // namespace smk
