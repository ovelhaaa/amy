#pragma once
#include <cstdint>
#include <atomic>
#include "esp_timer.h"

namespace smk {

class ClockManager {
public:
    static constexpr uint32_t kPpqn = 24; // 24 pulses per quarter note (standard MIDI clock)

    using TickCallback = void(*)(uint32_t tick_count, void* ctx);

    ClockManager();
    ~ClockManager();

    bool begin();
    void setBpm(float bpm);
    float bpm() const { return bpm_; }

    void start();
    void stop();
    bool isRunning() const { return running_; }

    void setCallback(TickCallback callback, void* ctx);

private:
    static void timerCallback(void* arg);
    void updateTimerPeriod();

    float                   bpm_ = 120.0f;
    std::atomic<bool>       running_{false};
    uint32_t                tick_count_ = 0;
    TickCallback            callback_ = nullptr;
    void*                   callback_ctx_ = nullptr;
    esp_timer_handle_t      timer_handle_ = nullptr;
};

} // namespace smk
