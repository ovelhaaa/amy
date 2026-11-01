#include "clock_manager.h"
#include "diagnostics.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <algorithm>
#include <cmath>

static const char* TAG = "ClockManager";

namespace smk {

ClockManager::ClockManager() {}

ClockManager::~ClockManager() {
    stop();
    if (timer_handle_) {
        esp_timer_delete(timer_handle_);
        timer_handle_ = nullptr;
    }
}

bool ClockManager::begin() {
    std::lock_guard<std::recursive_mutex> api_guard(api_mutex_);
    esp_timer_create_args_t timer_args = {
        .callback = &ClockManager::timerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "clock_timer",
        .skip_unhandled_events = true
    };

    esp_err_t err = esp_timer_create(&timer_args, &timer_handle_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create clock timer: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "ClockManager initialized at %.1f BPM (%u PPQN)", bpm(), static_cast<unsigned int>(kPpqn));
    return true;
}

void ClockManager::setBpm(float bpm) {
    std::lock_guard<std::recursive_mutex> api_guard(api_mutex_);
    if (!std::isfinite(bpm)) return;
    {
        TemporalGuard guard(timing_lock_);
        bpm_.store(std::clamp(bpm, 30.0f, 300.0f), std::memory_order_relaxed);
        previous_tick_us_.store(0, std::memory_order_relaxed);
        next_tick_us_ = static_cast<uint64_t>(esp_timer_get_time()) +
            static_cast<uint64_t>(60000000.0 / this->bpm() / kPpqn);
    }
    if (running_.load(std::memory_order_relaxed) && clockSource() == ClockSource::Internal) {
        updateTimerPeriod();
    }
}

void ClockManager::setSwing(uint8_t swing_percent) {
    std::lock_guard<std::recursive_mutex> api_guard(api_mutex_);
    swing_percent_.store(std::clamp(swing_percent, (uint8_t)50, (uint8_t)75), std::memory_order_relaxed);
}

void ClockManager::setClockSource(ClockSource source) {
    std::lock_guard<std::recursive_mutex> api_guard(api_mutex_);
    {
        TemporalGuard guard(timing_lock_);
        clock_source_.store(source, std::memory_order_relaxed);
        pending_ticks_.reset(tick_count_.load(std::memory_order_relaxed));
        previous_tick_us_.store(0, std::memory_order_relaxed);
        previous_external_tick_us_ = external_period_average_us_ = 0;
        next_tick_us_ = static_cast<uint64_t>(esp_timer_get_time()) +
            static_cast<uint64_t>(60000000.0 / bpm() / kPpqn);
    }
    if (source == ClockSource::UsbMidi) {
        if (timer_handle_) {
            esp_timer_stop(timer_handle_);
        }
    } else {
        if (running_.load(std::memory_order_relaxed)) {
            updateTimerPeriod();
        }
    }
}

void ClockManager::onExternalTick(uint64_t timestamp_us) {
    std::lock_guard<std::recursive_mutex> api_guard(api_mutex_);
    if (clock_source_.load(std::memory_order_relaxed) != ClockSource::UsbMidi ||
        !running_.load(std::memory_order_relaxed)) return;
    TemporalGuard guard(timing_lock_);
    const uint32_t tick = tick_count_.fetch_add(1, std::memory_order_relaxed) + 1;
    if (timestamp_us > previous_external_tick_us_ && previous_external_tick_us_) {
        const uint64_t interval_us = timestamp_us - previous_external_tick_us_;
        if (external_period_average_us_) {
            const uint32_t error_us = static_cast<uint32_t>(interval_us > external_period_average_us_
                ? interval_us - external_period_average_us_ : external_period_average_us_ - interval_us);
            auto& max_error = Diagnostics::instance().counters().clock_max_interval_error_us;
            uint32_t observed = max_error.load(std::memory_order_relaxed);
            if (error_us > observed) max_error.store(error_us, std::memory_order_relaxed);
        }
        external_period_average_us_ = external_period_average_us_
            ? (external_period_average_us_ * 7 + interval_us) / 8 : interval_us;
    }
    if (timestamp_us) previous_external_tick_us_ = timestamp_us;
    pending_ticks_.publish(tick, timestamp_us, external_period_average_us_ ? external_period_average_us_ : static_cast<uint64_t>(60000000.0 / bpm() / kPpqn), true);
}

void ClockManager::onExternalStart() {
    std::lock_guard<std::recursive_mutex> api_guard(api_mutex_);
    if (clock_source_.load(std::memory_order_relaxed) != ClockSource::UsbMidi) return;
    {
    TemporalGuard guard(timing_lock_);
    tick_count_.store(0, std::memory_order_relaxed);
    pending_ticks_.reset();
    previous_tick_us_.store(0, std::memory_order_relaxed);
    previous_external_tick_us_ = 0;
    external_period_average_us_ = 0;
    running_ = true;
    }
    ESP_LOGI(TAG, "External USB MIDI Clock Started");
}

void ClockManager::onExternalStop() {
    std::lock_guard<std::recursive_mutex> api_guard(api_mutex_);
    if (clock_source_.load(std::memory_order_relaxed) != ClockSource::UsbMidi) return;
    {
    TemporalGuard guard(timing_lock_);
    running_ = false;
    pending_ticks_.reset(tick_count_.load(std::memory_order_relaxed));
    previous_external_tick_us_ = 0;
    external_period_average_us_ = 0;
    }
    ESP_LOGI(TAG, "External USB MIDI Clock Stopped");
}

void ClockManager::start() {
    std::lock_guard<std::recursive_mutex> api_guard(api_mutex_);
    if (running_.load(std::memory_order_relaxed)) return;
    {
    TemporalGuard guard(timing_lock_);
    tick_count_.store(0, std::memory_order_relaxed);
    pending_ticks_.reset();
    previous_tick_us_.store(0, std::memory_order_relaxed);
    next_tick_us_ = static_cast<uint64_t>(esp_timer_get_time()) +
        static_cast<uint64_t>(60000000.0 / bpm() / kPpqn);
    running_.store(true, std::memory_order_relaxed);
    }
    if (clock_source_.load(std::memory_order_relaxed) == ClockSource::Internal) {
        updateTimerPeriod();
    }
    ESP_LOGI(TAG, "Clock started (Source: %s, %.1f BPM)", 
             (clock_source_.load(std::memory_order_relaxed) == ClockSource::Internal ? "INTERNAL" : "USB_MIDI"), bpm());
}

void ClockManager::stop() {
    std::lock_guard<std::recursive_mutex> api_guard(api_mutex_);
    {
        TemporalGuard guard(timing_lock_);
        running_.store(false, std::memory_order_relaxed);
        pending_ticks_.reset(tick_count_.load(std::memory_order_relaxed));
    }
    if (timer_handle_) {
        esp_timer_stop(timer_handle_);
    }
    ESP_LOGI(TAG, "Clock stopped");
}

void ClockManager::updateTimerPeriod() {
    if (!timer_handle_) return;

    // Period per PPQN tick in microseconds
    double quarter_note_us = (60.0 / static_cast<double>(bpm())) * 1000000.0;
    uint64_t period_us = static_cast<uint64_t>(quarter_note_us / (double)kPpqn);

    esp_timer_stop(timer_handle_);
    const esp_err_t err = esp_timer_start_periodic(timer_handle_, period_us);
    if (err != ESP_OK) {
        TemporalGuard guard(timing_lock_);
        running_ = false;
        pending_ticks_.reset(tick_count_.load());
    }
    if (err != ESP_OK) ESP_LOGE(TAG, "Clock timer start failed: %s", esp_err_to_name(err));
}

void ClockManager::timerCallback(void* arg) {
    ClockManager* self = static_cast<ClockManager*>(arg);
    TemporalGuard guard(self->timing_lock_);
    if (!self->running_.load(std::memory_order_relaxed) ||
        self->clock_source_.load(std::memory_order_relaxed) != ClockSource::Internal) return;

    const uint64_t now_us = static_cast<uint64_t>(esp_timer_get_time());
    if (now_us < self->next_tick_us_) return; // Old callback after restart/BPM change.
    const uint64_t period_us = static_cast<uint64_t>(60000000.0 / self->bpm() / kPpqn);
    const uint64_t edges = 1 + (now_us - self->next_tick_us_) / period_us;
    const uint32_t tick = self->tick_count_.fetch_add(static_cast<uint32_t>(edges), std::memory_order_relaxed) + static_cast<uint32_t>(edges);
    const uint64_t deadline_us = self->next_tick_us_ + (edges - 1) * period_us;
    self->next_tick_us_ += edges * period_us;
    const uint64_t previous_tick_us = self->previous_tick_us_.load(std::memory_order_relaxed);
    if (previous_tick_us != 0) {
        const uint64_t elapsed_us = now_us - previous_tick_us;
        const uint64_t period_us = static_cast<uint64_t>(60000000.0 / self->bpm() / kPpqn);
        const uint32_t jitter_us = static_cast<uint32_t>(elapsed_us > period_us ? elapsed_us - period_us : period_us - elapsed_us);
        auto& max_jitter = Diagnostics::instance().counters().clock_max_interval_error_us;
        uint32_t observed = max_jitter.load(std::memory_order_relaxed);
        if (jitter_us > observed) max_jitter.store(jitter_us, std::memory_order_relaxed);
    }
    self->previous_tick_us_.store(now_us, std::memory_order_relaxed);
    self->pending_ticks_.publish(tick, deadline_us, period_us, false);
}

} // namespace smk
