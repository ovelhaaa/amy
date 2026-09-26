#pragma once

#include <atomic>
#include <cstdint>

namespace smk {

// Audio budget/load formulas. Kept free-standing and constexpr so they are
// host-testable without pulling in the audio component, and so the exact same
// expression backs both the ESP snapshot and the regression test.
//   budget_us(128, 48000) = 2666.7 us
//   budget_us(256, 48000) = 5333.3 us
constexpr float audioBlockBudgetUs(uint32_t block_size, uint32_t sample_rate_hz) {
    return (sample_rate_hz == 0)
        ? 0.0f
        : 1000000.0f * static_cast<float>(block_size) / static_cast<float>(sample_rate_hz);
}

// Render load as a percentage in 0..100 (not a 0..1 fraction).
constexpr float audioRenderLoadPercent(uint32_t render_us, uint32_t block_size,
                                       uint32_t sample_rate_hz) {
    const float budget = audioBlockBudgetUs(block_size, sample_rate_hz);
    return (budget <= 0.0f) ? 0.0f : 100.0f * static_cast<float>(render_us) / budget;
}

struct DiagnosticCounters {
    // Audio render timing. These describe the synthesis owner's per-block work
    // (command application + AMY render); see audio_config.h for the block
    // budget they are compared against.
    std::atomic<uint32_t> audio_underruns{0};
    std::atomic<uint32_t> max_render_us{0};
    std::atomic<uint32_t> avg_render_us{0};
    std::atomic<uint32_t> frames_rendered{0};
    std::atomic<uint32_t> synth_pcm_starvations{0};
    std::atomic<uint32_t> synth_commands_dropped{0};
    std::atomic<uint32_t> synth_queue_high_water{0};
    std::atomic<uint32_t> synth_panics{0};
    std::atomic<uint32_t> synth_max_command_wait_us{0};
    // FM operator controls with no valid patch baseline were ignored rather
    // than overwriting the preset's operator topology with absolute values.
    std::atomic<uint32_t> fm_controls_ignored{0};

    // Output headroom telemetry, measured on the final post-master-gain PCM
    // block by the synthesis owner. peak_abs_sample is the maximum |sample| in
    // the current measurement window (0..32768). near_clip_samples and
    // hard_clip_samples are cumulative counts within the same window.
    //   near clip: |sample| >= 32106 (0.98 FS)
    //   hard clip: |sample| >= 32767
    std::atomic<uint32_t> peak_abs_sample{0};
    std::atomic<uint32_t> near_clip_samples{0};
    std::atomic<uint32_t> hard_clip_samples{0};

    // MIDI
    std::atomic<uint32_t> midi_parse_errors{0};
    std::atomic<uint32_t> events_received{0};
    std::atomic<uint32_t> events_dropped{0};
    std::atomic<uint32_t> event_queue_overflows{0};
    
    // USB
    std::atomic<uint32_t> usb_disconnects{0};
    std::atomic<uint32_t> usb_reconnects{0};
    std::atomic<bool>     usb_connected{false};
    
    // System
    std::atomic<uint32_t> panic_count{0};
    std::atomic<uint32_t> active_voices{0};
};

class Diagnostics {
public:
    static Diagnostics& instance();
    
    DiagnosticCounters& counters();
    
    // Snapshot for display/logging
    struct Snapshot {
        uint32_t audio_underruns;
        uint32_t max_render_us;
        uint32_t avg_render_us;
        uint32_t frames_rendered;
        uint32_t free_internal_ram;
        uint32_t free_psram;
        uint32_t largest_free_internal_block;
        uint32_t largest_free_psram_block;
        uint32_t cpu_freq_mhz;
        uint32_t flash_size;
        uint32_t psram_size;
        // Render timing context: the budget is derived from the active build's
        // block size and sample rate, so 128 and 256 builds report comparable
        // percentages for equivalent work.
        uint32_t block_size;
        uint32_t sample_rate_hz;
        float block_budget_us;
        // render_load is a percentage in 0..100 (not a 0..1 fraction):
        //   render_load     = avg_render_us / block_budget_us * 100
        //   max_render_load = max_render_us / block_budget_us * 100
        float render_load;
        float max_render_load;
        uint32_t peak_abs_sample;
        uint32_t near_clip_samples;
        uint32_t hard_clip_samples;
        uint32_t active_voices;
        uint32_t midi_parse_errors;
        uint32_t events_received;
        uint32_t event_queue_overflows;
        uint32_t usb_disconnects;
        uint32_t usb_reconnects;
        uint32_t panic_count;
        bool usb_connected;
        const char* firmware_version;
    };
    
    Snapshot takeSnapshot() const;
    void logSnapshot() const;

    // Clears only the runtime audio qualification metrics so each hardware test
    // run produces a clean window. USB connection state, MIDI counters and
    // system counters are intentionally preserved. All fields are relaxed
    // atomics, so this is safe to call from any task.
    void resetAudioMetrics() {
        auto& c = counters_;
        c.audio_underruns.store(0, std::memory_order_relaxed);
        c.max_render_us.store(0, std::memory_order_relaxed);
        c.avg_render_us.store(0, std::memory_order_relaxed);
        c.frames_rendered.store(0, std::memory_order_relaxed);
        c.synth_pcm_starvations.store(0, std::memory_order_relaxed);
        c.synth_commands_dropped.store(0, std::memory_order_relaxed);
        c.synth_queue_high_water.store(0, std::memory_order_relaxed);
        c.synth_panics.store(0, std::memory_order_relaxed);
        c.synth_max_command_wait_us.store(0, std::memory_order_relaxed);
        c.peak_abs_sample.store(0, std::memory_order_relaxed);
        c.near_clip_samples.store(0, std::memory_order_relaxed);
        c.hard_clip_samples.store(0, std::memory_order_relaxed);
    }
    
private:
    Diagnostics() = default;
    DiagnosticCounters counters_;
};

} // namespace smk
