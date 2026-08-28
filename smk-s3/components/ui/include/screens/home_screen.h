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

    enum class HomeKnobBankView : uint8_t {
        BankA_Macros = 0,
        BankB_Engine = 1
    };

    void setPatchInfo(uint16_t number, const char* name, const char* mode);
    void setBpm(float bpm);
    void setUsbConnected(bool connected);
    void setMidiActivity(bool active);
    void setMacroValues(const uint8_t values[8]);
    void setEngineValues(const uint8_t values[8]);
    void setHomeKnobBankView(HomeKnobBankView view);
    HomeKnobBankView homeKnobBankView() const { return bank_view_; }
    void setKnobBankLabel(const char* bank_name);
    void setActiveVoices(uint8_t active_count, uint8_t max_voices = 8);
    void setCpuLoad(float load_percent);
    void setWaveformSamples(const int16_t* samples, size_t count);

private:
    uint16_t patch_number_{42};
    char patch_name_[24]{"GLASS HORIZON"};
    char synth_mode_[8]{"LYR"};
    float bpm_{120.0f};
    bool usb_connected_{true};
    bool midi_active_{false};
    char knob_bank_[24]{"BANK A: MACROS"};
    HomeKnobBankView bank_view_{HomeKnobBankView::BankA_Macros};
    uint8_t macro_values_[8]{62, 78, 31, 45, 8, 67, 34, 22};
    uint8_t engine_values_[8]{64, 40, 50, 45, 20, 30, 40, 15};
    uint8_t active_voices_{0};
    uint8_t max_voices_{8};
    float cpu_load_{12.5f};
    uint32_t scope_phase_{0};
    uint32_t midi_activity_timer_{0};
    int16_t scope_samples_[128]{};
    size_t scope_sample_count_{0};
    
    BarGauge gauges_[8];
};

} // namespace smk
