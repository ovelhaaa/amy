#pragma once
#include "fx_state.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <cstdio>
#include <cstring>

namespace smk::p4::fx {

inline bool seqAfter16(uint16_t a, uint16_t b) {
    return static_cast<int16_t>(a - b) > 0;
}

// Control task only. Sound FX preset persistence in namespace sound012fx.
// Uses dual-bank ping-pong with 32-byte records protected by IEEE 802.3 CRC-32.
class NvsFxPresets final : public FxPresetStore {
public:
    ~NvsFxPresets() override { close(); }
    NvsFxPresets() = default;
    NvsFxPresets(const NvsFxPresets&) = delete;
    NvsFxPresets& operator=(const NvsFxPresets&) = delete;

    bool begin() {
        close();
        error_ = nvs_flash_init();
        if (error_ != ESP_OK) return false;
        error_ = nvs_open("sound012fx", NVS_READWRITE, &handle_);
        if (error_ != ESP_OK) { close(); return false; }
        return true;
    }

    bool checkNvsBudget(size_t required_entries = 4) {
        nvs_stats_t stats{};
        error_ = nvs_get_stats(nullptr, &stats);
        if (error_ != ESP_OK) {
            return false;
        }
        constexpr size_t gc_entries = 126;
        if (stats.free_entries < gc_entries + required_entries) {
            error_ = ESP_ERR_NVS_NOT_ENOUGH_SPACE;
            return false;
        }
        return true;
    }

    bool loadFx(unsigned slot, FxConfig& out) override {
        if (!handle_ || slot >= 16) return false;
        Scan scan{};
        if (!scanSlot(slot, scan) || !scan.found) return false;
        out = scan.latest;
        return true;
    }

    bool saveFx(unsigned slot, const FxConfig& cfg) override {
        if (!handle_ || slot >= 16 || !valid(cfg)) return false;
        Scan scan{};
        if (!scanSlot(slot, scan)) return false;

        uint16_t next_seq = scan.found ? static_cast<uint16_t>(scan.sequence + 1) : 1;
        if (next_seq == 0) next_seq = 1;

        // Ping-pong bank selection: prefer invalid/missing bank, else older sequence
        unsigned target_bank = !scan.valid[0] ? 0 : (!scan.valid[1] ? 1 : (seqAfter16(scan.seq[1], scan.seq[0]) ? 0 : 1));

        return writeFxBank(slot, target_bank, cfg, next_seq);
    }

    bool writeFxBank(unsigned slot, unsigned bank, const FxConfig& cfg, uint16_t seq) {
        if (!handle_ || slot >= 16 || bank > 1 || !valid(cfg)) return false;
        if (!checkNvsBudget(4)) return false;

        uint8_t bytes[kFxRecordBytes];
        if (!encodeRecord(cfg, seq, bytes)) return false;

        char key[12];
        makeKey(slot, bank, key);
        error_ = nvs_set_blob(handle_, key, bytes, sizeof(bytes));
        if (error_ != ESP_OK) return false;

        error_ = nvs_commit(handle_);
        if (error_ != ESP_OK) return false;

        uint8_t check[kFxRecordBytes];
        size_t n = sizeof(check);
        error_ = nvs_get_blob(handle_, key, check, &n);
        if (error_ != ESP_OK || n != sizeof(check) || std::memcmp(check, bytes, sizeof(bytes)) != 0) {
            return false;
        }
        return true;
    }

    bool readFxBank(unsigned slot, unsigned bank, FxConfig& cfg, uint16_t& seq) {
        if (!handle_ || slot >= 16 || bank > 1) return false;
        char key[12];
        makeKey(slot, bank, key);
        uint8_t bytes[kFxRecordBytes];
        size_t n = sizeof(bytes);
        error_ = nvs_get_blob(handle_, key, bytes, &n);
        if (error_ != ESP_OK) return false;
        uint32_t s32 = 0;
        if (!decodeRecord(bytes, unsigned(n), cfg, s32)) return false;
        seq = static_cast<uint16_t>(s32);
        return true;
    }

    bool writeMarkerBank(unsigned slot, unsigned bank, const uint8_t* marker_bytes, size_t len) {
        if (!handle_ || slot >= 16 || bank > 1 || !marker_bytes || len != 32) return false;
        if (!checkNvsBudget(4)) return false;

        char key[12];
        makeMarkerKey(slot, bank, key);
        error_ = nvs_set_blob(handle_, key, marker_bytes, len);
        if (error_ != ESP_OK) return false;

        error_ = nvs_commit(handle_);
        if (error_ != ESP_OK) return false;

        uint8_t check[32];
        size_t n = sizeof(check);
        error_ = nvs_get_blob(handle_, key, check, &n);
        if (error_ != ESP_OK || n != len || std::memcmp(check, marker_bytes, len) != 0) {
            return false;
        }
        return true;
    }

    bool readMarkerBank(unsigned slot, unsigned bank, uint8_t* marker_bytes, size_t& len) {
        if (!handle_ || slot >= 16 || bank > 1 || !marker_bytes || len < 32) return false;
        char key[12];
        makeMarkerKey(slot, bank, key);
        size_t n = len;
        error_ = nvs_get_blob(handle_, key, marker_bytes, &n);
        if (error_ != ESP_OK) return false;
        len = n;
        return true;
    }

    esp_err_t error() const { return error_; }

private:
    struct Scan {
        FxConfig latest;
        uint16_t sequence = 0;
        uint16_t seq[2] = {0, 0};
        bool valid[2] = {false, false};
        bool found = false;
    };

    void close() {
        if (handle_) {
            nvs_close(handle_);
            handle_ = 0;
        }
    }

    static void makeKey(unsigned slot, unsigned bank, char* key) {
        std::snprintf(key, 12, "f%02u%c", slot, bank ? 'b' : 'a');
    }

    static void makeMarkerKey(unsigned slot, unsigned bank, char* key) {
        std::snprintf(key, 12, "c%02u%c", slot, bank ? 'b' : 'a');
    }

    bool scanSlot(unsigned slot, Scan& scan) {
        if (!handle_ || slot >= 16) return false;
        for (unsigned bank = 0; bank < 2; ++bank) {
            char key[12];
            makeKey(slot, bank, key);
            uint8_t bytes[kFxRecordBytes];
            size_t n = sizeof(bytes);
            esp_err_t err = nvs_get_blob(handle_, key, bytes, &n);
            if (err == ESP_ERR_NVS_NOT_FOUND) continue;
            if (err != ESP_OK) return false; // Hardware I/O failure
            FxConfig cfg{};
            uint32_t seq = 0;
            if (decodeRecord(bytes, unsigned(n), cfg, seq)) {
                scan.valid[bank] = true;
                scan.seq[bank] = static_cast<uint16_t>(seq);
                if (!scan.found || seqAfter16(static_cast<uint16_t>(seq), scan.sequence)) {
                    scan.latest = cfg;
                    scan.sequence = static_cast<uint16_t>(seq);
                    scan.found = true;
                }
            }
        }
        return true;
    }

    nvs_handle_t handle_ = 0;
    esp_err_t error_ = ESP_OK;
};

} // namespace smk::p4::fx
