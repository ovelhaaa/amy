#include "usb_midi_host.h"
#include "esp_log.h"
#include <cstring>
#include "esp_timer.h"

static const char* TAG = "UsbMidiHost";

namespace smk {

UsbMidiHost::UsbMidiHost(EventBus& event_bus) : event_bus_(event_bus) {
    parser_.setSource(EventSource::UsbMidi);
    parser_.setCallback(onMidiEvent, this);
    device_sem_ = xSemaphoreCreateBinary();
}

UsbMidiHost::~UsbMidiHost() {
    stop();
    if (device_sem_) {
        vSemaphoreDelete(device_sem_);
    }
}

bool UsbMidiHost::begin() {
    ESP_LOGI(TAG, "Initializing USB Host Library");
    
    usb_host_config_t host_config = {};
    host_config.skip_phy_setup = false;
    host_config.intr_flags = ESP_INTR_FLAG_LEVEL1;

    esp_err_t err = usb_host_install(&host_config);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install USB host: %s", esp_err_to_name(err));
        return false;
    }
    
    xTaskCreatePinnedToCore(usbHostTask, "usb_host", kHostTaskStackSize, this, 5, &host_task_handle_, 0);
    xTaskCreatePinnedToCore(midiClientTask, "midi_client", kClientTaskStackSize, this, 5, &client_task_handle_, 0);
    
    return true;
}

void UsbMidiHost::stop() {
    if (host_task_handle_) {
        vTaskDelete(host_task_handle_);
        host_task_handle_ = nullptr;
    }
    if (client_task_handle_) {
        vTaskDelete(client_task_handle_);
        client_task_handle_ = nullptr;
    }
    usb_host_uninstall();
}

bool UsbMidiHost::isDeviceConnected() const {
    return device_connected_.load(std::memory_order_relaxed);
}

uint16_t UsbMidiHost::deviceVid() const {
    return device_vid_;
}

uint16_t UsbMidiHost::devicePid() const {
    return device_pid_;
}

uint32_t UsbMidiHost::disconnectCount() const {
    return disconnect_count_.load(std::memory_order_relaxed);
}

uint32_t UsbMidiHost::reconnectCount() const {
    return reconnect_count_.load(std::memory_order_relaxed);
}

void UsbMidiHost::usbHostTask(void* arg) {
    (void)arg;
    while (true) {
        uint32_t event_flags;
        esp_err_t err = usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (err == ESP_OK) {
            if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
                esp_err_t ret = usb_host_device_free_all();
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "All devices freed");
                }
            }
            if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
                ESP_LOGI(TAG, "USB Host All free");
            }
        }
    }
}

void UsbMidiHost::midiClientTask(void* arg) {
    UsbMidiHost* self = static_cast<UsbMidiHost*>(arg);
    
    usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {
            .client_event_callback = clientEventCallback,
            .callback_arg = self,
        }
    };
    
    esp_err_t err = usb_host_client_register(&client_config, &self->client_handle_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register USB client: %s", esp_err_to_name(err));
        vTaskDelete(nullptr);
        return;
    }
    
    while (true) {
        usb_host_client_handle_events(self->client_handle_, pdMS_TO_TICKS(50));
        
        if (xSemaphoreTake(self->device_sem_, 0) == pdTRUE) {
            self->handleDeviceConnection(self->pending_dev_addr_);
        }
    }
}

void UsbMidiHost::clientEventCallback(const usb_host_client_event_msg_t* event, void* arg) {
    UsbMidiHost* self = static_cast<UsbMidiHost*>(arg);
    
    if (event->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
        self->pending_dev_addr_ = event->new_dev.address;
        xSemaphoreGive(self->device_sem_);
    } else if (event->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
        ESP_LOGI(TAG, "Device Gone");
        self->handleDeviceDisconnection();
    }
}

void UsbMidiHost::handleDeviceConnection(uint8_t dev_addr) {
    esp_err_t err = usb_host_device_open(client_handle_, dev_addr, &device_handle_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open device");
        return;
    }
    
    const usb_device_desc_t* dev_desc;
    err = usb_host_get_device_descriptor(device_handle_, &dev_desc);
    if (err == ESP_OK) {
        device_vid_ = dev_desc->idVendor;
        device_pid_ = dev_desc->idProduct;
        ESP_LOGI(TAG, "Connected VID: %04X PID: %04X", device_vid_, device_pid_);
    }
    
    if (findMidiInterface(device_handle_)) {
        err = usb_host_interface_claim(client_handle_, device_handle_, midi_interface_num_, 0);
        if (err == ESP_OK) {
            device_connected_ = true;
            reconnect_count_.fetch_add(1, std::memory_order_relaxed);
            startMidiIn();

            SynthEvent connect_evt;
            connect_evt.type = EventType::UsbConnect;
            connect_evt.source = EventSource::UsbMidi;
            connect_evt.channel = 0;
            connect_evt.id = device_vid_;
            connect_evt.value = device_pid_;
            connect_evt.timestamp_us = (uint32_t)esp_timer_get_time();
            event_bus_.send(connect_evt);
        } else {
            ESP_LOGE(TAG, "Failed to claim MIDI interface");
            usb_host_device_close(client_handle_, device_handle_);
            device_handle_ = nullptr;
        }
    } else {
        ESP_LOGE(TAG, "No MIDI interface found");
        usb_host_device_close(client_handle_, device_handle_);
        device_handle_ = nullptr;
    }
}

bool UsbMidiHost::findMidiInterface(usb_device_handle_t dev_handle) {
    const usb_config_desc_t* config_desc;
    esp_err_t err = usb_host_get_active_config_descriptor(dev_handle, &config_desc);
    if (err != ESP_OK) return false;
    
    const uint8_t* p = (const uint8_t*)config_desc;
    const uint8_t* end = p + config_desc->wTotalLength;
    
    bool found_midi = false;
    
    while (p < end) {
        uint8_t len = p[0];
        uint8_t type = p[1];
        
        if (type == USB_B_DESCRIPTOR_TYPE_INTERFACE) {
            const usb_intf_desc_t* intf = (const usb_intf_desc_t*)p;
            
            // Audio class (1) and MIDIStreaming subclass (3) OR Vendor Specific (255)
            if ((intf->bInterfaceClass == 0x01 && intf->bInterfaceSubClass == 0x03) || 
                (intf->bInterfaceClass == 255)) {
                
                midi_interface_num_ = intf->bInterfaceNumber;
                
                // Search for bulk IN endpoint within this interface
                const uint8_t* ep_ptr = p + len;
                while (ep_ptr < end && ep_ptr[1] != USB_B_DESCRIPTOR_TYPE_INTERFACE) {
                    if (ep_ptr[1] == USB_B_DESCRIPTOR_TYPE_ENDPOINT) {
                        const usb_ep_desc_t* ep = (const usb_ep_desc_t*)ep_ptr;
                        if (USB_EP_DESC_GET_EP_DIR(ep) && 
                            USB_EP_DESC_GET_XFERTYPE(ep) == USB_BM_ATTRIBUTES_XFER_BULK) {
                            midi_in_endpoint_ = USB_EP_DESC_GET_EP_NUM(ep);
                            midi_in_max_packet_ = USB_EP_DESC_GET_MPS(ep);
                            found_midi = true;
                            ESP_LOGI(TAG, "Found MIDI Interface: %d, Endpoint IN: 0x%02X", midi_interface_num_, midi_in_endpoint_ | USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK);
                            break;
                        }
                    }
                    ep_ptr += ep_ptr[0];
                }
            }
        }
        if (found_midi) break;
        p += len;
    }
    
    return found_midi;
}

void UsbMidiHost::startMidiIn() {
    esp_err_t err = usb_host_transfer_alloc(kTransferBufferSize, 0, &in_transfer_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to allocate transfer");
        return;
    }
    
    in_transfer_->device_handle = device_handle_;
    in_transfer_->bEndpointAddress = midi_in_endpoint_ | USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK;
    in_transfer_->callback = transferCallback;
    in_transfer_->context = this;
    in_transfer_->num_bytes = kTransferBufferSize;
    
    err = usb_host_transfer_submit(in_transfer_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to submit transfer");
    }
}

void UsbMidiHost::transferCallback(usb_transfer_t* transfer) {
    UsbMidiHost* self = static_cast<UsbMidiHost*>(transfer->context);
    
    if (transfer->status == USB_TRANSFER_STATUS_COMPLETED) {
        for (int i = 0; i < transfer->actual_num_bytes; i += 4) {
            self->parser_.processUsbMidiPacket(transfer->data_buffer + i);
        }
        
        // Re-submit
        if (self->isDeviceConnected()) {
            transfer->num_bytes = kTransferBufferSize;
            usb_host_transfer_submit(transfer);
        }
    } else {
        // Re-submit on transient non-fatal errors if device is still connected
        if (self->isDeviceConnected() && 
            transfer->status != USB_TRANSFER_STATUS_CANCELED && 
            transfer->status != USB_TRANSFER_STATUS_NO_DEVICE) {
            transfer->num_bytes = kTransferBufferSize;
            usb_host_transfer_submit(transfer);
        }
    }
}

void UsbMidiHost::onMidiEvent(const SynthEvent& event, void* ctx) {
    UsbMidiHost* self = static_cast<UsbMidiHost*>(ctx);
    self->event_bus_.send(event);
}

void UsbMidiHost::handleDeviceDisconnection() {
    device_connected_ = false;
    disconnect_count_.fetch_add(1, std::memory_order_relaxed);
    
    SynthEvent disconnect_evt;
    disconnect_evt.type = EventType::UsbDisconnect;
    disconnect_evt.source = EventSource::UsbMidi;
    disconnect_evt.channel = 0;
    disconnect_evt.id = device_vid_;
    disconnect_evt.value = device_pid_;
    disconnect_evt.timestamp_us = (uint32_t)esp_timer_get_time();
    event_bus_.send(disconnect_evt);

    SynthEvent panic_event;
    panic_event.type = EventType::Panic;
    panic_event.source = EventSource::UsbMidi;
    panic_event.channel = 0;
    panic_event.id = 0;
    panic_event.value = 0;
    panic_event.timestamp_us = (uint32_t)esp_timer_get_time();
    
    event_bus_.send(panic_event);
    
    if (in_transfer_) {
        usb_host_transfer_free(in_transfer_);
        in_transfer_ = nullptr;
    }
    
    if (device_handle_) {
        usb_host_interface_release(client_handle_, device_handle_, midi_interface_num_);
        usb_host_device_close(client_handle_, device_handle_);
        device_handle_ = nullptr;
    }
}

} // namespace smk
