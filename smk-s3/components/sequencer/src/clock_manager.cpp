#include "clock_manager.h"
#include "esp_log.h"
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

    ESP_LOGI(TAG, "ClockManager initialized at %.1f BPM (%u PPQN)", bpm_, kPpqn);
    return true;
}

void ClockManager::setBpm(float bpm) {
    bpm_ = std::clamp(bpm, 30.0f, 300.0f);
    if (running_) {
        updateTimerPeriod();
    }
}

void ClockManager::setSwing(uint8_t swing_percent) {
    swing_percent_ = std::clamp(swing_percent, (uint8_t)50, (uint8_t)75);
}

void ClockManager::setClockSource(ClockSource source) {
    clock_source_ = source;
    if (source == ClockSource::UsbMidi) {
        if (timer_handle_) {
            esp_timer_stop(timer_handle_);
        }
    } else {
        if (running_) {
            updateTimerPeriod();
        }
    }
}

void ClockManager::onExternalTick() {
    if (clock_source_ != ClockSource::UsbMidi || !running_) return;
    tick_count_++;
    if (callback_) {
        callback_(tick_count_, callback_ctx_);
    }
}

void ClockManager::onExternalStart() {
    if (clock_source_ != ClockSource::UsbMidi) return;
    tick_count_ = 0;
    running_ = true;
    ESP_LOGI(TAG, "External USB MIDI Clock Started");
}

void ClockManager::onExternalStop() {
    if (clock_source_ != ClockSource::UsbMidi) return;
    running_ = false;
    ESP_LOGI(TAG, "External USB MIDI Clock Stopped");
}

void ClockManager::start() {
    if (running_) return;
    tick_count_ = 0;
    running_ = true;
    if (clock_source_ == ClockSource::Internal) {
        updateTimerPeriod();
    }
    ESP_LOGI(TAG, "Clock started (Source: %s, %.1f BPM)", 
             (clock_source_ == ClockSource::Internal ? "INTERNAL" : "USB_MIDI"), bpm_);
}

void ClockManager::stop() {
    if (!running_) return;
    running_ = false;
    if (timer_handle_) {
        esp_timer_stop(timer_handle_);
    }
    ESP_LOGI(TAG, "Clock stopped");
}

void ClockManager::setCallback(TickCallback callback, void* ctx) {
    callback_ = callback;
    callback_ctx_ = ctx;
}

void ClockManager::updateTimerPeriod() {
    if (!timer_handle_) return;

    // Period per PPQN tick in microseconds
    double quarter_note_us = (60.0 / (double)bpm_) * 1000000.0;
    uint64_t period_us = static_cast<uint64_t>(quarter_note_us / (double)kPpqn);

    esp_timer_stop(timer_handle_);
    esp_timer_start_periodic(timer_handle_, period_us);
}

void ClockManager::timerCallback(void* arg) {
    ClockManager* self = static_cast<ClockManager*>(arg);
    if (!self->running_) return;

    self->tick_count_++;
    if (self->callback_) {
        self->callback_(self->tick_count_, self->callback_ctx_);
    }
}

} // namespace smk
