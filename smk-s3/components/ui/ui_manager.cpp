#include "ui_manager.h"
#include "synth_engine.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "UIManager";

namespace smk {

UIManager::UIManager(DisplayDriver& display)
    : display_(display), current_screen_(&splash_screen_) {
}

UIManager::~UIManager() {
    stopTask();
}

bool UIManager::begin() {
    current_screen_id_ = ScreenId::Splash;
    current_screen_ = &splash_screen_;
    current_screen_->onEnter();

    ESP_LOGI(TAG, "UIManager initialized with SplashScreen");
    return true;
}

void UIManager::switchScreen(ScreenId screen_id) {
    if (current_screen_id_ == screen_id && current_screen_ != nullptr) return;

    if (current_screen_) {
        current_screen_->onExit();
    }

    current_screen_id_ = screen_id;
    switch (screen_id) {
        case ScreenId::Splash: current_screen_ = &splash_screen_; break;
        case ScreenId::Home: current_screen_ = &home_screen_; break;
        case ScreenId::System: current_screen_ = &system_screen_; break;
        case ScreenId::MidiMonitor: current_screen_ = &midi_monitor_screen_; break;
        case ScreenId::Sequencer: current_screen_ = &sequencer_screen_; break;
        case ScreenId::Pads: current_screen_ = &pad_screen_; break;
        case ScreenId::Scenes: current_screen_ = &scene_screen_; break;
        case ScreenId::MidiLearn: current_screen_ = &midi_learn_screen_; break;
        default: current_screen_ = &home_screen_; break;
    }

    if (current_screen_) {
        if (screen_id != ScreenId::Splash) {
            display_.setBrightness(255);
        }
        display_.invalidate();
        current_screen_->onEnter();
        ESP_LOGI(TAG, "Switched to screen: %s", current_screen_->name());
    }
}

void UIManager::nextPage() {
    // Navigation pages: 1=Home, 2=System, 3=MidiMonitor, 4=Sequencer, 5=Pads, 6=MidiLearn, 7=Scenes
    uint8_t id = static_cast<uint8_t>(current_screen_id_);
    if (id < 1 || id > 7) id = 1; // From splash or unknown -> Home
    else id = (id == 7) ? 1 : (id + 1);
    switchScreen(static_cast<ScreenId>(id));
}

void UIManager::previousPage() {
    uint8_t id = static_cast<uint8_t>(current_screen_id_);
    if (id < 1 || id > 7) id = 1;
    else id = (id == 1) ? 7 : (id - 1);
    switchScreen(static_cast<ScreenId>(id));
}

void UIManager::setPage(uint8_t page_idx) {
    if (page_idx >= 1 && page_idx <= 7) {
        switchScreen(static_cast<ScreenId>(page_idx));
    }
}

void UIManager::triggerParameterOverlay(const char* name, const char* target_layer,
                                        float current_val, float saved_val, 
                                        const char* unit_str, TakeoverStatus takeover) {
    if (current_screen_id_ == ScreenId::Splash) {
        return; // Suppress parameter overlays during boot splash screen
    }
    parameter_screen_.showParameter(name, target_layer, current_val, saved_val, unit_str, takeover);
    overlay_active_ = true;
}

void UIManager::processEvent(const SynthEvent& event) {
    // If Splash Screen is displaying and an explicit interactive key/button is pressed after 600ms, dismiss splash
    if (current_screen_id_ == ScreenId::Splash) {
        uint32_t now = static_cast<uint32_t>(esp_timer_get_time() / 1000);
        if (now - splash_screen_.startTime() > 600) {
            if (event.type == EventType::NoteOn || event.type == EventType::ButtonPress) {
                splash_screen_.dismiss();
                switchScreen(ScreenId::Home);
            }
        }
    }

    // Pass event to Midi Monitor
    midi_monitor_screen_.addEvent(event);

    // Trigger visual MIDI Activity on HomeScreen (LED and oscilloscope pulse)
    home_screen_.setMidiActivity(true);

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

        // Check if Splash screen is finished -> auto-transition to HomeScreen
        if (self->current_screen_id_ == ScreenId::Splash && self->splash_screen_.isFinished()) {
            self->switchScreen(ScreenId::Home);
        }

        // Fetch real audio samples for oscilloscope visualization on HomeScreen
        if (self->synth_engine_ && self->current_screen_id_ == ScreenId::Home) {
            int16_t scope_samples[128];
            size_t sample_count = 0;
            self->synth_engine_->getScopeSamples(scope_samples, 128, &sample_count);
            if (sample_count > 0) {
                self->home_screen_.setWaveformSamples(scope_samples, sample_count);
            }
        }

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
