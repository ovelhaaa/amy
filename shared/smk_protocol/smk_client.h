#pragma once
#include "smk_protocol.h"

namespace smk::link {
// Stop-and-wait. One immutable in-flight command, two release latches; callers
// coalesce unsent UI values. No retry ever changes sequence or payload.
class Client {
public:
    explicit Client(uint32_t session) : session_(session?session:1) {}
    bool request(Type type,const uint8_t* payload,uint16_t size,uint32_t now_ms);
    void releaseNote(uint8_t note);
    void panic() { panic_pending_=true; held_=kNoNote; }
    void heldNote(uint8_t note) { held_=note; }
    bool next(uint32_t now_ms,Frame& transmit);
    void receive(const Frame& response,uint32_t now_ms);
    bool takeResponse(Frame& response);
    void disconnect();
    bool connected() const { return connected_; }
    bool incompatible() const { return incompatible_; }
    bool busy() const { return pending_; }
    uint32_t errors() const { return errors_; }
    uint32_t bootId() const { return boot_id_; }
private:
    uint32_t session_=1,boot_id_=0,last_receive_ms_=0,last_send_ms_=0,last_state_ms_=0,errors_=0;
    uint16_t sequence_=1;
    uint8_t retries_=0,held_=kNoNote,release_pending_=kNoNote;
    bool connected_=false,incompatible_=false,pending_=false,sent_=false,response_ready_=false,panic_pending_=false;
    Frame command_{},response_{};
};
}
