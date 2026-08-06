#include "ui_manager.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "UIManager";

namespace smk {

UIManager::UIManager(DisplayDriver& display)
    : display_(display), current_screen_(&home_screen_) {
}

UIManager::~UIManager() {
    stopTask();
}

bool UIManager::begin() {
    if (!display_.begin()) {
        ESP_LOGE(TAG, "Failed to initialize DisplayDriver");
        return false;
    }

    current_screen_id_ = ScreenId::Home;
    current_screen_ = &home_screen_;
    current_screen_->onEnter();

    ESP_LOGI(TAG, "UIManager initialized successfully");
    return true;
}

void UIManager::switchScreen(ScreenId screen_id) {
    if (current_screen_id_ == screen_id && current_screen_ != nullptr) return;

    if (current_screen_) {
        current_screen_->onExit();
    }

    current_screen_id_ = screen_id;
    switch (screen_id) {
        case ScreenId::Home: current_screen_ = &home_screen_; break;
        case ScreenId::System: current_screen_ = &system_screen_; break;
        case ScreenId::MidiMonitor: current_screen_ = &midi_monitor_screen_; break;
        case ScreenId::Sequencer: current_screen_ = &sequencer_screen_; break;
        case ScreenId::Pads: current_screen_ = &pad_screen_; break;
        default: current_screen_ = &home_screen_; break;
    }

    if (current_screen_) {
        current_screen_->onEnter();
        ESP_LOGI(TAG, "Switched to screen: %s", current_screen_->name());
    }
}

void UIManager::triggerParameterOverlay(const char* name, const char* target_layer,
                                        float current_val, float saved_val, 
                                        const char* unit_str, TakeoverStatus takeover) {
    parameter_screen_.showParameter(name, target_layer, current_val, saved_val, unit_str, takeover);
    overlay_active_ = true;
}

void UIManager::processEvent(const SynthEvent& event) {
    // Pass event to Midi Monitor
    midi_monitor_screen_.addEvent(event);

    // Handle Pad hits if PadScreen is active or for visualization
    if (event.type == EventType::NoteOn && event.id >= 36 && event.id <= 43) {
        pad_screen_.triggerPadHit(event.id - 36, event.value);
    }
}

void UIManager::startTask(uint8_t core_id, uint8_t priority) {
    if (running_) return;
    running_ = true;

    xTaskCreatePinnedToCore(
        uiTaskRoutine,
        "ui_render_task",
        8192,
        this,
        priority,
        &task_handle_,
        core_id
    );
    ESP_LOGI(TAG, "UI render task started on Core %d", core_id);
}

void UIManager::stopTask() {
    running_ = false;
    if (task_handle_) {
        task_handle_ = nullptr;
    }
}

void UIManager::uiTaskRoutine(void* arg) {
    UIManager* self = static_cast<UIManager*>(arg);
    const TickType_t frame_interval = pdMS_TO_TICKS(33); // ~30 FPS

    while (self->running_) {
        TickType_t start_tick = xTaskGetTickCount();

        // Update active screen / overlay
        if (self->overlay_active_) {
            self->parameter_screen_.update();
            if (self->parameter_screen_.isExpired()) {
                self->overlay_active_ = false;
            }
        }

        if (self->current_screen_) {
            self->current_screen_->update();
        }

        // Render phase
        if (self->overlay_active_) {
            self->parameter_screen_.render(self->display_);
        } else if (self->current_screen_) {
            self->current_screen_->render(self->display_);
        }

        self->display_.flush();

        vTaskDelayUntil(&start_tick, frame_interval);
    }

    vTaskDelete(nullptr);
}

} // namespace smk
