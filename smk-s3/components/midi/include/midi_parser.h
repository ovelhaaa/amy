#pragma once
#include <cstdint>
#include <atomic>
#include "synth_event.h"

namespace smk {

enum class VelocityCurve : uint8_t {
    Linear = 0,
    Soft,
    Hard,
    SCurve,
    Fixed
};

class MidiParser {
public:
    using EventCallback = void(*)(const SynthEvent& event, void* ctx);
    
    MidiParser();
    
    void setCallback(EventCallback cb, void* ctx);
    void setSource(EventSource source);
    void setChannel(uint8_t default_channel);  // for running status
    
    void setVelocityCurve(VelocityCurve curve);
    VelocityCurve velocityCurve() const;
    static uint8_t applyVelocityCurve(uint8_t raw_velocity, VelocityCurve curve);
    static const char* velocityCurveName(VelocityCurve curve);
    
    // Process raw USB-MIDI event packet (4 bytes: CIN+cable, status, data1, data2)
    void processUsbMidiPacket(const uint8_t* packet);
    
    // Process raw MIDI bytes (for future serial MIDI)
    void processByte(uint8_t byte);
    
    // Diagnostics
    uint32_t parseErrorCount() const;
    
private:
    void emitEvent(EventType type, uint8_t channel, uint16_t id, int32_t value);
    
    EventCallback callback_ = nullptr;
    void* callback_ctx_ = nullptr;
    EventSource source_ = EventSource::UsbMidi;
    
    // Running status parser state
    uint8_t running_status_ = 0;
    uint8_t data_bytes_[2];
    uint8_t data_index_ = 0;
    uint8_t expected_bytes_ = 0;
    
    std::atomic<uint32_t> parse_errors_{0};
    std::atomic<VelocityCurve> velocity_curve_{VelocityCurve::Linear};
    int32_t smoothed_pitch_bend_ = 0;
};

} // namespace smk
