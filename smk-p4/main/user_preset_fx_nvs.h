#pragma once
#include "fx_state.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <cstdio>
#include <cstring>

namespace smk::p4::fx {

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

        uint32_t next_seq = scan.found ? (scan.sequence + 1) : 1;
        // Ping-pong bank selection:
        // Prefer invalid/missing bank, else older sequence
        unsigned target_bank = !scan.valid[0] ? 0 : (!scan.valid[1] ? 1 : (scan.seq[0] <= scan.seq[1] ? 0 : 1));

        // Check NVS entries budget: 32 bytes blob = 2 entries
        nvs_stats_t stats{};
        error_ = nvs_get_stats(nullptr, &stats);
        if (error_ == ESP_OK) {
            constexpr size_t gc_entries = 126;
            if (stats.free_entries < gc_entries + 4) {
                error_ = ESP_ERR_NVS_NOT_ENOUGH_SPACE;
                return false;
            }
        }

        uint8_t bytes[kFxRecordBytes];
        if (!encodeRecord(cfg, next_seq, bytes)) return false;

        char key[12];
        makeKey(slot, target_bank, key);
        error_ = nvs_set_blob(handle_, key, bytes, sizeof(bytes));
        if (error_ != ESP_OK) return false;

        error_ = nvs_commit(handle_);
        if (error_ != ESP_OK) return false;

        // Verify readback byte-for-byte
        uint8_t check[kFxRecordBytes];
        size_t n = sizeof(check);
        error_ = nvs_get_blob(handle_, key, check, &n);
        if (error_ != ESP_OK || n != sizeof(check) || std::memcmp(check, bytes, sizeof(bytes)) != 0) {
            return false;
        }

        return true;
    }

    esp_err_t error() const { return error_; }

private:
    struct Scan {
        FxConfig latest;
        uint32_t sequence = 0;
        uint32_t seq[2] = {0, 0};
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
                scan.seq[bank] = seq;
                if (!scan.found || seq > scan.sequence) {
                    scan.latest = cfg;
                    scan.sequence = seq;
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
