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

void ClockManager::start() {
    if (running_) return;
    tick_count_ = 0;
    running_ = true;
    updateTimerPeriod();
    ESP_LOGI(TAG, "Clock started at %.1f BPM", bpm_);
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
