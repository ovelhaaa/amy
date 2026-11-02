#include "smk_link.h"

namespace smk::link {
Frame Server::error(const Frame& request,Error reason) {
    ++rejected_;
    Frame response{}; response.type=Type::CommandError;
    response.session=request.session; response.sequence=request.sequence;
    response.size=4; response.payload[0]=uint8_t(reason); response.payload[1]=uint8_t(request.type);
    put16(response.payload+2,uint16_t(sequence_+1));
    return response;
}
void Server::release() {
    if(note_!=kNoNote) backend_.releaseAudition();
    note_=kNoNote;
}
void Server::disconnect() {
    release();
    if(connected_) ++disconnects_;
    connected_=false; have_request_=false;
}
void Server::tick(uint32_t now_ms) {
    if(note_!=kNoNote && uint32_t(now_ms-note_refreshed_ms_)>=kNoteLeaseMs) release();
    if(connected_ && uint32_t(now_ms-last_receive_ms_)>=kLinkTimeoutMs) disconnect();
}
Frame Server::handle(const Frame& request,uint32_t now_ms) {
    tick(now_ms);
    if(request.version!=kVersion) return error(request,Error::Version);
    if(!request.session) return error(request,Error::Session);
    if(!validCommand(request)) return error(request,Error::Payload);
    if(request.type==Type::Hello) {
        if(request.sequence) return error(request,Error::Sequence);
        // HELLO retries in the same session must not reset musical state.
        if(!connected_ || request.session!=session_) {
            release(); session_=request.session; sequence_=0; have_request_=false;
            connected_=true; ++reconnects_;
        }
        last_receive_ms_=now_ms;
        Frame response{}; response.type=Type::HelloAck; response.session=session_; response.size=8;
        put32(response.payload,boot_id_); put16(response.payload+4,uint16_t(sequence_+1));
        put16(response.payload+6,uint16_t(kPayloadBytes));
        return response;
    }
    if(!connected_ || request.session!=session_) return error(request,Error::Session);
    if(have_request_ && request.sequence==sequence_) {
        if(!sameRequest(request,last_request_)) return error(request,Error::Sequence);
        // Retry response, never repeat a note, patch or Panic. Duplicate note
        // messages do not renew the audition lease.
        last_receive_ms_=now_ms;
        return last_response_;
    }
    if(!request.sequence || sequence_==UINT16_MAX || request.sequence!=uint16_t(sequence_+1))
        return error(request,Error::Sequence);

    Frame response{}; response.session=session_; response.sequence=request.sequence;
    response.type=Type::CommandAck; response.size=2;
    response.payload[0]=uint8_t(Ack::Received); response.payload[1]=uint8_t(request.type);
    Error result=Error::None;
    if(request.type==Type::GetState) {
        const uint8_t held=request.payload[0];
        if(note_!=kNoNote && held==note_) note_refreshed_ms_=now_ms;
        else if(note_!=kNoNote) release();
        response.type=Type::StateSnapshot; backend_.snapshot(response);
    } else if(request.type==Type::GetPatch) {
        response.type=Type::PatchState;
        if(!backend_.patch(get16(request.payload),response)) result=Error::Unsupported;
    } else {
        if(request.type==Type::AuditionNoteOn && note_!=kNoNote) release();
        if(request.type!=Type::AuditionNoteOff || request.payload[0]==note_)
            result=backend_.receive(request);
        if(result==Error::None) {
            if(request.type==Type::AuditionNoteOn) { note_=request.payload[0]; note_refreshed_ms_=now_ms; }
            if(request.type==Type::AuditionNoteOff && request.payload[0]==note_) note_=kNoNote;
            if(request.type==Type::Panic) note_=kNoNote;
        }
    }
    // Backend errors also consume the sequence. A retry gets the same result;
    // a new request is needed to retry a command rejected as Busy.
    if(result!=Error::None) response=error(request,result);
    sequence_=request.sequence; last_receive_ms_=now_ms;
    last_request_=request; last_response_=response; have_request_=true;
    return response;
}
}
