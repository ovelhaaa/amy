#pragma once
#include "commands.h"
#include <cstddef>
namespace smk::p4::amy {
// USB client task is the sole producer. The AMY owner consumes this mailbox;
// the UI retains its own SPSC Commands. Never share a producer endpoint.
class UsbInput {
public:
    struct Trace { uint32_t timestamp_ms=0; uint8_t packet[4]{}; };
    void enableTrace() { trace_enabled_=true; } // Before starting the USB task.
    bool popTrace(Trace& t) { // Diagnostics/UI is the only reader.
        const auto r=trace_read_.load(std::memory_order_relaxed);
        if(r==trace_write_.load(std::memory_order_acquire)) return false;
        t=trace_[r%kCommandCapacity]; trace_read_.store(r+1,std::memory_order_release); return true;
    }
    Commands commands;
    bool packet(const uint8_t* p,uint32_t now_ms) {
        const unsigned cin=p[0]&15, status=p[1]>>4;
        if(cin>=8 && cin<=14 && (cin!=status || p[2]>=128 ||
            (cin!=12 && cin!=13 && p[3]>=128))) return false;
        if(cin==15 && p[1]<0xf8) return false;
        Command c{}; c.channel=p[1]&15; c.timestamp_ms=now_ms;
        if(cin==8 || cin==9) {
            c.type=cin==8 || !p[3]?CommandType::NoteOff:CommandType::NoteOn;
            c.id=p[2]; c.value=p[3];
        } else if(cin==11) {
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
        if((!data && size) || size%4) { ++parse_errors; commands.panic(); return false; }
        // Reject a malformed transfer before publishing any of its notes.
        for(size_t i=0;i<size;i+=4) {
            const auto* p=data+i; const unsigned cin=p[0]&15;
            if((p[0]>>4)!=0 || (cin>=8 && cin<=14 &&
                ((p[1]>>4)!=cin || p[2]>=128 || (cin!=12 && cin!=13 && p[3]>=128))) ||
                (cin==15 && p[1]<0xf8)) {
                ++parse_errors; commands.panic(); return false;
            }
        }
        for(size_t i=0;i<size;i+=4) {
            if(trace_enabled_) {
                const auto w=trace_write_.load(std::memory_order_relaxed);
                if(w-trace_read_.load(std::memory_order_acquire)<kCommandCapacity) {
                    auto& t=trace_[w%kCommandCapacity]; t.timestamp_ms=now_ms;
                    for(unsigned j=0;j<4;++j) t.packet[j]=data[i+j];
                    trace_write_.store(w+1,std::memory_order_release);
                } else ++trace_dropped;
            }
            packet(data+i,now_ms);
        }
        return true;
    }
    void disconnect() { commands.panic(); }
    std::atomic<uint32_t> events{0},dropped{0},parse_errors{0};
    std::atomic<uint32_t> trace_dropped{0};
private:
    bool trace_enabled_=false;
    Trace trace_[kCommandCapacity]{};
    std::atomic<uint32_t> trace_read_{0},trace_write_{0};
};
}
