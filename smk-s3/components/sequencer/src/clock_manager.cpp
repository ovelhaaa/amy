#include "clock_manager.h"
#include "diagnostics.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <algorithm>

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
    bpm_.store(std::clamp(bpm, 30.0f, 300.0f), std::memory_order_relaxed);
    previous_tick_us_.store(0, std::memory_order_relaxed);
    if (running_.load(std::memory_order_relaxed)) {
        updateTimerPeriod();
    }
}

void ClockManager::setSwing(uint8_t swing_percent) {
    swing_percent_.store(std::clamp(swing_percent, (uint8_t)50, (uint8_t)75), std::memory_order_relaxed);
}

void ClockManager::setClockSource(ClockSource source) {
    clock_source_.store(source, std::memory_order_relaxed);
    if (source == ClockSource::UsbMidi) {
        if (timer_handle_) {
            esp_timer_stop(timer_handle_);
        }
        pending_ticks_.reset(tick_count_.load(std::memory_order_relaxed));
        previous_tick_us_.store(0, std::memory_order_relaxed);
    } else {
        if (running_.load(std::memory_order_relaxed)) {
            updateTimerPeriod();
        }
    }
}

void ClockManager::onExternalTick(uint64_t timestamp_us) {
    if (clock_source_.load(std::memory_order_relaxed) != ClockSource::UsbMidi ||
        !running_.load(std::memory_order_relaxed)) return;
    const uint32_t tick = tick_count_.fetch_add(1, std::memory_order_relaxed) + 1;
    if (timestamp_us && previous_external_tick_us_) {
        const uint64_t interval_us = timestamp_us - previous_external_tick_us_;
        if (external_period_average_us_) {
            const uint32_t error_us = static_cast<uint32_t>(interval_us > external_period_average_us_
                ? interval_us - external_period_average_us_ : external_period_average_us_ - interval_us);
            auto& max_error = Diagnostics::instance().counters().clock_max_interval_error_us;
            uint32_t observed = max_error.load(std::memory_order_relaxed);
            while (error_us > observed && !max_error.compare_exchange_weak(observed, error_us, std::memory_order_relaxed)) {}
        }
        external_period_average_us_ = external_period_average_us_
            ? (external_period_average_us_ * 7 + interval_us) / 8 : interval_us;
    }
    if (timestamp_us) previous_external_tick_us_ = timestamp_us;
    pending_ticks_.publish(tick, timestamp_us);
}

void ClockManager::onExternalStart() {
    if (clock_source_.load(std::memory_order_relaxed) != ClockSource::UsbMidi) return;
    tick_count_.store(0, std::memory_order_relaxed);
    pending_ticks_.reset();
    previous_tick_us_.store(0, std::memory_order_relaxed);
    previous_external_tick_us_ = 0;
    external_period_average_us_ = 0;
    running_ = true;
    ESP_LOGI(TAG, "External USB MIDI Clock Started");
}

void ClockManager::onExternalStop() {
    if (clock_source_.load(std::memory_order_relaxed) != ClockSource::UsbMidi) return;
    running_ = false;
    pending_ticks_.reset(tick_count_.load(std::memory_order_relaxed));
    previous_external_tick_us_ = 0;
    external_period_average_us_ = 0;
    ESP_LOGI(TAG, "External USB MIDI Clock Stopped");
}

void ClockManager::start() {
    if (running_.load(std::memory_order_relaxed)) return;
    tick_count_.store(0, std::memory_order_relaxed);
    pending_ticks_.reset();
    previous_tick_us_.store(0, std::memory_order_relaxed);
    running_.store(true, std::memory_order_relaxed);
    if (clock_source_.load(std::memory_order_relaxed) == ClockSource::Internal) {
        updateTimerPeriod();
    }
    ESP_LOGI(TAG, "Clock started (Source: %s, %.1f BPM)", 
             (clock_source_.load(std::memory_order_relaxed) == ClockSource::Internal ? "INTERNAL" : "USB_MIDI"), bpm());
}

void ClockManager::stop() {
    if (!running_.exchange(false, std::memory_order_relaxed)) return;
    if (timer_handle_) {
        esp_timer_stop(timer_handle_);
    }
    pending_ticks_.reset(tick_count_.load(std::memory_order_relaxed));
    ESP_LOGI(TAG, "Clock stopped");
}

void ClockManager::updateTimerPeriod() {
    if (!timer_handle_) return;

    // Period per PPQN tick in microseconds
    double quarter_note_us = (60.0 / static_cast<double>(bpm())) * 1000000.0;
    uint64_t period_us = static_cast<uint64_t>(quarter_note_us / (double)kPpqn);

    esp_timer_stop(timer_handle_);
    esp_timer_start_periodic(timer_handle_, period_us);
}

void ClockManager::timerCallback(void* arg) {
    ClockManager* self = static_cast<ClockManager*>(arg);
    if (!self->running_.load(std::memory_order_relaxed) ||
        self->clock_source_.load(std::memory_order_relaxed) != ClockSource::Internal) return;

    const uint32_t tick = self->tick_count_.fetch_add(1, std::memory_order_relaxed) + 1;
    const uint64_t now_us = static_cast<uint64_t>(esp_timer_get_time());
    const uint64_t previous_tick_us = self->previous_tick_us_.load(std::memory_order_relaxed);
    if (previous_tick_us != 0) {
        const uint64_t elapsed_us = now_us - previous_tick_us;
        const uint64_t period_us = static_cast<uint64_t>(60000000.0 / self->bpm() / kPpqn);
        const uint32_t jitter_us = static_cast<uint32_t>(elapsed_us > period_us ? elapsed_us - period_us : period_us - elapsed_us);
        auto& max_jitter = Diagnostics::instance().counters().clock_max_interval_error_us;
        uint32_t observed = max_jitter.load(std::memory_order_relaxed);
        while (jitter_us > observed && !max_jitter.compare_exchange_weak(observed, jitter_us, std::memory_order_relaxed)) {}
    }
    self->previous_tick_us_.store(now_us, std::memory_order_relaxed);
    self->pending_ticks_.publish(tick, now_us);
}

} // namespace smk
