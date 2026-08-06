#pragma once
#include <cstdint>
#include <cstddef>
#include <array>
#include "patch_types.h"

namespace smk {

class AmyAdapter;
class UIManager;

enum class PadBank : uint8_t {
    BankA_Drums       = 0, // Standard 808/909 drum kit mapping
    BankB_Melodic     = 1, // Chromatic / Pentatonic scale notes
    BankC_Chords      = 2, // Multi-note chord triggers (Maj, Min, 7th, etc.)
    BankD_Performance = 3  // Performance FX (Stutter, Filter Sweep, Reverb Wash, Roll)
};

struct PadConfig {
    char        name[16];
    uint8_t     note_count;   // 1 for single note, >1 for chords
    uint8_t     notes[4];     // MIDI notes to trigger
    bool        is_fx;        // True if pad triggers a performance effect
    uint8_t     fx_type;      // 0=Stutter 16th, 1=Stutter 32nd, 2=LPF Sweep, 3=HPF Sweep, 4=Pitch Drop, 5=Reverb Wash
};

class PadManager {
public:
    static constexpr size_t kPadsPerBank = 8;

    PadManager();

    PadBank activeBank() const { return active_bank_; }
    void setBank(PadBank bank);
    void nextBank();

    /**
     * @brief Trigger pad press (NoteOn or Performance FX)
     */
    void handlePadPress(uint8_t pad_idx, uint8_t velocity, AmyAdapter* engine, UIManager* ui);

    /**
     * @brief Trigger pad release (NoteOff or end Performance FX)
     */
    void handlePadRelease(uint8_t pad_idx, AmyAdapter* engine);

private:
    void initDefaultBanks();

    PadBank active_bank_ = PadBank::BankA_Drums;
    std::array<std::array<PadConfig, kPadsPerBank>, 4> bank_configs_;
};

} // namespace smk
