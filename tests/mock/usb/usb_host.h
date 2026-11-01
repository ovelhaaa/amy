#pragma once
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <deque>
#include <thread>
#include <chrono>
#include <cassert>
using esp_err_t = int;
constexpr int ESP_OK = 0, ESP_FAIL = -1, ESP_INTR_FLAG_LEVEL1 = 1;
constexpr int USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS = 1, USB_HOST_LIB_EVENT_FLAGS_ALL_FREE = 2;
constexpr int USB_HOST_CLIENT_EVENT_NEW_DEV = 1, USB_HOST_CLIENT_EVENT_DEV_GONE = 2;
constexpr int USB_TRANSFER_STATUS_COMPLETED = 0, USB_TRANSFER_STATUS_CANCELED = 1;
using usb_device_handle_t = void*;
using usb_host_client_handle_t = void*;
struct usb_config_desc_t { uint8_t bLength, bDescriptorType; uint16_t wTotalLength; };
struct usb_device_desc_t { uint16_t idVendor = 0x1234, idProduct = 0x5678; };
struct usb_transfer_t {
    usb_device_handle_t device_handle;
    uint8_t bEndpointAddress;
    void (*callback)(usb_transfer_t*);
    void* context;
    int num_bytes, actual_num_bytes = 0, data_buffer_size = 512, status = 0;
    uint8_t data_buffer[512]{};
};
struct usb_host_client_event_msg_t {
    int event;
    struct { uint8_t address; } new_dev{};
    struct { usb_device_handle_t dev_hdl; } dev_gone{};
};
struct usb_host_config_t { bool skip_phy_setup; int intr_flags; };
struct usb_host_client_config_t {
    bool is_synchronous;
    int max_num_event_msg;
    struct { void (*client_event_callback)(const usb_host_client_event_msg_t*, void*); void* callback_arg; } async;
};
namespace mock_usb {
inline std::mutex mutex;
inline std::deque<usb_host_client_event_msg_t> events;
inline usb_host_client_config_t client{};
inline uint8_t descriptor[] = {9,2,25,0,1,1,0,0x80,50, 9,4,0,0,1,1,3,0,0, 7,5,0x81,2,64,0,0};
inline usb_device_desc_t device;
inline usb_transfer_t* transfer = nullptr;
inline bool installed = false, registered = false, opened = false, claimed = false, flight = false, flush = false;
inline bool fail_install = false, fail_register = false, fail_alloc = false, fail_submit = false;
inline bool fail_claim = false, fail_descriptor = false;
inline bool fail_free_once = false, fail_release_once = false, fail_close_once = false;
inline int allocations = 0, frees = 0, submits = 0;
inline void event(int kind) {
    std::lock_guard<std::mutex> lock(mutex);
    events.push_back({kind, {1}, {reinterpret_cast<void*>(2)}});
}
inline void reset() {
    assert(!installed && !opened && !claimed && !transfer && !registered);
    events.clear(); flight = flush = false;
    fail_install = fail_register = fail_alloc = fail_submit = false;
    fail_claim = fail_descriptor = false;
    fail_free_once = fail_release_once = fail_close_once = false;
    allocations = frees = submits = 0;
}
}
inline int usb_host_install(const usb_host_config_t*) { if (mock_usb::fail_install) return ESP_FAIL; mock_usb::installed = true; return ESP_OK; }
inline int usb_host_uninstall() { assert(!mock_usb::registered && !mock_usb::opened); mock_usb::installed = false; return ESP_OK; }
inline int usb_host_device_free_all() { return ESP_OK; }
inline int usb_host_lib_handle_events(uint32_t, uint32_t* flags) {
    *flags = 0; std::this_thread::sleep_for(std::chrono::milliseconds(1)); return ESP_OK;
}
inline int usb_host_client_register(const usb_host_client_config_t* config, usb_host_client_handle_t* handle) {
    if (mock_usb::fail_register) return ESP_FAIL;
    mock_usb::client = *config; mock_usb::registered = true; *handle = reinterpret_cast<void*>(1); return ESP_OK;
}
inline int usb_host_client_deregister(usb_host_client_handle_t) { assert(!mock_usb::opened); mock_usb::registered = false; return ESP_OK; }
inline int usb_host_client_handle_events(usb_host_client_handle_t, uint32_t) {
    usb_host_client_event_msg_t event{};
    { std::lock_guard<std::mutex> lock(mock_usb::mutex);
      if (!mock_usb::events.empty()) { event = mock_usb::events.front(); mock_usb::events.pop_front(); } }
    if (event.event >= 3) {
        auto* transfer = mock_usb::transfer;
        assert(transfer && mock_usb::flight);
        mock_usb::flight = false;
        transfer->status = USB_TRANSFER_STATUS_COMPLETED;
        transfer->actual_num_bytes = event.event == 3 ? 3 : event.event == 5 ? 513 : event.event == 6 ? -1 : event.event == 8 ? 8 : 4;
        if (event.event == 7) transfer->status = USB_TRANSFER_STATUS_CANCELED;
        if (event.event == 8) {
            transfer->data_buffer[4] = 8; transfer->data_buffer[5] = 0x80;
            transfer->data_buffer[6] = 60; transfer->data_buffer[7] = 0;
        }
        transfer->data_buffer[0] = 9; transfer->data_buffer[1] = 0x90;
        transfer->data_buffer[2] = 60; transfer->data_buffer[3] = 100;
        if (event.event == 4) mock_usb::fail_submit = true;
        transfer->callback(transfer);
    } else if (event.event) mock_usb::client.async.client_event_callback(&event, mock_usb::client.async.callback_arg);
    if (mock_usb::flush) {
        mock_usb::flush = false; mock_usb::flight = false;
        mock_usb::transfer->status = USB_TRANSFER_STATUS_CANCELED;
        mock_usb::transfer->callback(mock_usb::transfer);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1)); return ESP_OK;
}
inline int usb_host_device_open(usb_host_client_handle_t, uint8_t, usb_device_handle_t* handle) {
    assert(!mock_usb::opened); mock_usb::opened = true; *handle = reinterpret_cast<void*>(2); return ESP_OK;
}
inline int usb_host_device_close(usb_host_client_handle_t, usb_device_handle_t) {
    assert(!mock_usb::flight && !mock_usb::claimed && !mock_usb::transfer);
    if (mock_usb::fail_close_once) { mock_usb::fail_close_once = false; return ESP_FAIL; }
    mock_usb::opened = false; return ESP_OK;
}
inline int usb_host_get_device_descriptor(usb_device_handle_t, const usb_device_desc_t** desc) { *desc = &mock_usb::device; return ESP_OK; }
inline int usb_host_get_active_config_descriptor(usb_device_handle_t, const usb_config_desc_t** desc) { if (mock_usb::fail_descriptor) return ESP_FAIL; *desc = reinterpret_cast<const usb_config_desc_t*>(mock_usb::descriptor); return ESP_OK; }
inline int usb_host_interface_claim(usb_host_client_handle_t, usb_device_handle_t, int, int) { if (mock_usb::fail_claim) return ESP_FAIL; mock_usb::claimed = true; return ESP_OK; }
inline int usb_host_interface_release(usb_host_client_handle_t, usb_device_handle_t, int) { assert(!mock_usb::flight && !mock_usb::transfer); if (mock_usb::fail_release_once) { mock_usb::fail_release_once = false; return ESP_FAIL; } mock_usb::claimed = false; return ESP_OK; }
inline int usb_host_transfer_alloc(size_t, int, usb_transfer_t** transfer) {
    if (mock_usb::fail_alloc) return ESP_FAIL;
    *transfer = mock_usb::transfer = new usb_transfer_t{}; ++mock_usb::allocations; return ESP_OK;
}
inline int usb_host_transfer_free(usb_transfer_t* transfer) {
    assert(!mock_usb::flight); if (mock_usb::fail_free_once) { mock_usb::fail_free_once = false; return ESP_FAIL; } delete transfer; mock_usb::transfer = nullptr; ++mock_usb::frees; return ESP_OK;
}
inline int usb_host_transfer_submit(usb_transfer_t*) {
    ++mock_usb::submits; if (mock_usb::fail_submit) return ESP_FAIL;
    assert(!mock_usb::flight); mock_usb::flight = true; return ESP_OK;
}
inline int usb_host_endpoint_halt(usb_device_handle_t, uint8_t) { return ESP_OK; }
inline int usb_host_endpoint_flush(usb_device_handle_t, uint8_t) { mock_usb::flush = true; return ESP_OK; }
