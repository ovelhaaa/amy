#pragma once
#include "usb_input.h"
#include "usb_config.h"
#include "usb/usb_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
namespace smk::p4::usb {
// One task owns both library/client APIs and all device resources. No calls
// to AMY or LVGL. Lifecycle is serialized by main, never by a callback.
class Host {
public:
    enum class State : uint32_t { Disabled, Disconnected, Connected, Error };
    explicit Host(amy::UsbInput& input):input_(input) {}
    Host(const Host&)=delete;
    Host& operator=(const Host&)=delete;
    bool begin(unsigned controller);
    void stop();
    State state() const { return state_.load(); }
    uint32_t connects() const { return connects_.load(); }
    uint32_t disconnects() const { return disconnects_.load(); }
    uint32_t errors() const { return errors_.load(); }
    uint32_t stackFree() const { return stack_free_.load(); }
    // Owner-task operations exposed for deterministic mock lifecycle tests.
    bool initialize(unsigned controller);
    void service(uint32_t wait_ticks=0);
    bool shutdown();
private:
    static void task(void*);
    static void event(const usb_host_client_event_msg_t*,void*);
    static void received(usb_transfer_t*);
    void open(uint8_t);
    void fail(bool retry);
    void deviceGone();
    bool cleanup();
    amy::UsbInput& input_;
    usb_host_client_handle_t client_=nullptr;
    usb_device_handle_t device_=nullptr;
    usb_transfer_t* transfer_=nullptr;
    uint8_t interface_=0, endpoint_=0, address_=0, retry_address_=0;
    uint8_t pending_[16]{};
    unsigned transfer_bytes_=0;
    bool installed_=false, claimed_=false, inflight_=false, closing_=false;
    bool device_gone_=false;
    uint32_t retry_at_ms_=0;
    std::atomic<State> state_{State::Disabled};
    std::atomic<uint32_t> connects_{0},disconnects_{0},errors_{0},stack_free_{0};
    std::atomic<bool> stopping_{false};
    std::atomic<bool> startup_ok_{false};
    SemaphoreHandle_t started_=nullptr,done_=nullptr;
    TaskHandle_t task_=nullptr;
    unsigned controller_=0;
};
}
