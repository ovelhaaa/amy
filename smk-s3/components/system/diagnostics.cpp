#include "diagnostics.h"
#include <algorithm>
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
    
    // Audio block budget at 48kHz with 256 frames: (256 * 1000000) / 48000 = ~5333.33 us
    constexpr float kBlockBudgetUs = 5333.33f;
    s.render_load = std::clamp(((float)s.avg_render_us / kBlockBudgetUs) * 100.0f, 0.0f, 100.0f);
    s.active_voices = counters_.active_voices.load();
    
    s.midi_parse_errors = counters_.midi_parse_errors.load();
    s.events_received = counters_.events_received.load();
    s.event_queue_overflows = counters_.event_queue_overflows.load();
    s.usb_disconnects = counters_.usb_disconnects.load();
    s.usb_reconnects = counters_.usb_reconnects.load();
    s.panic_count = counters_.panic_count.load();
    s.usb_connected = counters_.usb_connected.load(std::memory_order_relaxed);
    s.firmware_version = "0.1.0"; 
    
    return s;
}

void Diagnostics::logSnapshot() const {
    Snapshot s = takeSnapshot();
    ESP_LOGI(TAG, "=== System Diagnostics Snapshot ===");
    ESP_LOGI(TAG, "Audio: Underruns=%lu, MaxRenderUs=%lu, AvgRenderUs=%lu, FramesRendered=%lu",
             s.audio_underruns, s.max_render_us, s.avg_render_us, s.frames_rendered);
    ESP_LOGI(TAG, "Memory: FreeInternal=%lu, FreePSRAM=%lu, MaxFreeInternalBlock=%lu, MaxFreePSRAMBlock=%lu",
             s.free_internal_ram, s.free_psram, s.largest_free_internal_block, s.largest_free_psram_block);
    ESP_LOGI(TAG, "Hardware: CPUFreq=%luMHz, FlashSize=%lu, PSRAMSize=%lu",
             s.cpu_freq_mhz, s.flash_size, s.psram_size);
    ESP_LOGI(TAG, "MIDI/USB: EventsReceived=%lu, ParseErrors=%lu, QueueOverflows=%lu, Disconnects=%lu, Reconnects=%lu",
             s.events_received, s.midi_parse_errors, s.event_queue_overflows, s.usb_disconnects, s.usb_reconnects);
    ESP_LOGI(TAG, "System: Panics=%lu, Version=%s", s.panic_count, s.firmware_version);
}

} // namespace smk
