#pragma once
#include <cstdint>
#include <atomic>
#include "esp_timer.h"

namespace smk {

enum class ClockSource : uint8_t {
    Internal = 0,
    UsbMidi
};

class ClockManager {
public:
    static constexpr uint32_t kPpqn = 24; // 24 pulses per quarter note (standard MIDI clock)

    using TickCallback = void(*)(uint32_t tick_count, void* ctx);

    ClockManager();
    ~ClockManager();

    bool begin();
    void setBpm(float bpm);
    float bpm() const { return bpm_; }

    void setSwing(uint8_t swing_percent);
    uint8_t swing() const { return swing_percent_; }

    void setClockSource(ClockSource source);
    ClockSource clockSource() const { return clock_source_; }

    void start();
    void stop();
    bool isRunning() const { return running_; }

    void onExternalTick();
    void onExternalStart();
    void onExternalStop();

    void setCallback(TickCallback callback, void* ctx);

private:
    static void timerCallback(void* arg);
    void updateTimerPeriod();

    float                   bpm_ = 120.0f;
    uint8_t                 swing_percent_ = 50; // 50% = Straight, 67% = Triplet, 75% = Dotted
    ClockSource             clock_source_ = ClockSource::Internal;
    std::atomic<bool>       running_{false};
    uint32_t                tick_count_ = 0;
    TickCallback            callback_ = nullptr;
    void*                   callback_ctx_ = nullptr;
    esp_timer_handle_t      timer_handle_ = nullptr;
};

} // namespace smk
