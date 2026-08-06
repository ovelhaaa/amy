#pragma once

#include "screen_base.h"
#include "widgets.h"
#include <cstdint>

namespace smk {

class HomeScreen : public ScreenBase {
public:
    HomeScreen();
    ~HomeScreen() override = default;

    void onEnter() override;
    void update() override;
    void render(DisplayDriver& display) override;
    const char* name() const override { return "Home"; }

    void setPatchInfo(uint16_t number, const char* name, const char* mode);
    void setBpm(float bpm);
    void setUsbConnected(bool connected);
    void setMidiActivity(bool active);
    void setMacroValues(const uint8_t values[8]);
    void setKnobBankLabel(const char* bank_name);

private:
    uint16_t patch_number_{42};
    char patch_name_[24]{"GLASS HORIZON"};
    char synth_mode_[8]{"LYR"};
    float bpm_{120.0f};
    bool usb_connected_{true};
    bool midi_active_{false};
    char knob_bank_[16]{"BANK A: MACROS"};
    uint8_t macro_values_[8]{62, 78, 31, 45, 8, 67, 34, 22};
    
    BarGauge gauges_[8];
};

} // namespace smk
