#pragma once
#include "smk_client.h"
#include "smk_state.h"
#include "smk_uart.h"

namespace smk::p4 {
class P4Link {
public:
    explicit P4Link(uint32_t session) : client_(session) {}
    bool begin();
    void service(uint32_t now_ms);
    link::Client& client() { return client_; }
    const link::Client& client() const { return client_; }
    const link::State& state() const { return state_; }
    bool hasState() const { return has_state_; }
    bool ready() const { return ready_; }
    uint32_t stateRevision() const { return snapshots_; }
    bool takeReply(link::Frame& frame) { if(!reply_ready_) return false; frame=reply_; reply_ready_=false; return true; }
private:
    link::Uart uart_; link::Client client_; link::State state_{};
    bool ready_=false,has_state_=false;
    uint32_t last_log_ms_=0,uart_errors_=0,snapshots_=0;
    uint32_t i2s_errors_=0,i2s_timeouts_=0;
    link::Frame reply_{};
    bool reply_ready_=false;
};
}
