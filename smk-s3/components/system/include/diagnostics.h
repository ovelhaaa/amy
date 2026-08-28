#pragma once

#include <atomic>
#include <cstdint>

namespace smk {

struct DiagnosticCounters {
    // Audio
    std::atomic<uint32_t> audio_underruns{0};
    std::atomic<uint32_t> max_render_us{0};
    std::atomic<uint32_t> avg_render_us{0};
    std::atomic<uint32_t> frames_rendered{0};
    
    // MIDI
    std::atomic<uint32_t> midi_parse_errors{0};
    std::atomic<uint32_t> events_received{0};
    std::atomic<uint32_t> events_dropped{0};
    std::atomic<uint32_t> event_queue_overflows{0};
    
    // USB
    std::atomic<uint32_t> usb_disconnects{0};
    std::atomic<uint32_t> usb_reconnects{0};
    
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
        float render_load;  // 0.0 - 1.0
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
    
private:
    Diagnostics() = default;
    DiagnosticCounters counters_;
};

} // namespace smk
