#include "user_preset_fx_nvs.h"
#include "fake_nvs.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <limits>

using namespace smk::p4::fx;

static void test_record_alignment_and_size() {
    static_assert(sizeof(Record) == 32, "Record size must be exactly 32 bytes");
    assert(sizeof(Record) == 32);
    assert(kFxRecordBytes == 32);

    // Verify critical offsets in the packed record
    Record r{};
    assert(reinterpret_cast<uintptr_t>(&r.magic) - reinterpret_cast<uintptr_t>(&r) == 0);
    assert(reinterpret_cast<uintptr_t>(&r.version) - reinterpret_cast<uintptr_t>(&r) == 4);
    assert(reinterpret_cast<uintptr_t>(&r.sequence) - reinterpret_cast<uintptr_t>(&r) == 6);
    assert(reinterpret_cast<uintptr_t>(&r.chorus_mode) - reinterpret_cast<uintptr_t>(&r) == 8);
    assert(reinterpret_cast<uintptr_t>(&r.chorus_depth) - reinterpret_cast<uintptr_t>(&r) == 9);
    assert(reinterpret_cast<uintptr_t>(&r.delay_time_ms) - reinterpret_cast<uintptr_t>(&r) == 10);
    assert(reinterpret_cast<uintptr_t>(&r.delay_feedback) - reinterpret_cast<uintptr_t>(&r) == 12);
    assert(reinterpret_cast<uintptr_t>(&r.delay_mix) - reinterpret_cast<uintptr_t>(&r) == 13);
    assert(reinterpret_cast<uintptr_t>(&r.reverb_size) - reinterpret_cast<uintptr_t>(&r) == 14);
    assert(reinterpret_cast<uintptr_t>(&r.reverb_mix) - reinterpret_cast<uintptr_t>(&r) == 15);
    assert(reinterpret_cast<uintptr_t>(&r.master_tone) - reinterpret_cast<uintptr_t>(&r) == 16);
    assert(reinterpret_cast<uintptr_t>(&r.eq_low_db) - reinterpret_cast<uintptr_t>(&r) == 17);
    assert(reinterpret_cast<uintptr_t>(&r.eq_mid_db) - reinterpret_cast<uintptr_t>(&r) == 18);
    assert(reinterpret_cast<uintptr_t>(&r.eq_high_db) - reinterpret_cast<uintptr_t>(&r) == 19);
    assert(reinterpret_cast<uintptr_t>(&r.bypass_all) - reinterpret_cast<uintptr_t>(&r) == 20);
    assert(reinterpret_cast<uintptr_t>(&r.rom_compat) - reinterpret_cast<uintptr_t>(&r) == 24);
    assert(reinterpret_cast<uintptr_t>(&r.crc32) - reinterpret_cast<uintptr_t>(&r) == 28);

    std::puts("  PASS: record alignment and layout (32 bytes packed)");
}

static void test_boundary_values_and_quantization() {
    FxConfig cfg{};
    cfg.chorus_mode = static_cast<uint8_t>(ChorusMode::Vibrato); // 5
    cfg.chorus_depth = 1.0f;
    cfg.delay_time_ms = 1000.0f;
    cfg.delay_feedback = 0.95f;
    cfg.delay_mix = 1.0f;
    cfg.reverb_size = 1.0f;
    cfg.reverb_mix = 1.0f;
    cfg.master_tone = 1.0f;
    cfg.eq_low_db = 12.0f;
    cfg.eq_mid_db = -12.0f;
    cfg.eq_high_db = 12.0f;
    cfg.bypass_all = true;

    assert(valid(cfg));

    uint8_t buf[kFxRecordBytes]{};
    assert(encodeRecord(cfg, 42, buf));

    FxConfig decoded{};
    uint32_t seq = 0;
    assert(decodeRecord(buf, sizeof(buf), decoded, seq));

    assert(seq == 42);
    assert(decoded.chorus_mode == static_cast<uint8_t>(ChorusMode::Vibrato));
    assert(std::fabs(decoded.chorus_depth - 1.0f) < 0.015f);
    assert(std::fabs(decoded.delay_time_ms - 1000.0f) < 0.5f);
    assert(std::fabs(decoded.delay_feedback - 0.95f) < 0.015f);
    assert(std::fabs(decoded.delay_mix - 1.0f) < 0.015f);
    assert(std::fabs(decoded.reverb_size - 1.0f) < 0.015f);
    assert(std::fabs(decoded.reverb_mix - 1.0f) < 0.015f);
    assert(std::fabs(decoded.master_tone - 1.0f) < 0.015f);
    assert(std::fabs(decoded.eq_low_db - 12.0f) < 0.5f);
    assert(std::fabs(decoded.eq_mid_db - (-12.0f)) < 0.5f);
    assert(std::fabs(decoded.eq_high_db - 12.0f) < 0.5f);
    assert(decoded.bypass_all == true);

    // Test minimum boundaries
    cfg.chorus_mode = 0;
    cfg.chorus_depth = 0.0f;
    cfg.delay_time_ms = 10.0f;
    cfg.delay_feedback = 0.0f;
    cfg.delay_mix = 0.0f;
    cfg.reverb_size = 0.0f;
    cfg.reverb_mix = 0.0f;
    cfg.master_tone = -1.0f;
    cfg.eq_low_db = -12.0f;
    cfg.eq_mid_db = 0.0f;
    cfg.eq_high_db = -12.0f;
    cfg.bypass_all = false;

    assert(valid(cfg));
    assert(encodeRecord(cfg, 100, buf));
    assert(decodeRecord(buf, sizeof(buf), decoded, seq));

    assert(seq == 100);
    assert(decoded.chorus_mode == 0);
    assert(decoded.chorus_depth == 0.0f);
    assert(decoded.delay_time_ms == 10.0f);
    assert(decoded.delay_feedback == 0.0f);
    assert(decoded.delay_mix == 0.0f);
    assert(decoded.reverb_size == 0.0f);
    assert(decoded.reverb_mix == 0.0f);
    assert(std::fabs(decoded.master_tone - (-1.0f)) < 0.015f);
    assert(decoded.eq_low_db == -12.0f);
    assert(decoded.eq_mid_db == 0.0f);
    assert(decoded.eq_high_db == -12.0f);
    assert(decoded.bypass_all == false);

    std::puts("  PASS: boundary values and quantization precision");
}

static void test_invalid_parameters_rejection() {
    FxConfig cfg{};
    uint8_t buf[kFxRecordBytes]{};

    // NaN / Inf tests
    cfg.chorus_depth = std::numeric_limits<float>::quiet_NaN();
    assert(!valid(cfg));
    assert(!encodeRecord(cfg, 1, buf));

    cfg.chorus_depth = 0.5f;
    cfg.delay_time_ms = std::numeric_limits<float>::infinity();
    assert(!valid(cfg));
    assert(!encodeRecord(cfg, 1, buf));

    // Out of range parameters
    cfg.delay_time_ms = 9.9f; // Below 10ms minimum
    assert(!valid(cfg));

    cfg.delay_time_ms = 1001.0f; // Above 1000ms maximum
    assert(!valid(cfg));

    cfg.delay_time_ms = 500.0f;
    cfg.delay_feedback = 0.96f; // Max is 0.95
    assert(!valid(cfg));

    cfg.delay_feedback = 0.5f;
    cfg.master_tone = 1.05f; // Range is -1..+1
    assert(!valid(cfg));

    cfg.master_tone = 0.0f;
    cfg.chorus_mode = 6; // Valid is 0..5 (ChorusMode::Count is 6)
    assert(!valid(cfg));

    std::puts("  PASS: invalid and NaN parameter rejection");
}

static void test_bit_corruption_resilience() {
    FxConfig cfg{};
    uint8_t original[kFxRecordBytes]{};
    assert(encodeRecord(cfg, 1, original));

    FxConfig decoded{};
    uint32_t seq = 0;

    // Flip every single bit across all 32 bytes (256 individual single-bit corruptions)
    for (size_t byte_idx = 0; byte_idx < kFxRecordBytes; ++byte_idx) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            uint8_t corrupted[kFxRecordBytes];
            std::memcpy(corrupted, original, sizeof(corrupted));
            corrupted[byte_idx] ^= (1u << bit);

            bool ok = decodeRecord(corrupted, sizeof(corrupted), decoded, seq);
            assert(!ok && "Single-bit corruption MUST be detected and rejected");
        }
    }

    std::puts("  PASS: 100% detection of 256 single-bit corruptions across entire record");
}

static void test_future_version_and_rom_rejection() {
    FxConfig cfg{};
    uint8_t buf[kFxRecordBytes]{};
    assert(encodeRecord(cfg, 1, buf));

    FxConfig decoded{};
    uint32_t seq = 0;

    // Test future version 2
    uint8_t future_ver[kFxRecordBytes];
    std::memcpy(future_ver, buf, sizeof(future_ver));
    auto* r_ver = reinterpret_cast<Record*>(future_ver);
    r_ver->version = 2;
    r_ver->crc32 = calcCrc(future_ver, 28); // Valid CRC for version 2
    assert(!decodeRecord(future_ver, sizeof(future_ver), decoded, seq));

    // Test wrong ROM compat
    uint8_t wrong_rom[kFxRecordBytes];
    std::memcpy(wrong_rom, buf, sizeof(wrong_rom));
    auto* r_rom = reinterpret_cast<Record*>(wrong_rom);
    r_rom->rom_compat = 0xdeadbeef;
    r_rom->crc32 = calcCrc(wrong_rom, 28);
    assert(!decodeRecord(wrong_rom, sizeof(wrong_rom), decoded, seq));

    // Test truncated length (< 32)
    assert(!decodeRecord(buf, 31, decoded, seq));
    // Test oversized length (> 32)
    assert(!decodeRecord(buf, 33, decoded, seq));

    std::puts("  PASS: future version, incompatible ROM, and invalid lengths safely rejected");
}

static void test_sequence_rollover_and_bank_pingpong() {
    fake_nvs::blobs.clear();
    fake_nvs::free_entries = 700;
    NvsFxPresets store;
    assert(store.begin());

    FxConfig cfg{};
    cfg.chorus_mode = 1;
    cfg.delay_feedback = 0.30f;

    // Directly write Bank A with sequence 65535 (0xFFFF)
    assert(store.writeFxBank(0, 0, cfg, 65535));

    // Verify it loads Bank A sequence 65535
    FxConfig loaded{};
    assert(store.loadFx(0, loaded));
    assert(loaded.chorus_mode == 1);

    // Save next revision: should ping-pong to Bank B with sequence 1 (skipping 0)
    cfg.chorus_mode = 2;
    assert(store.saveFx(0, cfg));

    // Verify Bank B sequence is 1 and Bank B is selected as latest
    uint16_t seq_b = 0;
    FxConfig cfg_b{};
    assert(store.readFxBank(0, 1, cfg_b, seq_b));
    assert(seq_b == 1);
    assert(cfg_b.chorus_mode == 2);

    // Load must return Bank B (sequence 1 after 65535)
    assert(store.loadFx(0, loaded));
    assert(loaded.chorus_mode == 2);

    // Next save: should ping-pong back to Bank A with sequence 2
    cfg.chorus_mode = 3;
    assert(store.saveFx(0, cfg));

    uint16_t seq_a = 0;
    FxConfig cfg_a{};
    assert(store.readFxBank(0, 0, cfg_a, seq_a));
    assert(seq_a == 2);
    assert(cfg_a.chorus_mode == 3);

    assert(store.loadFx(0, loaded));
    assert(loaded.chorus_mode == 3);

    std::puts("  PASS: 16-bit sequence rollover (65535 -> 1) and dual-bank ping-pong");
}

static void test_out_of_space_budget_check() {
    fake_nvs::blobs.clear();
    fake_nvs::free_entries = 700;
    NvsFxPresets store;
    assert(store.begin());

    // Check budget with standard entries -> true
    assert(store.checkNvsBudget(4));

    // Set mock free entries to low level (below gc_entries 126 + required 4 = 130)
    fake_nvs::free_entries = 100; // 100 free entries < 130
    assert(!store.checkNvsBudget(4));
    assert(store.error() == ESP_ERR_NVS_NOT_ENOUGH_SPACE);

    // Attempting saveFx must fail cleanly without corrupting state
    FxConfig cfg{};
    assert(!store.saveFx(0, cfg));
    assert(store.error() == ESP_ERR_NVS_NOT_ENOUGH_SPACE);

    // Restore free entries
    fake_nvs::free_entries = 700;
    assert(store.checkNvsBudget(4));
    assert(store.saveFx(0, cfg));

    std::puts("  PASS: out-of-space detection and budget safety");
}

int main() {
    std::puts("=== GATE H: FX Codec & Bank Recovery Hardening Tests ===");
    test_record_alignment_and_size();
    test_boundary_values_and_quantization();
    test_invalid_parameters_rejection();
    test_bit_corruption_resilience();
    test_future_version_and_rom_rejection();
    test_sequence_rollover_and_bank_pingpong();
    test_out_of_space_budget_check();
    std::puts("ALL GATE H TESTS PASSED.");
    return 0;
}
