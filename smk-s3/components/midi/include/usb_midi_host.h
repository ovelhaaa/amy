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
    explicit UsbMidiHost(EventBus& event_bus);
    ~UsbMidiHost();
    
    bool begin();
    void stop();
    
    // State
    bool isDeviceConnected() const;
    uint16_t deviceVid() const;
    uint16_t devicePid() const;
    
    // Diagnostics
    uint32_t disconnectCount() const;
    uint32_t reconnectCount() const;
    
private:
    // FreeRTOS tasks
    static void usbHostTask(void* arg);      // USB host library daemon
    static void midiClientTask(void* arg);   // MIDI client handling
    
    // USB Host callbacks
    static void clientEventCallback(const usb_host_client_event_msg_t* event, void* arg);
    static void transferCallback(usb_transfer_t* transfer);
    static void onMidiEvent(const SynthEvent& event, void* ctx);
    
    // Device handling
    void handleDeviceConnection(uint8_t dev_addr);
    void handleDeviceDisconnection();
    bool findMidiInterface(usb_device_handle_t dev_handle);
    void startMidiIn();
    
    EventBus& event_bus_;
    MidiParser parser_;
    
    // USB handles
    usb_host_client_handle_t client_handle_ = nullptr;
    usb_device_handle_t device_handle_ = nullptr;
    usb_transfer_t* in_transfer_ = nullptr;
    
    // Device info
    uint8_t midi_interface_num_ = 0;
    uint8_t midi_in_endpoint_ = 0;
    uint16_t midi_in_max_packet_ = 64;
    uint16_t device_vid_ = 0;
    uint16_t device_pid_ = 0;
    
    // State
    std::atomic<bool> device_connected_{false};
    std::atomic<uint32_t> disconnect_count_{0};
    std::atomic<uint32_t> reconnect_count_{0};
    
    // Task handles
    TaskHandle_t host_task_handle_ = nullptr;
    TaskHandle_t client_task_handle_ = nullptr;
    
    // Semaphore for device connection notification
    SemaphoreHandle_t device_sem_ = nullptr;
    uint8_t pending_dev_addr_ = 0;
    
    static constexpr size_t kTransferBufferSize = 512;
    static constexpr uint32_t kHostTaskStackSize = 4096;
    static constexpr uint32_t kClientTaskStackSize = 4096;
};

} // namespace smk
