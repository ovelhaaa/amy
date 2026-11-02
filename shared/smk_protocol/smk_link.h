#pragma once
#include "smk_protocol.h"

namespace smk::link {
// Control-side ownership only. Implementations must never invoke AMY directly.
class Backend {
public:
    virtual ~Backend() = default;
    virtual Error receive(const Frame& command) = 0;
    virtual void releaseAudition() = 0;
    virtual void snapshot(Frame& response) = 0;
    virtual bool patch(uint16_t id, Frame& response) = 0;
};

class Server {
public:
    Server(Backend& backend,uint32_t boot_id) : backend_(backend),boot_id_(boot_id) {}
    Frame handle(const Frame& request,uint32_t now_ms);
    void tick(uint32_t now_ms);
    void disconnect();
    bool connected() const { return connected_; }
    uint32_t session() const { return session_; }
    uint32_t disconnects() const { return disconnects_; }
    uint32_t reconnects() const { return reconnects_; }
    uint32_t rejected() const { return rejected_; }
    uint8_t auditionNote() const { return note_; }
private:
    Frame error(const Frame& request,Error reason);
    void release();
    Backend& backend_;
    const uint32_t boot_id_;
    uint32_t session_=0,last_receive_ms_=0,note_refreshed_ms_=0;
    uint32_t disconnects_=0,reconnects_=0,rejected_=0;
    uint16_t sequence_=0;
    uint8_t note_=kNoNote;
    bool connected_=false,have_request_=false;
    Frame last_request_{},last_response_{};
};
}
