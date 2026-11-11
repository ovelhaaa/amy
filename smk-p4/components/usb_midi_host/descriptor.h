#pragma once
#include "usb_midi_descriptor.h"
#include <cstring>
namespace smk::p4::usb {
struct Endpoint : smk::MidiEndpoint { uint8_t alternate=0; };
// Reuse S3 class/endpoint validation on a bounded view of each interface +
// endpoint. P4 additionally supports nonzero alternates and HS 512-byte MPS.
// The original SDK-owned descriptor is never modified.
inline bool findEndpoint(const uint8_t* data,size_t size,Endpoint& out) {
    if(!data || size<9 || data[0]<9 || data[1]!=2) return false;
    const size_t total=data[2]|(size_t(data[3])<<8);
    if(total<data[0] || total>size) return false;
    uint8_t view[]={9,2,25,0,1,1,0,0x80,50, 9,4,0,0,0,0,0,0,0, 7,5,0,0,0,0,0};
    uint8_t alt=0; bool have_interface=false,found=false; Endpoint selected;
    for(size_t offset=0;offset<total;) {
        if(total-offset<2 || data[offset]<2 || data[offset]>total-offset) return false;
        const auto* p=data+offset; const size_t length=p[0];
        if(p[1]==4) {
            if(length<9) return false;
            std::memcpy(view+9,p,9); view[9]=9;
            alt=p[3]; view[12]=0; have_interface=true;
        } else if(p[1]==5) {
            if(length<7) return false;
            if(have_interface) {
                std::memcpy(view+18,p,7); view[18]=7;
                const uint16_t mps=p[4]|(uint16_t(p[5])<<8);
                if(mps==512) { view[22]=64; view[23]=0; }
                smk::MidiEndpoint ep;
                if(smk::findMidiEndpoint(view,sizeof(view),ep) &&
                   (!found || (selected.alternate && !alt))) {
                    static_cast<smk::MidiEndpoint&>(selected)=ep;
                    selected.packet_bytes=mps; selected.alternate=alt; found=true;
                }
            }
        }
        offset+=length;
    }
    if(found) out=selected;
    return found;
}
}
