#pragma once
#include "smk_link.h"
#include "smk_uart.h"
#include "event_bus.h"
#include "patch_manager.h"
#include "amy_adapter.h"
#include "clock_manager.h"

namespace smk {
class RemoteLink final : public link::Backend {
public:
    RemoteLink(EventBus& events,PatchManager& patches,AmyAdapter& engine,ClockManager& clock,uint32_t boot_id)
        : events_(events),patches_(patches),engine_(engine),clock_(clock),server_(*this,boot_id) {}
    bool begin();
    void service(uint32_t now_ms);
    link::Error receive(const link::Frame& command) override;
    void releaseAudition() override;
    void snapshot(link::Frame& response) override;
    bool patch(uint16_t id,link::Frame& response) override;
private:
    EventBus& events_; PatchManager& patches_; AmyAdapter& engine_; ClockManager& clock_;
    link::Uart uart_; link::Server server_;
    uint32_t now_ms_=0,last_log_ms_=0,last_diagnostics_ms_=0,uart_errors_=0,last_frames_=0,last_frame_ms_=0;
    bool ready_=false;
};
}
