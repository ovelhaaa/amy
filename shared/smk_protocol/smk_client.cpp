#include "smk_client.h"
#include <cstring>

namespace smk::link {
bool Client::request(Type type,const uint8_t* payload,uint16_t size,uint32_t now_ms) {
    if(!connected_ || pending_ || !sequence_ || size>kPayloadBytes || (size && !payload)) return false;
    if((panic_pending_ || release_pending_!=kNoNote) && type!=Type::Panic && type!=Type::AuditionNoteOff) return false;
    Frame next{}; next.type=type; next.size=size;
    if(size) std::memcpy(next.payload,payload,size);
    if(type==Type::Hello || !validCommand(next)) return false;
    next.sequence=sequence_; next.session=session_;
    command_=next; pending_=true; sent_=false; retries_=0; last_send_ms_=now_ms;
    return true;
}
void Client::releaseNote(uint8_t note) {
    if(note>0 && note<128) release_pending_=note;
    held_=kNoNote;
}
void Client::disconnect() {
    connected_=false; pending_=false; sent_=false; response_ready_=false;
    held_=release_pending_=kNoNote; panic_pending_=false; sequence_=1;
    session_=session_*1664525u+1013904223u;
    if(!session_) session_=1;
}
bool Client::next(uint32_t now_ms,Frame& transmit) {
    if(connected_ && uint32_t(now_ms-last_receive_ms_)>=kLinkTimeoutMs) { ++errors_; disconnect(); }
    if(pending_) {
        if(!sent_) { transmit=command_; sent_=true; last_send_ms_=now_ms; return true; }
        if(uint32_t(now_ms-last_send_ms_)<250) return false;
        if(retries_++<3) { transmit=command_; last_send_ms_=now_ms; return true; }
        ++errors_; disconnect();
    }
    if(!connected_) {
        if(incompatible_ && uint32_t(now_ms-last_send_ms_)<2000) return false;
        command_={}; command_.type=Type::Hello; command_.session=session_;
        pending_=true; sent_=true; retries_=0; last_send_ms_=now_ms;
        transmit=command_; return true;
    }
    if(!sequence_ || sequence_==UINT16_MAX) { disconnect(); return false; } // Renew session before sequence wrap.
    if(panic_pending_) {
        panic_pending_=false;
        if(request(Type::Panic,nullptr,0,now_ms)) return next(now_ms,transmit);
    }
    if(release_pending_!=kNoNote) {
        const uint8_t note=release_pending_; release_pending_=kNoNote;
        if(request(Type::AuditionNoteOff,&note,1,now_ms)) return next(now_ms,transmit);
    }
    if(uint32_t(now_ms-last_state_ms_)>=200) {
        last_state_ms_=now_ms;
        if(request(Type::GetState,&held_,1,now_ms)) return next(now_ms,transmit);
    }
    return false;
}
void Client::receive(const Frame& r,uint32_t now_ms) {
    if(connected_ && r.type==Type::AudioDiagnostics && r.session==session_) {
        if(r.version!=kVersion || r.size!=kDiagnosticsBytes || r.sequence!=0) { ++errors_; return; }
        response_=r; response_ready_=true;
        return; // Telemetry never acknowledges a pending command or renews a note.
    }
    if(!pending_ || r.session!=session_ || r.sequence!=command_.sequence) return;
    if(r.version!=kVersion) { incompatible_=true; ++errors_; disconnect(); return; }
    if(r.type==Type::CommandError && r.size==4) {
        const auto reason=Error(r.payload[0]);
        if(reason==Error::Version) incompatible_=true;
        if(reason==Error::Version || reason==Error::Session || reason==Error::Sequence || reason==Error::Payload) {
            ++errors_; disconnect(); return;
        }
        if(reason!=Error::Busy && reason!=Error::Unsupported) { ++errors_; return; }
    } else if(command_.type==Type::Hello) {
        if(r.type!=Type::HelloAck || r.size!=8 || !get16(r.payload+4) || get16(r.payload+6)!=kPayloadBytes) { ++errors_; return; }
        boot_id_=get32(r.payload); sequence_=get16(r.payload+4);
        connected_=true; incompatible_=false;
    } else if(command_.type==Type::GetState) {
        if(r.type!=Type::StateSnapshot || r.size!=kSnapshotBytes) { ++errors_; return; }
    } else if(command_.type==Type::GetPatch) {
        if(r.type!=Type::PatchState || r.size!=28) { ++errors_; return; }
    } else if(r.type!=Type::CommandAck || r.size!=2 || r.payload[1]!=uint8_t(command_.type) ||
              (r.payload[0]!=uint8_t(Ack::Received) && r.payload[0]!=uint8_t(Ack::Applied))) { ++errors_; return; }
    if(command_.type!=Type::Hello) ++sequence_;
    last_receive_ms_=now_ms; pending_=false; sent_=false;
    response_=r; response_ready_=true;
}
bool Client::takeResponse(Frame& response) {
    if(!response_ready_) return false;
    response=response_; response_ready_=false; return true;
}
}
