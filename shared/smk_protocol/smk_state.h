#pragma once
#include "smk_protocol.h"
#include <cstring>

namespace smk::link {
constexpr uint8_t kUsbConnected=1,kAudioLive=2,kControlState=0x80;
struct Macro { char name[9]{}; uint8_t value=0; bool supported=false; };
struct State {
    uint16_t patch_id=0,engine_patch=0,bpm_centi=0,block_frames=0;
    char patch_name[25]{};
    uint8_t master=0,flags=kControlState;
    uint32_t sample_rate_hz=0,max_render_us=0,deadline_misses=0,pcm_starvations=0;
    uint32_t event_overflows=0,commands_dropped=0,free_internal=0,free_psram=0,active_notes=0,midi_events=0;
    Macro macros[8]{};
};
inline void writeState(Frame& f,const State& s) {
    f.type=Type::StateSnapshot; f.size=kSnapshotBytes;
    put16(f.payload,s.patch_id); put16(f.payload+2,s.engine_patch);
    std::memcpy(f.payload+4,s.patch_name,24); f.payload[28]=s.master; f.payload[29]=s.flags;
    put16(f.payload+30,s.bpm_centi); put32(f.payload+32,s.sample_rate_hz); put16(f.payload+36,s.block_frames);
    const uint32_t values[]={s.max_render_us,s.deadline_misses,s.pcm_starvations,s.event_overflows,
        s.commands_dropped,s.free_internal,s.free_psram,s.active_notes,s.midi_events};
    for(unsigned i=0;i<9;++i) put32(f.payload+38+i*4,values[i]);
    for(unsigned i=0;i<8;++i) {
        std::memcpy(f.payload+74+i*10,s.macros[i].name,8);
        f.payload[82+i*10]=s.macros[i].value; f.payload[83+i*10]=s.macros[i].supported?1:0;
    }
}
inline bool readState(const Frame& f,State& s) {
    if(f.type!=Type::StateSnapshot || f.size!=kSnapshotBytes || f.payload[28]>127) return false;
    for(unsigned i=0;i<8;++i) if(f.payload[82+i*10]>127 || f.payload[83+i*10]>1) return false;
    s={}; s.patch_id=get16(f.payload); s.engine_patch=get16(f.payload+2);
    std::memcpy(s.patch_name,f.payload+4,24); s.master=f.payload[28]; s.flags=f.payload[29];
    s.bpm_centi=get16(f.payload+30); s.sample_rate_hz=get32(f.payload+32); s.block_frames=get16(f.payload+36);
    uint32_t* const values[]={&s.max_render_us,&s.deadline_misses,&s.pcm_starvations,&s.event_overflows,
        &s.commands_dropped,&s.free_internal,&s.free_psram,&s.active_notes,&s.midi_events};
    for(unsigned i=0;i<9;++i) *values[i]=get32(f.payload+38+i*4);
    for(unsigned i=0;i<8;++i) {
        std::memcpy(s.macros[i].name,f.payload+74+i*10,8);
        s.macros[i].value=f.payload[82+i*10]; s.macros[i].supported=f.payload[83+i*10]!=0;
    }
    return true;
}
}
