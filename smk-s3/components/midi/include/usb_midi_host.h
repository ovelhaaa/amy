#pragma once
#include <cstdint>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "usb/usb_host.h"
#include "event_bus.h"
#include "midi_parser.h"

namespace smk {
class UsbMidiHost {
public:
    enum class State : uint8_t { Disconnected, Enumerating, Configuring, Ready, Recovering, Error };
    explicit UsbMidiHost(EventBus& event_bus);
    ~UsbMidiHost();
    bool begin();
    // Lifecycle calls are serialized by the application; never call from USB callbacks.
    void stop();
    bool isDeviceConnected() const { return state() == State::Ready; }
    State state() const { return state_.load(std::memory_order_acquire); }
    uint16_t deviceVid() const { return device_vid_.load(); }
    uint16_t devicePid() const { return device_pid_.load(); }
    void setVelocityCurve(VelocityCurve curve) { parser_.setVelocityCurve(curve); }
    VelocityCurve velocityCurve() const { return parser_.velocityCurve(); }
    MidiParser& parser() { return parser_; }
    const MidiParser& parser() const { return parser_; }
    uint32_t disconnectCount() const { return disconnect_count_.load(); }
    uint32_t reconnectCount() const { return reconnect_count_.load(); }
    UsbMidiHost(const UsbMidiHost&) = delete;
    UsbMidiHost& operator=(const UsbMidiHost&) = delete;
private:
    static void usbHostTask(void* arg);
    static void midiClientTask(void* arg);
    static void clientEventCallback(const usb_host_client_event_msg_t* event, void* arg);
    static void transferCallback(usb_transfer_t* transfer);
    static void onMidiEvent(const SynthEvent& event, void* ctx);
    void handleDeviceConnection(uint8_t address);
    void handleDeviceDisconnection();
    bool findMidiInterface(usb_device_handle_t device);
    bool startMidiIn();
    void failReception();
    bool cleanupDevice();
    void notifyDisconnect();
    EventBus& event_bus_;
    MidiParser parser_;
    usb_host_client_handle_t client_handle_ = nullptr;
    usb_device_handle_t device_handle_ = nullptr;
    usb_transfer_t* in_transfer_ = nullptr;
    bool claimed_ = false, in_flight_ = false, draining_ = false, installed_ = false, gone_ = false;
    uint8_t midi_interface_num_ = 0, midi_in_endpoint_ = 0, pending_dev_addr_ = 0;
    std::atomic<uint16_t> device_vid_{0}, device_pid_{0};
    std::atomic<State> state_{State::Disconnected};
    std::atomic<bool> stop_client_{false}, stop_host_{false}, client_ready_{false};
    std::atomic<uint32_t> disconnect_count_{0}, reconnect_count_{0};
    TaskHandle_t host_task_handle_ = nullptr, client_task_handle_ = nullptr;
    SemaphoreHandle_t startup_ = nullptr, client_done_ = nullptr, host_done_ = nullptr;
    static constexpr size_t kTransferBufferSize = 512;
    static constexpr uint32_t kEventWaitMs = 20;
    static constexpr uint32_t kHostTaskStackSize = 4096, kClientTaskStackSize = 4096;
};
} // namespace smk
