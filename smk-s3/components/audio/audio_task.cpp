#include "audio_task.h"
#include "diagnostics.h"
#include "audio_config.h"
#include <esp_log.h>
#include <esp_timer.h>

namespace smk {




static const char* TAG = "AUDIO_TASK";

SynthEngine* AudioTask::_engine = nullptr;
AudioOutput* AudioTask::_output = nullptr;
TaskHandle_t AudioTask::_task_handle = nullptr;
std::atomic<bool> AudioTask::_running{false};
std::atomic<bool> AudioTask::_failed{false};
std::atomic<uint32_t> AudioTask::_max_render_us{0};
std::atomic<uint32_t> AudioTask::_avg_render_us{0};

bool AudioTask::start(SynthEngine* engine, AudioOutput* output, uint8_t core_id, uint8_t priority, uint32_t stack_size_bytes) {
    if (_running || _task_handle || !engine || !output || stack_size_bytes == 0) return false;
    
    _engine = engine;
    _output = output;
    _running = true;
    _failed = false;
    _max_render_us = 0;
    _avg_render_us = 0;
    
    const BaseType_t result = xTaskCreatePinnedToCore(
        taskRoutine, 
        "audio_render_task", 
        stack_size_bytes,
        nullptr, 
        priority, 
        &_task_handle, 
        core_id
    );
    if (result != pdPASS) {
        _running = false;
        _failed = true;
        _task_handle = nullptr;
        return false;
    }
    return true;
}

void AudioTask::stop() {
    _running = false;
    // Task will self-terminate on next loop iteration.
}

uint32_t AudioTask::getMaxRenderUs() { return Diagnostics::instance().counters().max_render_us.load(); }
uint32_t AudioTask::getAvgRenderUs() { return Diagnostics::instance().counters().avg_render_us.load(); }

void AudioTask::taskRoutine(void* arg) {
    ESP_LOGI(TAG, "Audio task started on core %d", xPortGetCoreID());
    
    uint64_t ema_render_time = 0;

    uint32_t fade_frame = 0;

    while (_running) {
        uint64_t start_time = esp_timer_get_time();
        
        int16_t* buffer = _engine->render();
        uint16_t frames = _engine->blockSize();
        if (!buffer || frames != config::kBlockSize) {
            _failed = true;
            break;
        }
        // Fixed work, no allocation: start both channels at zero gain.
        for (uint16_t frame = 0; frame < frames && fade_frame < config::kAudioFadeInFrames; ++frame, ++fade_frame) {
            for (uint8_t channel = 0; channel < 2; ++channel) {
                const size_t index = frame * 2 + channel;
                buffer[index] = static_cast<int16_t>(static_cast<int32_t>(buffer[index]) *
                    static_cast<int32_t>(fade_frame) / static_cast<int32_t>(config::kAudioFadeInFrames));
            }
        }
        
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
        uint32_t avg_us = (uint32_t)(ema_render_time >> 4);
        _avg_render_us.store(avg_us);

        // Sync with Diagnostics subsystem
        auto& diag = Diagnostics::instance().counters();
        if (!_engine->bufferedOutput()) {
            diag.max_render_us.store(_max_render_us.load(), std::memory_order_relaxed);
            diag.avg_render_us.store(avg_us, std::memory_order_relaxed);
            diag.frames_rendered.fetch_add(frames, std::memory_order_relaxed);
        }
        
        // Write out blocks over I2S
        if (!_output->write(buffer, frames)) {
            _failed = true;
            break;
        }
    }
    
    _running = false;
    // Mute on failure/stop; the control task reports any failure, including stop.
    if (!_output->stop()) _failed = true;
    _engine->onAudioStopped();
    _task_handle = nullptr;
    vTaskDelete(nullptr);
}


} // namespace smk
