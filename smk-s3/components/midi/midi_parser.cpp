#include "midi_parser.h"
#include "esp_timer.h"

namespace smk {

MidiParser::MidiParser() {
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
                emitEvent(EventType::NoteOn, channel, data1, data2);
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
        default:
            // Other CINs not currently handled
            break;
    }
}

void MidiParser::processByte(uint8_t byte) {
    // Handle running status for future serial MIDI
    if (byte & 0x80) {
        if (byte >= 0xF8) {
            // Realtime message
            if (byte == 0xFF) emitEvent(EventType::Panic, 0, 0, 0);
            return;
        }
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
                        emitEvent(EventType::NoteOn, channel, data_bytes_[0], data_bytes_[1]);
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
