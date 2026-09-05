#pragma once

#include "screen_base.h"
#include <cstdint>

namespace smk {

enum class PadBankMode : uint8_t {
    Drums,
    Chords,
    PatternLaunch,
    PerformanceFx
};

class PadScreen : public ScreenBase {
public:
    PadScreen();
    ~PadScreen() override = default;

    void update() override;
    void render(DisplayDriver& display) override;
    const char* name() const override { return "PadAssignments"; }

    void setBankMode(PadBankMode mode, const char* kit_or_preset_name);
    // 0 = unknown, 1 = physical Pad bank A, 2 = physical Pad bank B.
    // The SMK selector is silent, so this is inferred from incoming Note messages.
    void setObservedInputBank(uint8_t bank);
    void triggerPadHit(uint8_t pad_idx, uint8_t velocity);

private:
    PadBankMode bank_mode_{PadBankMode::Drums};
    char kit_name_[24]{"VINTAGE 808"};
    char pad_labels_[8][12]{
        "KICK 808", "SNARE DRY", "CHAT HI", "OHAT OPEN",
        "CLAP HAND", "TOM LOW",  "CYMBAL",  "COWBELL"
    };
    uint8_t last_hit_pad_{0};
    uint8_t last_hit_vel_{0};
    uint32_t last_hit_time_ms_{0};
    uint8_t observed_input_bank_{0};
};

} // namespace smk
