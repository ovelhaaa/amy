#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace smk {

// ─────────────────────────────────────────────────────────────
// Perceptual control mappings shared by knob processing, soft takeover and
// the UI engine values.
//
// Every mapping is a pure monotonic pair: forward(norm) turns the physical
// knob position (0..1) into the engine value, inverse(value) recovers the knob
// position. Soft takeover must use the same inverse so pickup happens exactly
// where the saved value sits on the knob. None of these are applied to values
// loaded from a patch: a saved 4000 Hz cutoff is still applied as 4000 Hz.
// The curve only defines which physical knob position corresponds to it.
// ─────────────────────────────────────────────────────────────
namespace control_ranges {
constexpr float kCutoffMinHz   = 20.0f;
constexpr float kCutoffMaxHz   = 18000.0f;
constexpr float kEnvelopeMinMs = 1.0f;
constexpr float kEnvelopeMaxMs = 5000.0f;
constexpr float kResonanceMin  = 0.5f;
constexpr float kResonanceMax  = 10.0f;
constexpr float kDelayMinMs    = 10.0f;
constexpr float kDelayMaxMs    = 1000.0f; // free-running delay knob range
constexpr float kDelaySyncMaxMs = 1200.0f; // BPM-synced delay upper bound
// Highest musically safe echo feedback. The mapping already uses this; the
// engine clamp (AmyAdapter::executeDelay) re-asserts it so a hand-authored or
// migrated value can never drive runaway feedback.
constexpr float kMaxDelayFeedback = 0.95f;
} // namespace control_ranges

// Filter cutoff: geometric sweep 20 Hz .. 18 kHz, so the knob midpoint lands
// near 600 Hz instead of the ~9 kHz a linear sweep would give.
inline float cutoffFromNorm(float norm) {
    norm = std::clamp(norm, 0.0f, 1.0f);
    const float ratio = control_ranges::kCutoffMaxHz / control_ranges::kCutoffMinHz;
    return control_ranges::kCutoffMinHz * std::pow(ratio, norm);
}
inline float cutoffToNorm(float cutoff_hz) {
    cutoff_hz = std::clamp(cutoff_hz, control_ranges::kCutoffMinHz, control_ranges::kCutoffMaxHz);
    const float ratio = control_ranges::kCutoffMaxHz / control_ranges::kCutoffMinHz;
    return std::log(cutoff_hz / control_ranges::kCutoffMinHz) / std::log(ratio);
}

// Envelope times: geometric sweep 1 ms .. 5000 ms. The first half of the knob
// resolves 1 ms .. ~70 ms, the second half eases into long envelopes.
inline float envelopeMsFromNorm(float norm) {
    norm = std::clamp(norm, 0.0f, 1.0f);
    const float ratio = control_ranges::kEnvelopeMaxMs / control_ranges::kEnvelopeMinMs;
    return control_ranges::kEnvelopeMinMs * std::pow(ratio, norm);
}
inline float envelopeMsToNorm(float ms) {
    ms = std::clamp(ms, control_ranges::kEnvelopeMinMs, control_ranges::kEnvelopeMaxMs);
    const float ratio = control_ranges::kEnvelopeMaxMs / control_ranges::kEnvelopeMinMs;
    return std::log(ms / control_ranges::kEnvelopeMinMs) / std::log(ratio);
}

// Resonance: same 0.5 .. 10.0 DSP range, squared response for finer resolution
// in the low/mid zone.
inline float resonanceFromNorm(float norm) {
    norm = std::clamp(norm, 0.0f, 1.0f);
    const float shaped = norm * norm;
    return control_ranges::kResonanceMin +
           shaped * (control_ranges::kResonanceMax - control_ranges::kResonanceMin);
}
inline float resonanceToNorm(float resonance) {
    resonance = std::clamp(resonance, control_ranges::kResonanceMin, control_ranges::kResonanceMax);
    const float range = control_ranges::kResonanceMax - control_ranges::kResonanceMin;
    return std::sqrt((resonance - control_ranges::kResonanceMin) / range);
}

// Drive: squared response; fine control at low saturation, still reaches max.
inline float driveFromNorm(float norm) {
    norm = std::clamp(norm, 0.0f, 1.0f);
    return norm * norm;
}
inline float driveToNorm(float drive) {
    drive = std::clamp(drive, 0.0f, 1.0f);
    return std::sqrt(drive);
}

// Free delay time: geometric sweep 10 ms .. 1000 ms for slapback/doubling
// resolution. BPM-synced delay uses its own discrete selection.
inline float delayMsFromNorm(float norm) {
    norm = std::clamp(norm, 0.0f, 1.0f);
    const float ratio = control_ranges::kDelayMaxMs / control_ranges::kDelayMinMs;
    return control_ranges::kDelayMinMs * std::pow(ratio, norm);
}
inline float delayMsToNorm(float delay_ms) {
    delay_ms = std::clamp(delay_ms, control_ranges::kDelayMinMs, control_ranges::kDelayMaxMs);
    const float ratio = control_ranges::kDelayMaxMs / control_ranges::kDelayMinMs;
    return std::log(delay_ms / control_ranges::kDelayMinMs) / std::log(ratio);
}

// BPM-synced delay subdivisions, in beats (quarter notes). The order is the
// knob sweep order and is shared by the control path and the tests so the two
// can never disagree. It intentionally preserves the historical multipliers
// (0.25, 1/3, 0.5, 0.75, 1, 1.5, 2) but names them as musical divisions.
struct DelayDivision {
    const char* name;
    float       beats;
};
constexpr int kDelayDivisionCount = 7;
constexpr DelayDivision kDelayDivisions[kDelayDivisionCount] = {
    { "1/16", 0.25f },
    { "1/8T", 1.0f / 3.0f },
    { "1/8",  0.5f },
    { "1/8D", 0.75f },
    { "1/4",  1.0f },
    { "1/4D", 1.5f },
    { "1/2",  2.0f },
};

// Wet FX amount (delay mix, reverb mix, chorus depth). Squared so a mid knob
// stays in the musically useful low-wet region and does not jump to an
// exaggeratedly wet mix, while still reaching 100%.
inline float wetFromNorm(float norm) {
    norm = std::clamp(norm, 0.0f, 1.0f);
    return norm * norm;
}
inline float wetToNorm(float wet) {
    wet = std::clamp(wet, 0.0f, 1.0f);
    return std::sqrt(wet);
}

} // namespace smk
