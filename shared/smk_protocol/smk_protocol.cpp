#include "smk_protocol.h"
#include <cstring>

namespace smk::link {
uint16_t crc16(const uint8_t* data, size_t size) {
    uint16_t crc=0xffff;
    for(size_t i=0;i<size;++i) {
        crc ^= uint16_t(data[i])<<8;
        for(unsigned bit=0;bit<8;++bit)
            crc = uint16_t((crc<<1) ^ ((crc & 0x8000) ? 0x1021 : 0));
    }
    return crc;
}
size_t encode(const Frame& frame,uint8_t* wire,size_t capacity) {
    if(!wire || frame.size>kPayloadBytes) return 0;
    uint8_t bytes[kPacketBytes]{};
    bytes[0]='S'; bytes[1]='M'; bytes[2]=frame.version; bytes[3]=uint8_t(frame.type);
    put16(bytes+4,frame.sequence); put32(bytes+6,frame.session); put16(bytes+10,frame.size);
    std::memcpy(bytes+kHeaderBytes,frame.payload,frame.size);
    const size_t body=kHeaderBytes+frame.size;
    put16(bytes+body,crc16(bytes,body));
    size_t required=2;
    for(size_t i=0;i<body+2;++i) required += (bytes[i]==0xc0 || bytes[i]==0xdb) ? 2 : 1;
    if(required>capacity) return 0;
    size_t used=0; wire[used++]=0xc0;
    for(size_t i=0;i<body+2;++i) {
        if(bytes[i]==0xc0 || bytes[i]==0xdb) { wire[used++]=0xdb; wire[used++]=bytes[i]==0xc0?0xdc:0xdd; }
        else wire[used++]=bytes[i];
    }
    wire[used++]=0xc0;
    return used;
}
bool sameRequest(const Frame& a,const Frame& b) {
    return a.version==b.version && a.type==b.type && a.sequence==b.sequence &&
           a.session==b.session && a.size==b.size && a.size<=kPayloadBytes &&
           std::memcmp(a.payload,b.payload,a.size)==0;
}
bool validCommand(const Frame& f) {
    switch(f.type) {
        case Type::Hello: case Type::Panic: return f.size==0;
        case Type::GetState: return f.size==1 && (f.payload[0]==kNoNote || (f.payload[0]>0 && f.payload[0]<128));
        case Type::SetPatch: case Type::GetPatch: return f.size==2 && get16(f.payload)<256;
        case Type::SetMacro: return f.size==2 && f.payload[0]<8 && f.payload[1]<128;
        case Type::SetMasterVolume: return f.size==1 && f.payload[0]<128;
        case Type::AuditionNoteOn: return f.size==2 && f.payload[0]>0 && f.payload[0]<128 && f.payload[1]>0 && f.payload[1]<128;
        case Type::AuditionNoteOff: return f.size==1 && f.payload[0]>0 && f.payload[0]<128;
        default: return false;
    }
}
void Decoder::reset() { size_=0; started_=escaped_=dropping_=false; }
bool Decoder::expire(uint32_t now_ms) {
    if(started_ && (size_ || escaped_ || dropping_) && uint32_t(now_ms-last_ms_)>=kFrameTimeoutMs) {
        reset(); ++timeouts_; ++errors_; return true;
    }
    return false;
}
Decoder::Result Decoder::feed(uint8_t byte,uint32_t now_ms,Frame& out) {
    expire(now_ms);
    if(byte==0xc0) {
        const size_t size=size_;
        const bool bad=dropping_ || escaped_;
        size_=0; escaped_=dropping_=false; started_=true; last_ms_=now_ms;
        if(!size && !bad) return Result::None;
        if(bad || size<kHeaderBytes+2 || bytes_[0]!='S' || bytes_[1]!='M') { ++errors_; return Result::Invalid; }
        const uint16_t payload_size=get16(bytes_+10);
        if(payload_size>kPayloadBytes || size!=kHeaderBytes+payload_size+2 ||
           get16(bytes_+size-2)!=crc16(bytes_,size-2)) { ++errors_; return Result::Invalid; }
        out={}; out.version=bytes_[2]; out.type=Type(bytes_[3]); out.sequence=get16(bytes_+4);
        out.session=get32(bytes_+6); out.size=payload_size;
        std::memcpy(out.payload,bytes_+kHeaderBytes,payload_size);
        return Result::Packet;
    }
    if(!started_) return Result::None;
    last_ms_=now_ms;
    if(dropping_) return Result::None;
    if(escaped_) {
        escaped_=false;
        if(byte==0xdc) byte=0xc0;
        else if(byte==0xdd) byte=0xdb;
        else { dropping_=true; return Result::None; }
    } else if(byte==0xdb) { escaped_=true; return Result::None; }
    if(size_==sizeof(bytes_)) { dropping_=true; return Result::None; }
    bytes_[size_++]=byte;
    return Result::None;
}
}
