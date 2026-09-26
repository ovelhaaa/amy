#include "diagnostics.h"
#include "audio_config.h"
#include "firmware_info.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_flash.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "esp_private/esp_clk.h"

static const char* TAG = "Diagnostics";

namespace smk {

Diagnostics& Diagnostics::instance() {
    static Diagnostics instance;
    return instance;
}

DiagnosticCounters& Diagnostics::counters() {
    return counters_;
}

Diagnostics::Snapshot Diagnostics::takeSnapshot() const {
    Snapshot s = {};
    s.audio_underruns = counters_.audio_underruns.load();
    s.max_render_us = counters_.max_render_us.load();
    s.avg_render_us = counters_.avg_render_us.load();
    s.frames_rendered = counters_.frames_rendered.load();
    
    s.free_internal_ram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    s.free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    s.largest_free_internal_block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    s.largest_free_psram_block = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    s.cpu_freq_mhz = esp_clk_cpu_freq() / 1000000;
    
    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);
    s.flash_size = flash_size;
    // Real PSRAM size from ESP-IDF; 0 when the part is absent or not initialized.
    s.psram_size = esp_psram_get_size();

    // Audio block budget is derived from the active build, never hard-coded.
    // This keeps the load percentage comparable across block sizes. The load is
    // deliberately not clamped: a value above 100% is a real deadline overrun.
    s.block_size = config::kBlockSize;
    s.sample_rate_hz = config::kSampleRateHz;
    s.block_budget_us = audioBlockBudgetUs(config::kBlockSize, config::kSampleRateHz);
    s.render_load =
        audioRenderLoadPercent(s.avg_render_us, config::kBlockSize, config::kSampleRateHz);
    s.max_render_load =
        audioRenderLoadPercent(s.max_render_us, config::kBlockSize, config::kSampleRateHz);
    s.peak_abs_sample = counters_.peak_abs_sample.load();
    s.near_clip_samples = counters_.near_clip_samples.load();
    s.hard_clip_samples = counters_.hard_clip_samples.load();
    s.active_voices = counters_.active_voices.load();
    
    s.midi_parse_errors = counters_.midi_parse_errors.load();
    s.events_received = counters_.events_received.load();
    s.event_queue_overflows = counters_.event_queue_overflows.load();
    s.usb_disconnects = counters_.usb_disconnects.load();
    s.usb_reconnects = counters_.usb_reconnects.load();
    s.panic_count = counters_.panic_count.load();
    s.usb_connected = counters_.usb_connected.load(std::memory_order_relaxed);
    s.firmware_version = config::kFirmwareVersion;
    
    return s;
}

void Diagnostics::logSnapshot() const {
    Snapshot s = takeSnapshot();
    ESP_LOGI(TAG, "=== System Diagnostics Snapshot ===");
    ESP_LOGI(TAG, "Audio: Block=%lu, Rate=%luHz, Budget=%.1fus, AvgRender=%luus (%.1f%%), MaxRender=%luus (%.1f%%)",
             s.block_size, s.sample_rate_hz, s.block_budget_us,
             s.avg_render_us, s.render_load, s.max_render_us, s.max_render_load);
    ESP_LOGI(TAG, "Audio: Underruns=%lu, PCMStarvations=%lu, FramesRendered=%lu, Voices=%lu",
             s.audio_underruns, counters_.synth_pcm_starvations.load(), s.frames_rendered, s.active_voices);
    ESP_LOGI(TAG, "Headroom: PeakAbs=%lu, NearClip=%lu, HardClip=%lu",
             s.peak_abs_sample, s.near_clip_samples, s.hard_clip_samples);
    ESP_LOGI(TAG, "Memory: FreeInternal=%lu, FreePSRAM=%lu, MaxFreeInternalBlock=%lu, MaxFreePSRAMBlock=%lu",
             s.free_internal_ram, s.free_psram, s.largest_free_internal_block, s.largest_free_psram_block);
    ESP_LOGI(TAG, "Hardware: CPUFreq=%luMHz, FlashSize=%lu, PSRAMSize=%lu",
             s.cpu_freq_mhz, s.flash_size, s.psram_size);
    ESP_LOGI(TAG, "MIDI/USB: EventsReceived=%lu, ParseErrors=%lu, QueueOverflows=%lu, Disconnects=%lu, Reconnects=%lu",
             s.events_received, s.midi_parse_errors, s.event_queue_overflows, s.usb_disconnects, s.usb_reconnects);
    ESP_LOGI(TAG, "System: Panics=%lu, Version=%s", s.panic_count, s.firmware_version);
}

} // namespace smk
