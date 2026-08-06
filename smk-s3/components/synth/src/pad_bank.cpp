#include "pad_bank.h"
#include "amy_adapter.h"
#include "ui_manager.h"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "PadManager";

namespace smk {

PadManager::PadManager() {
    initDefaultBanks();
}

void PadManager::initDefaultBanks() {
    // ─── Bank A: Drums / Percussion ──────────────────
    PadConfig bankA[8] = {
        {"KICK",     1, {36, 0, 0, 0}, false, 0}, // C1
        {"RIMS",     1, {37, 0, 0, 0}, false, 0}, // C#1
        {"SNARE",    1, {38, 0, 0, 0}, false, 0}, // D1
        {"CLAP",     1, {39, 0, 0, 0}, false, 0}, // D#1
        {"E-SNARE",  1, {40, 0, 0, 0}, false, 0}, // E1
        {"LOW-TOM",  1, {41, 0, 0, 0}, false, 0}, // F1
        {"CLOSED-HH",1, {42, 0, 0, 0}, false, 0}, // F#1
        {"HIGH-TOM", 1, {43, 0, 0, 0}, false, 0}  // G1
    };
    for (size_t i = 0; i < 8; ++i) bank_configs_[0][i] = bankA[i];

    // ─── Bank B: Melodic C4..C5 Pentatonic ────────────
    PadConfig bankB[8] = {
        {"C4", 1, {60, 0, 0, 0}, false, 0},
        {"D4", 1, {62, 0, 0, 0}, false, 0},
        {"E4", 1, {64, 0, 0, 0}, false, 0},
        {"G4", 1, {67, 0, 0, 0}, false, 0},
        {"A4", 1, {69, 0, 0, 0}, false, 0},
        {"C5", 1, {72, 0, 0, 0}, false, 0},
        {"D5", 1, {74, 0, 0, 0}, false, 0},
        {"E5", 1, {76, 0, 0, 0}, false, 0}
    };
    for (size_t i = 0; i < 8; ++i) bank_configs_[1][i] = bankB[i];

    // ─── Bank C: Chords ───────────────────────────────
    PadConfig bankC[8] = {
        {"C MAJ",  3, {60, 64, 67, 0}, false, 0}, // C4 - E4 - G4
        {"A MIN",  3, {57, 60, 64, 0}, false, 0}, // A3 - C4 - E4
        {"F MAJ7", 4, {53, 57, 60, 64}, false, 0},// F3 - A3 - C4 - E4
        {"G 7TH",  4, {55, 59, 62, 65}, false, 0},// G3 - B3 - D4 - F4
        {"D MIN",  3, {62, 65, 69, 0}, false, 0}, // D4 - F4 - A4
        {"E MIN7", 4, {52, 55, 59, 62}, false, 0},// E3 - G3 - B3 - D4
        {"C SUS4", 3, {60, 65, 67, 0}, false, 0}, // C4 - F4 - G4
        {"B DIM",  3, {59, 62, 65, 0}, false, 0}  // B3 - D4 - F4
    };
    for (size_t i = 0; i < 8; ++i) bank_configs_[2][i] = bankC[i];

    // ─── Bank D: Performance FX ───────────────────────
    PadConfig bankD[8] = {
        {"STUTTER 16", 0, {0, 0, 0, 0}, true, 0},
        {"STUTTER 32", 0, {0, 0, 0, 0}, true, 1},
        {"LPF SWEEP",  0, {0, 0, 0, 0}, true, 2},
        {"HPF SWEEP",  0, {0, 0, 0, 0}, true, 3},
        {"PITCH DROP", 0, {0, 0, 0, 0}, true, 4},
        {"REVERB WASH",0, {0, 0, 0, 0}, true, 5},
        {"DELAY ROLL", 0, {0, 0, 0, 0}, true, 6},
        {"MUTE CUT",   0, {0, 0, 0, 0}, true, 7}
    };
    for (size_t i = 0; i < 8; ++i) bank_configs_[3][i] = bankD[i];
}

void PadManager::nextBank() {
    uint8_t b = static_cast<uint8_t>(active_bank_);
    setBank(static_cast<PadBank>((b + 1) % 4));
}

void PadManager::setBank(PadBank bank) {
    active_bank_ = bank;
    const char* bname = "BANK A: DRUMS";
    switch (active_bank_) {
        case PadBank::BankA_Drums:       bname = "BANK A: DRUMS"; break;
        case PadBank::BankB_Melodic:     bname = "BANK B: MELODIC"; break;
        case PadBank::BankC_Chords:      bname = "BANK C: CHORDS"; break;
        case PadBank::BankD_Performance: bname = "BANK D: PERF FX"; break;
    }
    ESP_LOGI(TAG, "Switched Pad Bank to %s", bname);
}

void PadManager::handlePadPress(uint8_t pad_idx, uint8_t velocity, AmyAdapter* engine, UIManager* ui) {
    if (pad_idx >= 8) return;
    const auto& config = bank_configs_[static_cast<uint8_t>(active_bank_)][pad_idx];

    if (config.is_fx) {
        ESP_LOGI(TAG, "Triggering Performance FX: %s", config.name);
        if (ui) {
            ui->triggerParameterOverlay(config.name, "PERF FX", (float)velocity, 64.0f, "", TakeoverStatus::Captured);
        }
        return;
    }

    // Single note or multi-note chord trigger
    for (uint8_t i = 0; i < config.note_count; ++i) {
        if (engine) {
            engine->noteOn(9, config.notes[i], velocity); // Channel 10 / 9 for drums/pads
        }
    }

    if (ui) {
        ui->triggerParameterOverlay(config.name, "PAD TRIGGER", (float)velocity, 64.0f, "", TakeoverStatus::Captured);
    }
}

void PadManager::handlePadRelease(uint8_t pad_idx, AmyAdapter* engine) {
    if (pad_idx >= 8) return;
    const auto& config = bank_configs_[static_cast<uint8_t>(active_bank_)][pad_idx];

    if (!config.is_fx) {
        for (uint8_t i = 0; i < config.note_count; ++i) {
            if (engine) {
                engine->noteOff(9, config.notes[i]);
            }
        }
    }
}

} // namespace smk
