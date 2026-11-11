#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <atomic>

extern "C" {
#include "amy.h"
void config_eq(uint8_t bus, SAMPLE eq_l, SAMPLE eq_m, SAMPLE eq_h);
}

#ifdef true
#undef true
#endif
#ifdef false
#undef false
#endif
#ifdef PCM
#undef PCM
#endif

namespace smk::p4::fx {

constexpr uint32_t kFxMagic = 0x32315846; // "FX12"
constexpr uint32_t kFxVersion = 1;
constexpr unsigned kFxRecordBytes = 32;
constexpr uint32_t kRomCompatibility = 0x3e76efb6;

enum class ChorusMode : uint8_t {
    Off = 0,
    Classic = 1,
    JunoI = 2,
    JunoII = 3,
    Ensemble = 4,
    Vibrato = 5,
    Count = 6
};

struct FxConfig {
    uint8_t chorus_mode = 0;       // ChorusMode::Off
    float chorus_depth = 0.45f;    // 0.0 .. 1.0 (45%)
    float delay_time_ms = 350.0f;  // 10.0 .. 1000.0 ms
    float delay_feedback = 0.40f;  // 0.0 .. 0.95 (40%)
    float delay_mix = 0.0f;        // 0.0 .. 1.0 (0% default neutral)
    float reverb_size = 0.70f;     // 0.0 .. 1.0 (70%)
    float reverb_mix = 0.0f;       // 0.0 .. 1.0 (0% default neutral)
    float master_tone = 0.0f;      // -1.0 .. +1.0 (0%)
    float eq_low_db = 0.0f;        // -12.0 .. +12.0 dB
    float eq_mid_db = 0.0f;        // -12.0 .. +12.0 dB
    float eq_high_db = 0.0f;       // -12.0 .. +12.0 dB
    bool bypass_all = false;
};

inline bool valid(const FxConfig& c) {
    if (c.chorus_mode >= static_cast<uint8_t>(ChorusMode::Count)) return false;
    if (!std::isfinite(c.chorus_depth) || c.chorus_depth < 0.0f || c.chorus_depth > 1.0f) return false;
    if (!std::isfinite(c.delay_time_ms) || c.delay_time_ms < 10.0f || c.delay_time_ms > 1000.0f) return false;
    if (!std::isfinite(c.delay_feedback) || c.delay_feedback < 0.0f || c.delay_feedback > 0.95f) return false;
    if (!std::isfinite(c.delay_mix) || c.delay_mix < 0.0f || c.delay_mix > 1.0f) return false;
    if (!std::isfinite(c.reverb_size) || c.reverb_size < 0.0f || c.reverb_size > 1.0f) return false;
    if (!std::isfinite(c.reverb_mix) || c.reverb_mix < 0.0f || c.reverb_mix > 1.0f) return false;
    if (!std::isfinite(c.master_tone) || c.master_tone < -1.0f || c.master_tone > 1.0f) return false;
    if (!std::isfinite(c.eq_low_db) || c.eq_low_db < -12.0f || c.eq_low_db > 12.0f) return false;
    if (!std::isfinite(c.eq_mid_db) || c.eq_mid_db < -12.0f || c.eq_mid_db > 12.0f) return false;
    if (!std::isfinite(c.eq_high_db) || c.eq_high_db < -12.0f || c.eq_high_db > 12.0f) return false;
    return true;
}

struct FxState {
    FxConfig config;
    uint32_t revision = 0;
    uint32_t request = 0;
    uint32_t errors = 0;
    uint32_t rejected = 0;
    bool enabled = true;
    bool dirty = false;
};

struct FxRequest {
    FxConfig config;
    uint32_t generation = 0;
    uint32_t request = 0;
    uint32_t recall_started_ms = 0;
    bool preset_recall = false;
};

template<unsigned Capacity = 8>
class FxQueue {
public:
    bool push(const FxRequest& r) {
        const uint32_t w = write_.load(std::memory_order_relaxed);
        if (w - read_.load(std::memory_order_acquire) >= Capacity) return false;
        slots_[w % Capacity] = r;
        write_.store(w + 1, std::memory_order_release);
        return true;
    }
    bool pop(FxRequest& r) {
        const uint32_t r_idx = read_.load(std::memory_order_relaxed);
        if (r_idx == write_.load(std::memory_order_acquire)) return false;
        r = slots_[r_idx % Capacity];
        read_.store(r_idx + 1, std::memory_order_release);
        return true;
    }
    // SPSC discipline: ONLY the consumer (audio task) advances read_.
    // Producer invalidates obsolete requests via generation stamping;
    // applyFx() discards requests with mismatched generations without race.
    void drain() {
        FxRequest req;
        while (pop(req)) {}
    }
private:
    static_assert(std::atomic<uint32_t>::is_always_lock_free, "32-bit atomics required");
    FxRequest slots_[Capacity]{};
    std::atomic<uint32_t> read_{0}, write_{0};
};

using Mailbox = FxQueue<8>;

// Applies an FxConfig directly to AMY Bus 0.
// Hard real-time safe: Zero heap allocations, bounded execution time.
inline void applyBusFx(const FxConfig& cfg) {
    if (cfg.bypass_all) {
        config_chorus(0, 0.0f, CHORUS_DEFAULT_MAX_DELAY, CHORUS_DEFAULT_LFO_FREQ, CHORUS_DEFAULT_MOD_DEPTH);
        config_echo(0, 0.0f, 350.0f, 1000.0f, 0.0f, 0.0f);
        config_reverb(0, 0.0f, REVERB_DEFAULT_LIVENESS, REVERB_DEFAULT_DAMPING, REVERB_DEFAULT_XOVER_HZ);
        config_eq(0, F2S(1.0f), F2S(1.0f), F2S(1.0f));
        return;
    }

    // --- Chorus ---
    float chorus_level = 0.0f;
    float chorus_rate = 0.5f;
    uint16_t chorus_max_del = 320;
    float depth_val = std::clamp(cfg.chorus_depth, 0.0f, 1.0f);

    switch (static_cast<ChorusMode>(cfg.chorus_mode)) {
        case ChorusMode::Off:
            chorus_level = 0.0f;
            break;
        case ChorusMode::Classic:
            chorus_level = 0.50f;
            chorus_rate = 0.50f;
            chorus_max_del = 320;
            break;
        case ChorusMode::JunoI:
            chorus_level = 0.50f;
            chorus_rate = 0.40f;
            chorus_max_del = 320;
            depth_val *= 0.80f;
            break;
        case ChorusMode::JunoII:
            chorus_level = 0.50f;
            chorus_rate = 0.65f;
            chorus_max_del = 320;
            depth_val = std::min(1.0f, depth_val * 1.30f);
            break;
        case ChorusMode::Ensemble:
            chorus_level = 0.60f;
            chorus_rate = 1.00f;
            chorus_max_del = 400;
            depth_val = std::min(1.0f, depth_val * 1.50f);
            break;
        case ChorusMode::Vibrato:
            chorus_level = 1.00f; // 100% wet pitch modulation
            chorus_rate = 2.50f;
            chorus_max_del = 200;
            depth_val *= 0.70f;
            break;
        default:
            chorus_level = 0.0f;
            break;
    }
    config_chorus(0, chorus_level, chorus_max_del, chorus_rate, depth_val);

    // --- Echo / Delay ---
    float echo_mix = std::clamp(cfg.delay_mix, 0.0f, 1.0f);
    float echo_time = std::clamp(cfg.delay_time_ms, 10.0f, 1000.0f);
    float echo_fb = std::clamp(cfg.delay_feedback, 0.0f, 0.95f);
    config_echo(0, echo_mix, echo_time, 1000.0f, echo_fb, 0.0f);

    // --- Reverb ---
    float rev_mix = std::clamp(cfg.reverb_mix, 0.0f, 1.0f);
    float rev_size = std::clamp(cfg.reverb_size, 0.0f, 1.0f);
    float liveness = 0.40f + 0.55f * rev_size; // 0.40 .. 0.95
    config_reverb(0, rev_mix, liveness, 0.5f, 3000.0f);

    // --- 3-Band Parametric EQ & Tilt Tone ---
    float tone = std::clamp(cfg.master_tone, -1.0f, 1.0f);
    float eq_l = (tone >= 0.0f) ? (1.0f - 0.4f * tone) : (1.0f - 0.6f * tone);
    float eq_h = (tone >= 0.0f) ? (1.0f + 0.6f * tone) : (1.0f + 0.4f * tone);
    float eq_m = 1.0f;

    if (cfg.eq_low_db != 0.0f) eq_l *= std::pow(10.0f, cfg.eq_low_db / 20.0f);
    if (cfg.eq_mid_db != 0.0f) eq_m *= std::pow(10.0f, cfg.eq_mid_db / 20.0f);
    if (cfg.eq_high_db != 0.0f) eq_h *= std::pow(10.0f, cfg.eq_high_db / 20.0f);

    config_eq(0, F2S(eq_l), F2S(eq_m), F2S(eq_h));
}

// 32-Byte Persisted Record
#pragma pack(push, 1)
struct Record {
    uint32_t magic;          // 0x32315846 ("FX12")
    uint16_t version;        // 1
    uint16_t sequence;       // Sequence ID
    uint8_t chorus_mode;     // 0..5
    uint8_t chorus_depth;    // 0..100
    uint16_t delay_time_ms;  // 10..1000
    uint8_t delay_feedback;  // 0..95
    uint8_t delay_mix;       // 0..100
    uint8_t reverb_size;     // 0..100
    uint8_t reverb_mix;      // 0..100
    int8_t master_tone;      // -100..100
    int8_t eq_low_db;        // -12..12
    int8_t eq_mid_db;        // -12..12
    int8_t eq_high_db;       // -12..12
    uint8_t bypass_all;      // 0 or 1
    uint8_t reserved[3];     // 0
    uint32_t rom_compat;     // 0x3e76efb6
    uint32_t crc32;          // CRC of bytes 0..27
};
#pragma pack(pop)
static_assert(sizeof(Record) == kFxRecordBytes, "Record must be exactly 32 bytes");

inline uint32_t calcCrc(const uint8_t* data, unsigned n) {
    uint32_t c = ~0u;
    for (unsigned i = 0; i < n; ++i) {
        c ^= data[i];
        for (unsigned j = 0; j < 8; ++j) c = (c >> 1) ^ (0xedb88320u & uint32_t(-int32_t(c & 1)));
    }
    return ~c;
}

inline bool encodeRecord(const FxConfig& cfg, uint32_t sequence, uint8_t* out) {
    if (!valid(cfg)) return false;
    Record r{};
    r.magic = kFxMagic;
    r.version = static_cast<uint16_t>(kFxVersion);
    r.sequence = static_cast<uint16_t>(sequence);
    r.chorus_mode = cfg.chorus_mode;
    r.chorus_depth = static_cast<uint8_t>(std::lround(cfg.chorus_depth * 100.0f));
    r.delay_time_ms = static_cast<uint16_t>(std::lround(cfg.delay_time_ms));
    r.delay_feedback = static_cast<uint8_t>(std::lround(cfg.delay_feedback * 100.0f));
    r.delay_mix = static_cast<uint8_t>(std::lround(cfg.delay_mix * 100.0f));
    r.reverb_size = static_cast<uint8_t>(std::lround(cfg.reverb_size * 100.0f));
    r.reverb_mix = static_cast<uint8_t>(std::lround(cfg.reverb_mix * 100.0f));
    r.master_tone = static_cast<int8_t>(std::lround(cfg.master_tone * 100.0f));
    r.eq_low_db = static_cast<int8_t>(std::lround(cfg.eq_low_db));
    r.eq_mid_db = static_cast<int8_t>(std::lround(cfg.eq_mid_db));
    r.eq_high_db = static_cast<int8_t>(std::lround(cfg.eq_high_db));
    r.bypass_all = cfg.bypass_all ? 1 : 0;
    r.rom_compat = kRomCompatibility;
    r.crc32 = calcCrc(reinterpret_cast<const uint8_t*>(&r), 28);
    std::memcpy(out, &r, sizeof(r));
    return true;
}

inline bool decodeRecord(const uint8_t* in, unsigned len, FxConfig& cfg, uint32_t& sequence) {
    if (len != kFxRecordBytes || !in) return false;
    Record r{};
    std::memcpy(&r, in, sizeof(r));
    if (r.magic != kFxMagic || r.version != kFxVersion || r.rom_compat != kRomCompatibility) return false;
    if (r.crc32 != calcCrc(in, 28)) return false;

    FxConfig c{};
    c.chorus_mode = r.chorus_mode;
    c.chorus_depth = r.chorus_depth / 100.0f;
    c.delay_time_ms = static_cast<float>(r.delay_time_ms);
    c.delay_feedback = r.delay_feedback / 100.0f;
    c.delay_mix = r.delay_mix / 100.0f;
    c.reverb_size = r.reverb_size / 100.0f;
    c.reverb_mix = r.reverb_mix / 100.0f;
    c.master_tone = r.master_tone / 100.0f;
    c.eq_low_db = static_cast<float>(r.eq_low_db);
    c.eq_mid_db = static_cast<float>(r.eq_mid_db);
    c.eq_high_db = static_cast<float>(r.eq_high_db);
    c.bypass_all = (r.bypass_all != 0);

    if (!valid(c)) return false;
    cfg = c;
    sequence = r.sequence;
    return true;
}

class FxPresetStore {
public:
    virtual ~FxPresetStore() = default;
    virtual bool loadFx(unsigned slot, FxConfig& out) = 0;
    virtual bool saveFx(unsigned slot, const FxConfig& cfg) = 0;
};

} // namespace smk::p4::fx
