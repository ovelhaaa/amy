#include "usb_midi_host.h"
#include "usb_midi_descriptor.h"
#include "diagnostics.h"
#include "esp_log.h"
#include "esp_timer.h"
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

static const char* TAG = "UsbMidiHost";
namespace smk {
namespace {
int16_t encodeUsbPid(uint16_t pid) {
    return pid <= INT16_MAX ? static_cast<int16_t>(pid)
                            : static_cast<int16_t>(static_cast<int32_t>(pid) - 65536);
}
}
UsbMidiHost::UsbMidiHost(EventBus& bus) : event_bus_(bus) {
    parser_.setSource(EventSource::UsbMidi);
    parser_.setCallback(onMidiEvent, this);
    startup_ = xSemaphoreCreateBinary();
    client_done_ = xSemaphoreCreateBinary();
    host_done_ = xSemaphoreCreateBinary();
}
UsbMidiHost::~UsbMidiHost() {
    stop();
    if (startup_) vSemaphoreDelete(startup_);
    if (client_done_) vSemaphoreDelete(client_done_);
    if (host_done_) vSemaphoreDelete(host_done_);
}
bool UsbMidiHost::begin() {
    if (installed_ || !startup_ || !client_done_ || !host_done_ || !event_bus_.ready()) return false;
    usb_host_config_t config{};
    config.intr_flags = ESP_INTR_FLAG_LEVEL1;
    if (usb_host_install(&config) != ESP_OK) { state_ = State::Error; return false; }
    installed_ = true;
    state_ = State::Disconnected;
    pending_dev_addr_ = 0;
    parser_.reset();
    stop_host_ = false;
    stop_client_ = false;
    client_ready_ = false;
    if (xTaskCreatePinnedToCore(usbHostTask, "usb_host", kHostTaskStackSize, this, 5, &host_task_handle_, 0) != pdPASS) {
        host_task_handle_ = nullptr;
        ESP_LOGE(TAG, "USB host task creation failed");
        stop(); state_ = State::Error; return false;
    }
    if (xTaskCreatePinnedToCore(midiClientTask, "midi_client", kClientTaskStackSize, this, 5, &client_task_handle_, 0) != pdPASS) {
        client_task_handle_ = nullptr;
        ESP_LOGE(TAG, "MIDI client task creation failed");
        stop(); state_ = State::Error; return false;
    }
    xSemaphoreTake(startup_, portMAX_DELAY);
    if (!client_ready_) { ESP_LOGE(TAG, "USB client registration failed"); stop(); state_ = State::Error; return false; }
    return true;
}
void UsbMidiHost::stop() {
    if (!installed_) return;
    stop_client_.store(true, std::memory_order_release);
    if (client_task_handle_) {
        xSemaphoreTake(client_done_, portMAX_DELAY);
        client_task_handle_ = nullptr;
    }
    stop_host_.store(true, std::memory_order_release);
    if (host_task_handle_) {
        xSemaphoreTake(host_done_, portMAX_DELAY);
        host_task_handle_ = nullptr;
    }
    if (usb_host_uninstall() == ESP_OK) { installed_ = false; state_ = State::Disconnected; }
    else { state_ = State::Error; ESP_LOGE(TAG, "USB host uninstall failed"); }
}
void UsbMidiHost::usbHostTask(void* arg) {
    auto* self = static_cast<UsbMidiHost*>(arg);
    bool freeing = false;
    while (true) {
        uint32_t flags = 0;
        usb_host_lib_handle_events(pdMS_TO_TICKS(kEventWaitMs), &flags);
        if (self->stop_host_.load(std::memory_order_acquire)) {
            if ((freeing && (flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE)) ||
                usb_host_device_free_all() == ESP_OK) break;
            freeing = true;
        } else if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            usb_host_device_free_all();
        }
    }
    xSemaphoreGive(self->host_done_);
    vTaskDelete(nullptr);
}
void UsbMidiHost::midiClientTask(void* arg) {
    auto* self = static_cast<UsbMidiHost*>(arg);
    usb_host_client_config_t config{};
    config.max_num_event_msg = 5;
    config.async.client_event_callback = clientEventCallback;
    config.async.callback_arg = self;
    const bool registered = usb_host_client_register(&config, &self->client_handle_) == ESP_OK;
    self->client_ready_ = registered;
    xSemaphoreGive(self->startup_);
    if (registered) {
        while (!self->stop_client_.load(std::memory_order_acquire)) {
            usb_host_client_handle_events(self->client_handle_, pdMS_TO_TICKS(kEventWaitMs));
            if (self->draining_) self->cleanupDevice();
            if (self->pending_dev_addr_ ) {
                const uint8_t address = self->pending_dev_addr_;
                self->pending_dev_addr_ = 0;
                self->handleDeviceConnection(address);
            }
        }
        if (self->device_handle_) {
            self->notifyDisconnect();
            self->state_ = State::Recovering;
            self->draining_ = true;
        }
        // Flush callbacks must complete before release/close/deregister. Never
        // free an in-flight transfer or force-delete the task owning callbacks.
        while (!self->cleanupDevice())
            usb_host_client_handle_events(self->client_handle_, pdMS_TO_TICKS(kEventWaitMs));
        while (usb_host_client_deregister(self->client_handle_) != ESP_OK)
            usb_host_client_handle_events(self->client_handle_, pdMS_TO_TICKS(kEventWaitMs));
        self->client_handle_ = nullptr;
    }
    xSemaphoreGive(self->client_done_);
    vTaskDelete(nullptr);
}
void UsbMidiHost::clientEventCallback(const usb_host_client_event_msg_t* event, void* arg) {
    auto* self = static_cast<UsbMidiHost*>(arg);
    if (event->event == USB_HOST_CLIENT_EVENT_NEW_DEV && !self->stop_client_)
        self->pending_dev_addr_ = event->new_dev.address;
    else if (event->event == USB_HOST_CLIENT_EVENT_DEV_GONE && event->dev_gone.dev_hdl == self->device_handle_)
        self->handleDeviceDisconnection();
}
void UsbMidiHost::handleDeviceConnection(uint8_t address) {
    if (device_handle_ || stop_client_) return;
    state_ = State::Enumerating;
    gone_ = false;
    parser_.reset();
    if (usb_host_device_open(client_handle_, address, &device_handle_) != ESP_OK) {
        state_ = State::Error;
        ESP_LOGE(TAG, "USB device open failed");
        return;
    }
    const usb_device_desc_t* descriptor = nullptr;
    if (usb_host_get_device_descriptor(device_handle_, &descriptor) != ESP_OK || !descriptor) { failReception(); return; }
    device_vid_ = descriptor->idVendor;
    device_pid_ = descriptor->idProduct;
    state_ = State::Configuring;
    if (!findMidiInterface(device_handle_)) {
        failReception(); return;
    }
#ifdef ESP_PLATFORM
    ESP_LOGI(TAG, "Claim VID=%04x PID=%04x interface=%u IN=%02x; DMA free=%zu largest=%zu internal free=%zu",
             static_cast<unsigned>(device_vid_.load()), static_cast<unsigned>(device_pid_.load()),
             static_cast<unsigned>(midi_interface_num_), static_cast<unsigned>(midi_in_endpoint_),
             heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL),
             heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL),
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
#endif
    const esp_err_t claim_result = usb_host_interface_claim(client_handle_, device_handle_, midi_interface_num_, 0);
    if (claim_result != ESP_OK) {
        ESP_LOGE(TAG, "MIDI interface claim failed: %s", esp_err_to_name(claim_result));
        failReception(); return;
    }
    claimed_ = true;
    if (!startMidiIn()) { failReception(); return; }
    state_ = State::Ready;
    reconnect_count_.fetch_add(1);
    auto& diag = Diagnostics::instance().counters();
    diag.usb_reconnects.fetch_add(1);
    diag.usb_connected = true;
    event_bus_.send({EventType::UsbConnect, EventSource::UsbMidi, 0, deviceVid(),
                     encodeUsbPid(devicePid()), static_cast<uint64_t>(esp_timer_get_time())});
}
bool UsbMidiHost::findMidiInterface(usb_device_handle_t device) {
    const usb_config_desc_t* descriptor = nullptr;
    if (usb_host_get_active_config_descriptor(device, &descriptor) != ESP_OK || !descriptor) return false;
    MidiEndpoint endpoint;
    // ESP-IDF owns this cached, enumerated allocation. Its API provides no
    // separate allocation length; wTotalLength is the SDK's accessible span.
    if (!findMidiEndpoint(reinterpret_cast<const uint8_t*>(descriptor), descriptor->wTotalLength, endpoint)) return false;
    midi_interface_num_ = endpoint.interface_number;
    midi_in_endpoint_ = endpoint.address;
    return true;
}
bool UsbMidiHost::startMidiIn() {
    if (usb_host_transfer_alloc(kTransferBufferSize, 0, &in_transfer_) != ESP_OK) return false;
    in_transfer_->device_handle = device_handle_;
    in_transfer_->bEndpointAddress = midi_in_endpoint_;
    in_transfer_->callback = transferCallback;
    in_transfer_->context = this;
    in_transfer_->num_bytes = kTransferBufferSize;
    if (usb_host_transfer_submit(in_transfer_) != ESP_OK) {
        Diagnostics::instance().counters().usb_submit_failures.fetch_add(1);
        return false;
    }
    in_flight_ = true;
    return true;
}
void UsbMidiHost::transferCallback(usb_transfer_t* transfer) {
    auto* self = static_cast<UsbMidiHost*>(transfer->context);
    self->in_flight_ = false;
    if (self->draining_ || self->stop_client_) return;
    if (transfer->status != USB_TRANSFER_STATUS_COMPLETED || transfer->actual_num_bytes < 0 ||
        transfer->actual_num_bytes > transfer->num_bytes || transfer->actual_num_bytes > transfer->data_buffer_size) {
        Diagnostics::instance().counters().usb_transfer_errors.fetch_add(1);
        self->failReception(); return;
    }
    // Reject the whole malformed transfer: a missing release requires Panic.
    if (!self->parser_.processUsbMidiTransfer(transfer->data_buffer, transfer->actual_num_bytes)) {
        self->failReception(); return;
    }
    transfer->num_bytes = kTransferBufferSize;
    if (usb_host_transfer_submit(transfer) != ESP_OK) {
        Diagnostics::instance().counters().usb_submit_failures.fetch_add(1);
        self->failReception(); return;
    }
    self->in_flight_ = true;
}
void UsbMidiHost::onMidiEvent(const SynthEvent& event, void* ctx) {
    auto* self = static_cast<UsbMidiHost*>(ctx);
    if (!self->event_bus_.send(event)) Diagnostics::instance().counters().midi_events_dropped.fetch_add(1);
}
void UsbMidiHost::notifyDisconnect() {
    // Connection truth is updated independently of possibly saturated UI events.
    Diagnostics::instance().counters().usb_connected = false;
    event_bus_.send({EventType::UsbDisconnect, EventSource::UsbMidi, 0, deviceVid(),
                     encodeUsbPid(devicePid()), static_cast<uint64_t>(esp_timer_get_time())});
}
void UsbMidiHost::failReception() {
    notifyDisconnect();
    state_ = State::Recovering;
    draining_ = true;
    ESP_LOGE(TAG, "MIDI reception failed; Panic requested, draining device resources");
}
void UsbMidiHost::handleDeviceDisconnection() {
    if (!device_handle_ || gone_) return;
    gone_ = true;
    disconnect_count_.fetch_add(1);
    Diagnostics::instance().counters().usb_disconnects.fetch_add(1);
    if (!draining_) notifyDisconnect();
    state_ = State::Recovering;
    draining_ = true;
    parser_.reset();
}
bool UsbMidiHost::cleanupDevice() {
    if (in_flight_) {
        // Host calls are task-owned, not ISR/audio operations. On an API error
        // retain ownership and retry while servicing callbacks, never free early.
        if (usb_host_endpoint_halt(device_handle_, midi_in_endpoint_) == ESP_OK)
            usb_host_endpoint_flush(device_handle_, midi_in_endpoint_);
        return false;
    }
    if (in_transfer_) {
        if (usb_host_transfer_free(in_transfer_) != ESP_OK) return false;
        in_transfer_ = nullptr;
    }
    if (claimed_) {
        if (usb_host_interface_release(client_handle_, device_handle_, midi_interface_num_) != ESP_OK) return false;
        claimed_ = false;
    }
    if (device_handle_) {
        if (usb_host_device_close(client_handle_, device_handle_) != ESP_OK) return false;
        device_handle_ = nullptr;
    }
    parser_.reset();
    draining_ = false;
    state_ = State::Disconnected;
    return true;
}
} // namespace smk
