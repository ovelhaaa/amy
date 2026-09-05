#pragma once
#include "synth_engine.h"
#include "audio_output.h"
#include "test_tone.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>

namespace smk {




class AudioTask {
public:
    static bool start(SynthEngine* engine, AudioOutput* output, uint8_t core_id, uint8_t priority, uint32_t stack_size_bytes);
    static void stop();
    static bool failed() { return _failed.load(); }
    
    static uint32_t getMaxRenderUs();
    static uint32_t getAvgRenderUs();

private:
    static void taskRoutine(void* arg);
    
    static SynthEngine* _engine;
    static AudioOutput* _output;
    static TaskHandle_t _task_handle;
    static std::atomic<bool> _running;
    static std::atomic<bool> _failed;
    static std::atomic<uint32_t> _max_render_us;
    static std::atomic<uint32_t> _avg_render_us;
};


} // namespace smk
