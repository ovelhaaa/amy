#pragma once
#include "user_presets_v2.h"
#include "user_preset_fx_nvs.h"
#include <cstdio>
#include <cstring>
#include <cstdint>

namespace smk::p4::preset {

constexpr uint32_t kMarkerMagic = 0x3132434d; // "12CM"
constexpr uint16_t kMarkerVersion = 1;
constexpr unsigned kMarkerRecordBytes = 32;

#pragma pack(push, 1)
struct CommitMarker {
    uint32_t magic = kMarkerMagic;    // 0..3: 0x3132434d
    uint16_t version = kMarkerVersion;// 4..5: 1
    uint16_t slot = 0;               // 6..7: 0..15
    uint32_t commit_seq = 0;         // 8..11: logical generation
    uint32_t synth_seq = 0;          // 12..15: synth sequence
    uint8_t  synth_bank = 0;         // 16: 0 or 1
    uint8_t  synth_version = 2;      // 17: 1 or 2
    uint16_t patch_id = 0;           // 18..19: patch ROM ID
    uint16_t fx_seq = 0;             // 20..21: fx sequence
    uint8_t  fx_bank = 0;            // 22: 0 or 1
    uint8_t  reserved = 0;           // 23: 0
    uint32_t rom_compat = fx::kRomCompatibility; // 24..27: 0x3e76efb6
    uint32_t crc32 = 0;              // 28..31: CRC of bytes 0..27
};
#pragma pack(pop)
static_assert(sizeof(CommitMarker) == kMarkerRecordBytes, "CommitMarker must be 32 bytes");

inline bool validMarker(const CommitMarker& m) {
    if (m.magic != kMarkerMagic || m.version != kMarkerVersion) return false;
    if (m.slot >= 16) return false;
    if (m.synth_bank > 1 || (m.synth_version != 1 && m.synth_version != 2)) return false;
    if (m.fx_bank > 1) return false;
    if (m.reserved != 0) return false;
    if (m.rom_compat != fx::kRomCompatibility) return false;
    if (m.crc32 != fx::calcCrc(reinterpret_cast<const uint8_t*>(&m), 28)) return false;
    return true;
}

inline bool markerAfter(uint32_t a, uint32_t b) {
    return static_cast<int32_t>(a - b) > 0;
}

enum class FaultInjection : uint8_t {
    None = 0,
    BeforeFxWrite,
    DuringFxWrite,
    AfterFxWrite,
    BeforeSynthWrite,
    DuringSynthWrite,
    AfterSynthWrite,
    BeforeMarkerWrite,
    DuringMarkerWrite,
    AfterMarkerWrite,
    DuringReadback
};

struct SaveResult {
    enum class Status : uint8_t {
        Success,
        FailedBeforeCommit,
        CommitOutcomeUnknown,
        NvsFull,
        InvalidPreset,
        StorageUnavailable,
        // Backward-compatible aliases
        FxStorageUnavailable = StorageUnavailable,
        SynthStorageUnavailable = StorageUnavailable,
        FxWriteError = FailedBeforeCommit,
        SynthWriteError = FailedBeforeCommit,
        CommitMarkerError = FailedBeforeCommit,
        ReadbackError = CommitOutcomeUnknown,
        TransactionIncomplete = FailedBeforeCommit
    };
    Status status = Status::Success;
    const char* message = "SAVED";
    uint32_t expected_commit = 0;
    uint32_t expected_synth = 0;
};

class PresetTransactionCoordinator {
public:
    void setSynthStore(edit::VersionedPresetStore* store) { synth_store_ = store; }
    void setFxStore(fx::NvsFxPresets* store) { fx_store_ = store; }
    void setFaultInjection(FaultInjection f) { fault_ = f; }

    SaveResult save(unsigned slot, const edit::Preset& preset, const fx::FxConfig& fx) {
        if (slot >= 16) return { SaveResult::Status::InvalidPreset, "SAVE FAILED: INVALID SLOT" };
        if (!synth_store_) return { SaveResult::Status::StorageUnavailable, "SAVE FAILED: STORAGE UNAVAILABLE" };
        if (!fx_store_) return { SaveResult::Status::StorageUnavailable, "SAVE FAILED: FX STORAGE UNAVAILABLE" };
        if (!edit::validPreset(preset, false)) return { SaveResult::Status::InvalidPreset, "SAVE FAILED: INVALID SYNTH" };
        if (!fx::valid(fx)) return { SaveResult::Status::InvalidPreset, "SAVE FAILED: INVALID FX" };

        if (!fx_store_->checkNvsBudget(12)) {
            return { SaveResult::Status::NvsFull, "SAVE FAILED: NVS FULL" };
        }

        // Fault injection check 1
        if (fault_ == FaultInjection::BeforeFxWrite) {
            return { SaveResult::Status::FailedBeforeCommit, "SAVE FAILED: PRE-COMMIT" };
        }

        // 1. Identify active commit marker or fallback state
        CommitMarker cur_marker{};
        unsigned cur_marker_bank = 0;
        bool marker_found = false;
        scanMarkers(slot, cur_marker, cur_marker_bank, marker_found);

        if (!marker_found) {
            LegacySlotInspection insp{};
            if (!inspectLegacySlot(slot, insp)) {
                return { SaveResult::Status::StorageUnavailable, "SAVE FAILED: NVS READ ERROR" };
            }

            if (insp.synth_found) {
                // Establish baseline commit marker for legacy preset to guarantee rollback
                CommitMarker base_marker{};
                base_marker.magic = kMarkerMagic;
                base_marker.version = kMarkerVersion;
                base_marker.slot = static_cast<uint16_t>(slot);
                base_marker.commit_seq = 1;
                base_marker.synth_seq = insp.synth_seq;
                base_marker.synth_bank = static_cast<uint8_t>(insp.synth_bank);
                base_marker.synth_version = static_cast<uint8_t>(insp.synth_version);
                base_marker.patch_id = insp.patch_id;
                base_marker.fx_seq = insp.fx_found ? insp.fx_seq : 0;
                base_marker.fx_bank = insp.fx_found ? static_cast<uint8_t>(insp.fx_bank) : 0;
                base_marker.reserved = 0;
                base_marker.rom_compat = fx::kRomCompatibility;
                base_marker.crc32 = fx::calcCrc(reinterpret_cast<const uint8_t*>(&base_marker), 28);

                if (!fx_store_->writeMarkerBank(slot, 0, reinterpret_cast<const uint8_t*>(&base_marker), sizeof(base_marker))) {
                    return { SaveResult::Status::FailedBeforeCommit, "SAVE FAILED: PRE-COMMIT" };
                }

                cur_marker = base_marker;
                cur_marker_bank = 0;
                marker_found = true;
            }
        }

        // Ping-pong targets (always target the alternate / inactive bank)
        const unsigned target_marker_bank = marker_found ? (1 - cur_marker_bank) : 0;
        const unsigned target_synth_bank = marker_found ? (1 - cur_marker.synth_bank) : 0;
        const unsigned target_fx_bank = marker_found ? (1 - cur_marker.fx_bank) : 0;

        uint32_t next_commit_seq = marker_found ? (cur_marker.commit_seq + 1) : 1;
        if (next_commit_seq == 0) next_commit_seq = 1;

        uint32_t next_synth_seq = marker_found ? (cur_marker.synth_seq + 1) : 1;
        if (next_synth_seq == 0) next_synth_seq = 1;

        uint16_t next_fx_seq = marker_found ? static_cast<uint16_t>(cur_marker.fx_seq + 1) : 1;
        if (next_fx_seq == 0) next_fx_seq = 1;

        // 2. Write candidate FX to inactive FX bank
        if (fault_ == FaultInjection::DuringFxWrite) {
            // Write corrupt FX record
            fx::FxConfig corrupt_fx = fx;
            corrupt_fx.chorus_mode = 99; // invalid
            fx_store_->writeFxBank(slot, target_fx_bank, corrupt_fx, next_fx_seq);
            return { SaveResult::Status::FailedBeforeCommit, "SAVE FAILED: PRE-COMMIT" };
        }
        if (!fx_store_->writeFxBank(slot, target_fx_bank, fx, next_fx_seq)) {
            return { SaveResult::Status::FailedBeforeCommit, "SAVE FAILED: PRE-COMMIT" };
        }

        // Fault injection check 3
        if (fault_ == FaultInjection::AfterFxWrite || fault_ == FaultInjection::BeforeSynthWrite) {
            return { SaveResult::Status::FailedBeforeCommit, "SAVE FAILED: PRE-COMMIT" };
        }

        // 3. Write candidate Synth to inactive Synth bank
        edit::Preset p_to_save = preset;
        p_to_save.sequence = next_synth_seq;
        uint8_t synth_bytes[edit::kV2RecordBytes];
        if (!edit::encodeV2(p_to_save, synth_bytes)) {
            return { SaveResult::Status::InvalidPreset, "SAVE FAILED: SYNTH ENCODE ERROR" };
        }
        if (fault_ == FaultInjection::DuringSynthWrite) {
            synth_bytes[10] ^= 0xff; // corrupt
            synth_store_->writeV2(slot, target_synth_bank, synth_bytes, sizeof(synth_bytes));
            return { SaveResult::Status::FailedBeforeCommit, "SAVE FAILED: PRE-COMMIT" };
        }
        if (!synth_store_->writeV2(slot, target_synth_bank, synth_bytes, sizeof(synth_bytes))) {
            return { SaveResult::Status::FailedBeforeCommit, "SAVE FAILED: PRE-COMMIT" };
        }

        // Readback verify candidate synth
        uint8_t synth_check[edit::kV2RecordBytes];
        unsigned synth_check_len = sizeof(synth_check);
        if (synth_store_->readVersioned(2, slot, target_synth_bank, synth_check, synth_check_len) != edit::BankRead::Present ||
            synth_check_len != sizeof(synth_check) ||
            std::memcmp(synth_check, synth_bytes, sizeof(synth_check)) != 0) {
            return { SaveResult::Status::FailedBeforeCommit, "SAVE FAILED: PRE-COMMIT" };
        }

        // Fault injection check 6
        if (fault_ == FaultInjection::AfterSynthWrite || fault_ == FaultInjection::BeforeMarkerWrite) {
            return { SaveResult::Status::FailedBeforeCommit, "SAVE FAILED: PRE-COMMIT" };
        }

        // 4. Form and write Commit Marker to inactive Marker bank
        CommitMarker marker{};
        marker.magic = kMarkerMagic;
        marker.version = kMarkerVersion;
        marker.slot = static_cast<uint16_t>(slot);
        marker.commit_seq = next_commit_seq;
        marker.synth_seq = next_synth_seq;
        marker.synth_bank = static_cast<uint8_t>(target_synth_bank);
        marker.synth_version = 2;
        marker.patch_id = preset.patch;
        marker.fx_seq = next_fx_seq;
        marker.fx_bank = static_cast<uint8_t>(target_fx_bank);
        marker.reserved = 0;
        marker.rom_compat = fx::kRomCompatibility;
        marker.crc32 = fx::calcCrc(reinterpret_cast<const uint8_t*>(&marker), 28);

        if (fault_ == FaultInjection::DuringMarkerWrite) {
            marker.crc32 ^= 0xff; // Corrupt marker CRC
            fx_store_->writeMarkerBank(slot, target_marker_bank, reinterpret_cast<const uint8_t*>(&marker), sizeof(marker));
            CommitMarker verify_m{};
            unsigned verify_bank = 0;
            bool verify_found = false;
            scanMarkers(slot, verify_m, verify_bank, verify_found);
            if (verify_found && verify_m.commit_seq == next_commit_seq && verify_bank == target_marker_bank) {
                edit::Preset check_p{};
                fx::FxConfig check_fx{};
                if (load(slot, check_p, check_fx) && check_p.sequence == next_synth_seq) {
                    return { SaveResult::Status::Success, "SAVED" };
                }
                return { SaveResult::Status::CommitOutcomeUnknown, "SAVE INDETERMINATE: VERIFY PRESET", next_commit_seq, next_synth_seq };
            }
            return { SaveResult::Status::FailedBeforeCommit, "SAVE FAILED: PRE-COMMIT" };
        }
        if (!fx_store_->writeMarkerBank(slot, target_marker_bank, reinterpret_cast<const uint8_t*>(&marker), sizeof(marker))) {
            CommitMarker verify_m{};
            unsigned verify_bank = 0;
            bool verify_found = false;
            scanMarkers(slot, verify_m, verify_bank, verify_found);
            if (verify_found && verify_m.commit_seq == next_commit_seq && verify_bank == target_marker_bank) {
                edit::Preset check_p{};
                fx::FxConfig check_fx{};
                if (load(slot, check_p, check_fx) && check_p.sequence == next_synth_seq) {
                    return { SaveResult::Status::Success, "SAVED" };
                }
                return { SaveResult::Status::CommitOutcomeUnknown, "SAVE INDETERMINATE: VERIFY PRESET", next_commit_seq, next_synth_seq };
            }
            return { SaveResult::Status::CommitOutcomeUnknown, "SAVE INDETERMINATE: VERIFY PRESET", next_commit_seq, next_synth_seq };
        }

        // Fault injection check: AfterMarkerWrite (marker committed!)
        if (fault_ == FaultInjection::AfterMarkerWrite) {
            edit::Preset check_preset{};
            fx::FxConfig check_fx{};
            if (load(slot, check_preset, check_fx) && check_preset.sequence == next_synth_seq) {
                return { SaveResult::Status::Success, "SAVED" };
            }
            return { SaveResult::Status::CommitOutcomeUnknown, "SAVE INDETERMINATE: VERIFY PRESET", next_commit_seq, next_synth_seq };
        }

        // 5. Final Transaction Readback Verification
        if (fault_ == FaultInjection::DuringReadback) {
            return { SaveResult::Status::CommitOutcomeUnknown, "SAVE INDETERMINATE: VERIFY PRESET", next_commit_seq, next_synth_seq };
        }

        edit::Preset check_preset{};
        fx::FxConfig check_fx{};
        if (!load(slot, check_preset, check_fx) || check_preset.sequence != next_synth_seq) {
            return { SaveResult::Status::CommitOutcomeUnknown, "SAVE INDETERMINATE: VERIFY PRESET", next_commit_seq, next_synth_seq };
        }

        return { SaveResult::Status::Success, "SAVED" };
    }

    bool load(unsigned slot, edit::Preset& preset, fx::FxConfig& fx, CommitMarker* verified_marker=nullptr) {
        if (slot >= 16 || !synth_store_) return false;

        // 1. Try Commit Markers first
        CommitMarker markers[2]{};
        bool marker_valid[2] = {false, false};
        for (unsigned b = 0; b < 2; ++b) {
            if (fx_store_) {
                uint8_t m_bytes[kMarkerRecordBytes];
                size_t len = sizeof(m_bytes);
                if (fx_store_->readMarkerBank(slot, b, m_bytes, len) && len == sizeof(m_bytes)) {
                    CommitMarker m{};
                    std::memcpy(&m, m_bytes, sizeof(m));
                    if (validMarker(m) && m.slot == slot) {
                        markers[b] = m;
                        marker_valid[b] = true;
                    }
                }
            }
        }

        if (marker_valid[0] || marker_valid[1]) {
            // Pick newest marker first
            unsigned preferred = !marker_valid[0] ? 1 : (!marker_valid[1] ? 0 :
                (markerAfter(markers[0].commit_seq, markers[1].commit_seq) ? 0 : 1));
            unsigned order[2] = { preferred, 1 - preferred };

            for (unsigned idx = 0; idx < 2; ++idx) {
                unsigned b = order[idx];
                if (!marker_valid[b]) continue;
                const auto& m = markers[b];

                // Verify Synth Record
                uint8_t synth_bytes[edit::kV2RecordBytes];
                unsigned len = sizeof(synth_bytes);
                const auto res = synth_store_->readVersioned(m.synth_version, slot, m.synth_bank, synth_bytes, len);
                if (res != edit::BankRead::Present) continue;

                edit::Preset p{};
                const bool synth_ok = (m.synth_version == 1) ?
                    edit::decode(synth_bytes, len, p) :
                    edit::decodeV2(synth_bytes, len, p);
                if (!synth_ok || p.sequence != m.synth_seq || p.patch != m.patch_id) continue;

                // Verify FX Record
                fx::FxConfig fx_cfg{};
                uint16_t fx_seq = 0;
                if (m.fx_seq == 0) {
                    fx_cfg = fx::FxConfig{};
                } else {
                    if (!fx_store_ || !fx_store_->readFxBank(slot, m.fx_bank, fx_cfg, fx_seq) || fx_seq != m.fx_seq) {
                        continue;
                    }
                }

                // Both verified and paired!
                if(verified_marker) *verified_marker=m;
                preset = p;
                fx = fx_cfg;
                return true;
            }
            // If commit markers existed but pointed records are corrupt, fail closed.
            return false;
        }

        if(verified_marker) return false;

        // 2. Legacy Fallback (No Commit Marker found)
        if (!synth_store_->load(slot, preset)) return false;

        // Try to load FX from sound012fx if present
        if (fx_store_ && fx_store_->loadFx(slot, fx)) {
            return true;
        }

        // Synthesis-only preset prior to 0.12 -> default to neutral dry FX
        fx = fx::FxConfig{};
        return true;
    }

private:
    struct LegacySlotInspection {
        bool synth_found = false;
        unsigned synth_version = 0; // 1 or 2
        unsigned synth_bank = 0;    // 0 or 1
        uint32_t synth_seq = 0;
        uint16_t patch_id = 0;
        edit::Preset synth_preset{};

        bool fx_found = false;
        unsigned fx_bank = 0;       // 0 or 1
        uint16_t fx_seq = 0;
        fx::FxConfig fx_config{};
    };

    bool inspectLegacySlot(unsigned slot, LegacySlotInspection& insp) {
        insp = LegacySlotInspection{};
        if (!synth_store_ || slot >= 16) return false;

        // 1. Scan Synth V1 and V2 in accordance with VersionedPresetStore rules:
        // scan version 1 (banks 0, 1), then version 2 (banks 0, 1).
        // If p.sequence > insp.synth_seq, update.
        // If sequences are equal, V1 bank 0 is retained unless a strictly newer sequence appears.
        for (unsigned version = 1; version <= 2; ++version) {
            for (unsigned bank = 0; bank < 2; ++bank) {
                uint8_t bytes[edit::kV2RecordBytes];
                unsigned len = sizeof(bytes);
                auto res = synth_store_->readVersioned(version, slot, bank, bytes, len);
                if (res == edit::BankRead::Error) return false;
                if (res == edit::BankRead::Missing) continue;
                if (len >= 8 && edit::get32(bytes) == 0x31314553 && edit::get32(bytes + 4) > 2) {
                    return false; // Future incompatible version
                }
                edit::Preset p{};
                bool valid = (version == 1) ? edit::decode(bytes, len, p) : edit::decodeV2(bytes, len, p);
                if (!valid) continue;

                if (!insp.synth_found || p.sequence > insp.synth_seq) {
                    insp.synth_found = true;
                    insp.synth_version = version;
                    insp.synth_bank = bank;
                    insp.synth_seq = p.sequence;
                    insp.patch_id = p.patch;
                    insp.synth_preset = p;
                }
            }
        }

        // 2. Scan FX (sound012fx)
        if (fx_store_) {
            for (unsigned bank = 0; bank < 2; ++bank) {
                fx::FxConfig cfg{};
                uint16_t seq = 0;
                if (fx_store_->readFxBank(slot, bank, cfg, seq)) {
                    if (!insp.fx_found || fx::seqAfter16(seq, insp.fx_seq)) {
                        insp.fx_found = true;
                        insp.fx_bank = bank;
                        insp.fx_seq = seq;
                        insp.fx_config = cfg;
                    }
                }
            }
        }
        return true;
    }

    void scanMarkers(unsigned slot, CommitMarker& latest, unsigned& active_bank, bool& found) {
        found = false;
        if (!fx_store_) return;
        CommitMarker markers[2]{};
        bool valid[2] = {false, false};
        for (unsigned b = 0; b < 2; ++b) {
            uint8_t bytes[kMarkerRecordBytes];
            size_t len = sizeof(bytes);
            if (fx_store_->readMarkerBank(slot, b, bytes, len) && len == sizeof(bytes)) {
                CommitMarker m{};
                std::memcpy(&m, bytes, sizeof(m));
                if (validMarker(m) && m.slot == slot) {
                    markers[b] = m;
                    valid[b] = true;
                }
            }
        }
        if (valid[0] || valid[1]) {
            active_bank = !valid[0] ? 1 : (!valid[1] ? 0 :
                (markerAfter(markers[0].commit_seq, markers[1].commit_seq) ? 0 : 1));
            latest = markers[active_bank];
            found = true;
        }
    }

    edit::VersionedPresetStore* synth_store_ = nullptr;
    fx::NvsFxPresets* fx_store_ = nullptr;
    FaultInjection fault_ = FaultInjection::None;
};

} // namespace smk::p4::preset
