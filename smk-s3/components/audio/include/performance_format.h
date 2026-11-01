#pragma once
#include "performance.h"

namespace smk {
// Separate envelope, never reinterpret an existing patch/scene/profile file.
// LE: magic u32, version u16, payload bytes u16, payload CRC32 u32;
// mode u8; twice: enabled u8, mute u8, note lo/hi u8, velocity lo/hi u8,
// channel mask u16, transpose+24 u8, volume u8, patch u16.
constexpr uint32_t kPerformanceMagic = 0x46504d53; // Bytes "SMPF".
constexpr uint16_t kPerformanceVersion = 1;
constexpr size_t kPerformancePayloadBytes = 25;
constexpr size_t kPerformanceBytes = 12 + kPerformancePayloadBytes;

inline uint32_t performanceCrc32(const uint8_t* bytes, size_t size) {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xedb88320u : 0u);
    }
    return ~crc;
}
namespace performance_format {
inline void put(uint8_t* out, uint32_t value, unsigned count) {
    for (unsigned i = 0; i < count; ++i) out[i] = static_cast<uint8_t>(value >> (8 * i));
}
inline uint32_t get(const uint8_t* in, unsigned count) {
    uint32_t result = 0;
    for (unsigned i = 0; i < count; ++i) result |= static_cast<uint32_t>(in[i]) << (8 * i);
    return result;
}
}

inline bool encodePerformance(const PerformanceConfig& config, uint8_t* out, size_t size) {
    if (!out || size != kPerformanceBytes || !validPerformance(config)) return false;
    using namespace performance_format;
    put(out, kPerformanceMagic, 4);
    put(out + 4, kPerformanceVersion, 2);
    put(out + 6, kPerformancePayloadBytes, 2);
    out[12] = static_cast<uint8_t>(config.mode);
    size_t pos = 13;
    for (const auto& layer : config.layers) {
        out[pos++] = layer.enabled ? 1 : 0;
        out[pos++] = layer.mute ? 1 : 0;
        out[pos++] = layer.zone.low_note;
        out[pos++] = layer.zone.high_note;
        out[pos++] = layer.zone.low_velocity;
        out[pos++] = layer.zone.high_velocity;
        put(out + pos, layer.zone.midi_channel_mask, 2); pos += 2;
        out[pos++] = static_cast<uint8_t>(layer.transpose + 24);
        out[pos++] = layer.volume;
        put(out + pos, layer.patch, 2); pos += 2;
    }
    put(out + 8, performanceCrc32(out + 12, kPerformancePayloadBytes), 4);
    return true;
}

// Rejection leaves caller state unchanged. Caller chooses embedded defaults
// on startup failure; never silently replaces a live config after bad input.
inline bool decodePerformance(const uint8_t* in, size_t size, PerformanceConfig& out) {
    if (!in || size != kPerformanceBytes) return false;
    using namespace performance_format;
    if (get(in, 4) != kPerformanceMagic || get(in + 4, 2) != kPerformanceVersion ||
        get(in + 6, 2) != kPerformancePayloadBytes ||
        get(in + 8, 4) != performanceCrc32(in + 12, kPerformancePayloadBytes)) return false;
    PerformanceConfig next{};
    next.mode = static_cast<PerformanceMode>(in[12]);
    size_t pos = 13;
    for (auto& layer : next.layers) {
        if (in[pos] > 1 || in[pos + 1] > 1) return false;
        layer.enabled = in[pos++] != 0;
        layer.mute = in[pos++] != 0;
        layer.zone.low_note = in[pos++];
        layer.zone.high_note = in[pos++];
        layer.zone.low_velocity = in[pos++];
        layer.zone.high_velocity = in[pos++];
        layer.zone.midi_channel_mask = static_cast<uint16_t>(get(in + pos, 2)); pos += 2;
        if (in[pos] > 48) return false;
        layer.transpose = static_cast<int8_t>(static_cast<int>(in[pos++]) - 24);
        layer.volume = in[pos++];
        layer.patch = static_cast<uint16_t>(get(in + pos, 2)); pos += 2;
    }
    if (!validPerformance(next)) return false;
    out = next;
    return true;
}

// Call only AFTER the existing legacy patch loader has validated/migrated its
// own envelope and CRC. No changes to v3/v4/v5/v6 or scene/profile formats.
inline PerformanceConfig performanceForLegacyPatch(uint16_t engine_patch) {
    PerformanceConfig result{};
    result.layers[0].patch = engine_patch;
    return result;
}
} // namespace smk
