#include "midi_parser.h"
#include "esp_timer.h"
#include <cmath>
#include <algorithm>

namespace smk {

static uint8_t s_vel_lut[5][128];
static bool s_vel_lut_init = false;

static void initVelocityLuts() {
    if (s_vel_lut_init) return;
    
    for (int i = 0; i < 128; ++i) {
        float norm = (float)i / 127.0f;
        
        // 0. Linear
        s_vel_lut[0][i] = (uint8_t)i;
        
        // 1. Soft (Exponential, power 1.6)
        if (i == 0) s_vel_lut[1][i] = 0;
        else {
            int val = (int)std::round(127.0f * std::pow(norm, 1.6f));
            s_vel_lut[1][i] = (uint8_t)std::clamp(val, 1, 127);
        }
        
        // 2. Hard (Logarithmic, power 0.6)
        if (i == 0) s_vel_lut[2][i] = 0;
        else {
            int val = (int)std::round(127.0f * std::pow(norm, 0.6f));
            s_vel_lut[2][i] = (uint8_t)std::clamp(val, 1, 127);
        }
        
        // 3. SCurve (Sigmoid)
        if (i == 0) s_vel_lut[3][i] = 0;
        else {
            float s_min = 1.0f / (1.0f + std::exp(4.0f));
            float s_max = 1.0f / (1.0f + std::exp(-4.0f));
            float s_val = 1.0f / (1.0f + std::exp(-8.0f * (norm - 0.5f)));
            float s_norm = (s_val - s_min) / (s_max - s_min);
            int val = (int)std::round(127.0f * std::clamp(s_norm, 0.0f, 1.0f));
            s_vel_lut[3][i] = (uint8_t)std::clamp(val, 1, 127);
        }
        
        // 4. Fixed (127 for any hit)
        s_vel_lut[4][i] = (i > 0) ? 127 : 0;
    }
    s_vel_lut_init = true;
}

MidiParser::MidiParser() {
    initVelocityLuts();
}

void MidiParser::setVelocityCurve(VelocityCurve curve) {
    velocity_curve_.store(curve, std::memory_order_relaxed);
}

VelocityCurve MidiParser::velocityCurve() const {
    return velocity_curve_.load(std::memory_order_relaxed);
}

uint8_t MidiParser::applyVelocityCurve(uint8_t raw_velocity, VelocityCurve curve) {
    initVelocityLuts();
    raw_velocity &= 0x7F;
    uint8_t c_idx = static_cast<uint8_t>(curve);
    if (c_idx > 4) c_idx = 0;
    return s_vel_lut[c_idx][raw_velocity];
}

const char* MidiParser::velocityCurveName(VelocityCurve curve) {
    switch (curve) {
        case VelocityCurve::Linear: return "LINEAR";
        case VelocityCurve::Soft:   return "SOFT";
        case VelocityCurve::Hard:   return "HARD";
        case VelocityCurve::SCurve: return "S-CURVE";
        case VelocityCurve::Fixed:  return "FIXED";
        default:                    return "LINEAR";
    }
}

void MidiParser::setCallback(EventCallback cb, void* ctx) {
    callback_ = cb;
    callback_ctx_ = ctx;
}

void MidiParser::setSource(EventSource source) {
    source_ = source;
}

void MidiParser::setChannel(uint8_t default_channel) {
    // Setting channel for future use with running status
}

void MidiParser::processUsbMidiPacket(const uint8_t* packet) {
    uint8_t cin = packet[0] & 0x0F;
    if (cin == 0x0) return; // Skip CIN=0

    uint8_t status = packet[1];
    uint8_t data1 = packet[2];
    uint8_t data2 = packet[3];

    uint8_t channel = status & 0x0F;

    switch (cin) {
        case 0x8: // Note Off
            emitEvent(EventType::NoteOff, channel, data1, data2);
            break;
        case 0x9: // Note On
            if (data2 == 0) {
                emitEvent(EventType::NoteOff, channel, data1, 0);
            } else {
                uint8_t curved_vel = applyVelocityCurve(data2, velocity_curve_.load(std::memory_order_relaxed));
                emitEvent(EventType::NoteOn, channel, data1, curved_vel);
            }
            break;
        case 0xB: // Control Change
            emitEvent(EventType::ControlChange, channel, data1, data2);
            if (data1 == 1) { // Modulation
                emitEvent(EventType::Modulation, channel, data1, data2);
            } else if (data1 == 120) { // All Sound Off
                emitEvent(EventType::AllNotesOff, channel, 0, 0);
            } else if (data1 == 123) { // All Notes Off
                emitEvent(EventType::AllNotesOff, channel, 0, 0);
            }
            break;
        case 0xE: // Pitch Bend
            {
                int32_t bend = ((int32_t)data2 << 7) | data1;
                emitEvent(EventType::PitchBend, channel, 0, bend - 8192);
            }
            break;
        case 0xC: // Program Change
            emitEvent(EventType::ProgramChange, channel, data1, 0);
            break;
        case 0xF: // Single Byte / Realtime
            if (status == 0xF8) {
                emitEvent(EventType::Clock, 0, 0, 0);
            } else if (status == 0xFA || status == 0xFB) {
                emitEvent(EventType::TransportPlay, 0, 0, 0);
            } else if (status == 0xFC) {
                emitEvent(EventType::TransportStop, 0, 0, 0);
            } else if (status == 0xFF) {
                emitEvent(EventType::Panic, 0, 0, 0);
            }
            break;
        default:
            // Other CINs not currently handled
            break;
    }
}

void MidiParser::processByte(uint8_t byte) {
    // Handle realtime messages immediately regardless of running status
    if (byte >= 0xF8) {
        if (byte == 0xF8) {
            emitEvent(EventType::Clock, 0, 0, 0);
        } else if (byte == 0xFA || byte == 0xFB) {
            emitEvent(EventType::TransportPlay, 0, 0, 0);
        } else if (byte == 0xFC) {
            emitEvent(EventType::TransportStop, 0, 0, 0);
        } else if (byte == 0xFF) {
            emitEvent(EventType::Panic, 0, 0, 0);
        }
        return;
    }

    if (byte & 0x80) {
        if (byte >= 0xF0) {
            // System common, ignored for now
            return;
        }
        
        running_status_ = byte;
        data_index_ = 0;
        
        uint8_t type = (byte & 0xF0) >> 4;
        if (type == 0xC || type == 0xD) {
            expected_bytes_ = 1;
        } else {
            expected_bytes_ = 2;
        }
    } else {
        if (running_status_ == 0) {
            parse_errors_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        
        data_bytes_[data_index_++] = byte;
        
        if (data_index_ >= expected_bytes_) {
            uint8_t channel = running_status_ & 0x0F;
            uint8_t type = (running_status_ & 0xF0) >> 4;
            
            switch (type) {
                case 0x8: // Note Off
                    emitEvent(EventType::NoteOff, channel, data_bytes_[0], data_bytes_[1]);
                    break;
                case 0x9: // Note On
                    if (data_bytes_[1] == 0) {
                        emitEvent(EventType::NoteOff, channel, data_bytes_[0], 0);
                    } else {
                        uint8_t curved_vel = applyVelocityCurve(data_bytes_[1], velocity_curve_.load(std::memory_order_relaxed));
                        emitEvent(EventType::NoteOn, channel, data_bytes_[0], curved_vel);
                    }
                    break;
                case 0xB: // CC
                    emitEvent(EventType::ControlChange, channel, data_bytes_[0], data_bytes_[1]);
                    if (data_bytes_[0] == 1) {
                        emitEvent(EventType::Modulation, channel, data_bytes_[0], data_bytes_[1]);
                    } else if (data_bytes_[0] == 120 || data_bytes_[0] == 123) {
                        emitEvent(EventType::AllNotesOff, channel, 0, 0);
                    }
                    break;
                case 0xE: // Pitch Bend
                    {
                        int32_t bend = ((int32_t)data_bytes_[1] << 7) | data_bytes_[0];
                        emitEvent(EventType::PitchBend, channel, 0, bend - 8192);
                    }
                    break;
                case 0xC: // Program Change
                    emitEvent(EventType::ProgramChange, channel, data_bytes_[0], 0);
                    break;
            }
            
            data_index_ = 0;
        }
    }
}

void MidiParser::emitEvent(EventType type, uint8_t channel, uint16_t id, int32_t value) {
    if (!callback_) return;
    
    SynthEvent ev;
    ev.type = type;
    ev.source = source_;
    ev.channel = channel;
    ev.id = id;
    ev.value = value;
    ev.timestamp_us = (uint32_t)esp_timer_get_time();
    
    callback_(ev, callback_ctx_);
}

uint32_t MidiParser::parseErrorCount() const {
    return parse_errors_.load(std::memory_order_relaxed);
}

} // namespace smk
