#include "audio_task.h"
#include <esp_log.h>
#include <esp_timer.h>

namespace smk {




static const char* TAG = "AUDIO_TASK";

SynthEngine* AudioTask::_engine = nullptr;
AudioOutput* AudioTask::_output = nullptr;
TaskHandle_t AudioTask::_task_handle = nullptr;
std::atomic<bool> AudioTask::_running{false};
std::atomic<uint32_t> AudioTask::_max_render_us{0};
std::atomic<uint32_t> AudioTask::_avg_render_us{0};

void AudioTask::start(SynthEngine* engine, AudioOutput* output, uint8_t core_id, uint8_t priority) {
    if (_running) return;
    
    _engine = engine;
    _output = output;
    _running = true;
    _max_render_us = 0;
    _avg_render_us = 0;
    
    xTaskCreatePinnedToCore(
        taskRoutine, 
        "audio_render_task", 
        16384, 
        nullptr, 
        priority, 
        &_task_handle, 
        core_id
    );
}

void AudioTask::stop() {
    _running = false;
    // Task will self-terminate on next loop iteration.
}

uint32_t AudioTask::getMaxRenderUs() { return _max_render_us.load(); }
uint32_t AudioTask::getAvgRenderUs() { return _avg_render_us.load(); }

void AudioTask::taskRoutine(void* arg) {
    ESP_LOGI(TAG, "Audio task started on core %d", xPortGetCoreID());
    
    uint64_t ema_render_time = 0;

    // Optional: Fade-in block logic can go here. For now we just render.

    while (_running) {
        uint64_t start_time = esp_timer_get_time();
        
        int16_t* buffer = _engine->render();
        uint16_t frames = _engine->blockSize();
        
        uint64_t end_time = esp_timer_get_time();
        uint32_t render_time = (uint32_t)(end_time - start_time);
        
        // Track max render time
        if (render_time > _max_render_us.load()) {
            _max_render_us.store(render_time);
        }
        
        // Track average render time (exponential moving average)
        if (ema_render_time == 0) {
            ema_render_time = render_time << 4;
        } else {
            ema_render_time = (ema_render_time * 15 + (render_time << 4)) / 16;
        }
        _avg_render_us.store((uint32_t)(ema_render_time >> 4));
        
        // Write out blocks over I2S
        _output->write(buffer, frames);
    }
    
    ESP_LOGI(TAG, "Audio task stopping");
    _task_handle = nullptr;
    vTaskDelete(nullptr);
}


} // namespace smk
