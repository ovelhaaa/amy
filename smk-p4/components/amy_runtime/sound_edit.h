#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>
namespace smk::p4::edit {
using Mask = uint64_t;
constexpr Mask bit(unsigned p) { return Mask(1) << p; }

// Stable persisted IDs. Operator IDs follow materialized algo_source order (OP1..OP6).
// IDs 0..28 are frozen for V1 (0..16) and legacy V2 (0..28) compatibility.
enum Param : uint8_t {
    Cutoff = 0, Resonance = 1, Attack = 2, Release = 3,
    OpLevel0 = 4, OpLevel5 = 9,
    OpRatio0 = 10, OpRatio5 = 15,
    Depth = 16,
    Decay = 17, Sustain = 18, FilterEnv = 19, FilterKeytrack = 20,
    OscLevel = 21, SubLevel = 22, NoiseLevel = 23, PulseWidth = 24, Detune = 25,
    PatchLevel = 26, Pan = 27, Velocity = 28,
    FmFeedback = 29,
    OpAttack0 = 30, OpAttack5 = 35,
    OpDecay0 = 36, OpDecay5 = 41,
    OpVel0 = 42, OpVel5 = 47,
    LfoRate = 48, LfoDepth = 49, LfoWave = 50, LfoDest = 51,
    ModEnable = 52, ModSource = 53, ModDest = 54, ModAmount = 55, ModPolarity = 56,
    Count = 57
};
static_assert(Count <= 64, "Widen edit::Mask before admitting more than 64 fields");
constexpr unsigned kSlots = 16;

struct State {
    uint16_t patch = 128;
    uint8_t family = 0;
    Mask available = 0, mask = 0;
    uint32_t revision = 0, request = 0, errors = 0, epoch = 0, apply_max_us = 0;
    uint8_t macros[3]{64, 64, 64};
    float original[Count]{}, manual[Count]{}, applied[Count]{};
    bool compare = false, dirty = false;
    int8_t user_slot = -1;
};

inline bool valid(unsigned p, float v) {
    if (!std::isfinite(v)) return false;
    if (p == Cutoff) return v >= 20.0f && v <= 18000.0f;
    if (p == Resonance) return v >= 0.1f && v <= 2.0f;
    if (p == Attack || p == Release) return v >= 1.0f && v <= 8000.0f;
    if (p == Decay) return v >= 1.0f && v <= 30000.0f;
    if (p >= OpLevel0 && p <= OpLevel5) return v >= 0.0f && v <= 100.0f;
    if (p >= OpRatio0 && p <= OpRatio5) return v >= 0.5f && v <= 16.0f;
    if (p == FilterEnv) return v >= -11.0f && v <= 11.0f;
    if (p == FilterKeytrack) return v >= 0.0f && v <= 200.0f;
    if (p == OscLevel || p == SubLevel || p == NoiseLevel || p == PatchLevel || p == Velocity) return v >= 0.0f && v <= 100.0f;
    if (p == PulseWidth) return v >= 5.0f && v <= 95.0f;
    if (p == Detune) return v >= -100.0f && v <= 100.0f;
    if (p == Pan) return v >= -64.0f && v <= 63.0f;
    if (p == Sustain) return v >= 0.0f && v <= 100.0f;
    if (p == Depth) return v >= 0.0f && v <= 1.0f;
    if (p == FmFeedback) return v >= 0.0f && v <= 100.0f;
    if (p >= OpAttack0 && p <= OpAttack5) return v >= 0.0f && v <= 2000.0f;
    if (p >= OpDecay0 && p <= OpDecay5) return v >= 0.0f && v <= 4000.0f;
    if (p >= OpVel0 && p <= OpVel5) return v >= 0.0f && v <= 100.0f;
    if (p == LfoRate) return v >= 0.1f && v <= 20.0f;
    if (p == LfoDepth) return v >= 0.0f && v <= 100.0f;
    if (p == LfoWave) return v >= 0.0f && v <= 4.0f && std::floor(v) == v;
    if (p == LfoDest) return v >= 0.0f && v <= 3.0f && std::floor(v) == v;
    if (p == ModEnable) return v == 0.0f || v == 1.0f;
    if (p == ModSource) return v == 0.0f || v == 1.0f;
    if (p == ModDest) return v == 0.0f || v == 1.0f || v == 3.0f;
    if (p == ModAmount) return v >= -100.0f && v <= 100.0f;
    if (p == ModPolarity) return v >= 0.0f && v <= 2.0f && std::floor(v) == v;
    return false;
}

struct Request {
    uint16_t patch = 0;
    uint32_t generation = 0, request = 0;
    Mask mask = 0;
    uint8_t macros[3]{64, 64, 64};
    float values[Count]{};
    bool compare = false;
    int8_t slot = -1;
};

// Single UI producer, AMY owner consumer. One request in flight; producer keeps
// latest desired complete state locally until space opens. Independent of notes.
class Mailbox {
public:
    bool push(const Request& r) {
        if (full_.load(std::memory_order_acquire)) return false;
        request_ = r;
        full_.store(true, std::memory_order_release);
        return true;
    }
    bool pop(Request& r) {
        if (!full_.load(std::memory_order_acquire)) return false;
        r = request_;
        full_.store(false, std::memory_order_release);
        return true;
    }
private:
    Request request_{};
    std::atomic<bool> full_{false};
};
}
