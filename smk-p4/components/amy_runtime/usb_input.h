#pragma once
#include "commands.h"
#include "midi_control_events.h"
#include <cstddef>
namespace smk::p4::amy {
// USB client task is the sole producer. The AMY owner consumes this mailbox;
// the UI retains its own SPSC Commands. Never share a producer endpoint.
class UsbInput {
public:
    struct Trace { uint32_t timestamp_ms=0; uint8_t packet[4]{}; uint8_t valid_bytes=0; };
    void enableTrace() { trace_enabled_=true; } // Before starting the USB task.
    void enableControls() { controls_enabled_=true; } // Before starting the USB task.
    bool popControl(MidiControlEvent& event) { return controls_.pop(event); }
    uint32_t controlDrops() const { return controls_.dropped(); }
    uint32_t deviceGeneration() const { return device_generation_.load(std::memory_order_acquire); }
    bool currentControl(const MidiControlEvent& event) const {
        return event.device_generation==deviceGeneration() &&
            event.command_generation==commands.generation();
    }
    bool popTrace(Trace& t) { // Diagnostics/UI is the only reader.
        const auto r=trace_read_.load(std::memory_order_relaxed);
        if(r==trace_write_.load(std::memory_order_acquire)) return false;
        t=trace_[r%kCommandCapacity]; trace_read_.store(r+1,std::memory_order_release); return true;
    }
    CommandQueue<kUsbCommandCapacity> commands;
    bool packet(const uint8_t* p,uint32_t now_ms) {
        if(p[0]>>4) return true; // This application routes only virtual cable 0.
        const unsigned cin=p[0]&15, status=p[1]>>4;
        if(cin>=8 && cin<=14 && (cin!=status || p[2]>=128 ||
            (cin!=12 && cin!=13 && p[3]>=128))) return false;
        if(cin==15 && p[1]<0xf8) return false;
        Command c{}; c.channel=p[1]&15; c.timestamp_ms=now_ms;
        if(cin==8 || cin==9) {
            c.type=cin==8 || !p[3]?CommandType::NoteOff:CommandType::NoteOn;
            c.id=p[2]; c.value=p[3];
        } else if(cin==11) {
            if(controls_enabled_ && observableController(p[2]))
                controls_.push({now_ms,deviceGeneration(),commands.generation(),0,c.channel,p[2],p[3]});
            if(!musicalController(p[2])) { ++events; return true; }
            c.type=CommandType::Control; c.id=p[2]; c.value=p[3];
        } else if(cin==14) {
            c.type=CommandType::Bend; c.id=uint16_t(p[2])|(uint16_t(p[3])<<7);
        } else if(cin==15 && p[1]==0xff) { commands.panic(); return true; }
        else return true; // Unsupported pressure/program/SysEx/clock: no routing.
        // On a lost release/pedal edge invalidate the USB source, including
        // older queued gates, without affecting Touch. Continuous controls
        // and new gates may be dropped safely without canceling held notes.
        if(!commands.push(c)) {
            if(c.type==CommandType::NoteOff || (c.type==CommandType::NoteOn && !c.value) ||
               (c.type==CommandType::Control && (c.id==64 || c.id==120 || c.id==123))) commands.panic();
            ++dropped;
        }
        ++events; return true;
    }
    bool transfer(const uint8_t* data,size_t size,uint32_t now_ms) {
        if(!data && size) { ++parse_errors; commands.panic(); return false; }
        // The Host bounds transfers to its allocated buffer (at most 512 B).
        // Capture before filtering, including a trailing partial packet, so
        // rejected real traffic is visible without publishing any of its notes.
        if(trace_enabled_) for(size_t i=0;i<size;i+=4) {
            const auto w=trace_write_.load(std::memory_order_relaxed);
            if(w-trace_read_.load(std::memory_order_acquire)<kCommandCapacity) {
                auto& t=trace_[w%kCommandCapacity]; t.timestamp_ms=now_ms;
                t.valid_bytes=static_cast<uint8_t>(size-i<4?size-i:4);
                for(unsigned j=0;j<4;++j) t.packet[j]=j<t.valid_bytes?data[i+j]:0;
                trace_write_.store(w+1,std::memory_order_release);
            } else ++trace_dropped;
        }
        if(size%4) { ++parse_errors; commands.panic(); return false; }
        // Reject a malformed transfer before publishing any of its notes.
        for(size_t i=0;i<size;i+=4) {
            const auto* p=data+i; const unsigned cin=p[0]&15;
            // Valid multiport devices may duplicate keyboard traffic on other
            // virtual cables. Keep their identities separate: trace/count and
            // ignore unselected cables, without canceling cable 0 or reopening
            // the device. Never merge them into the musical voice ownership.
            if(p[0]>>4) { ++ignored_cable_packets; continue; }
            if((cin>=8 && cin<=14 &&
                ((p[1]>>4)!=cin || p[2]>=128 || (cin!=12 && cin!=13 && p[3]>=128))) ||
                (cin==15 && p[1]<0xf8)) {
                ++parse_errors; commands.panic(); return false;
            }
        }
        for(size_t i=0;i<size;i+=4) {
            packet(data+i,now_ms);
        }
        return true;
    }
    void disconnect() {
        device_generation_.fetch_add(1,std::memory_order_acq_rel);
        commands.panic();
    }
    std::atomic<uint32_t> events{0},dropped{0},parse_errors{0},ignored_cable_packets{0};
    std::atomic<uint32_t> trace_dropped{0};
private:
    bool trace_enabled_=false;
    bool controls_enabled_=false;
    MidiControlEvents controls_;
    std::atomic<uint32_t> device_generation_{1};
    Trace trace_[kCommandCapacity]{};
    std::atomic<uint32_t> trace_read_{0},trace_write_{0};
};
}
