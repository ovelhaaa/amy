#include "usb_midi_host.h"
#include "descriptor.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "sdkconfig.h"
namespace smk::p4::usb {
namespace {
uint32_t nowMs() { return static_cast<uint32_t>(esp_timer_get_time()/1000); }
}
bool Host::initialize(unsigned controller) {
    if(installed_ || controller>1) return false;
    usb_host_config_t config{}; config.intr_flags=ESP_INTR_FLAG_LEVEL1;
    // IDF 5.5.2 esp32p4: USB_DWC_LL_GET_HW(0)=HS, (1)=FS.
    config.peripheral_map=1U<<controller;
    if(usb_host_install(&config)!=ESP_OK) { ++errors_; state_=State::Error; return false; }
    installed_=true;
    usb_host_client_config_t client{}; client.max_num_event_msg=kClientMessages;
    client.async.client_event_callback=event; client.async.callback_arg=this;
    if(usb_host_client_register(&client,&client_)!=ESP_OK) {
        ++errors_; state_=State::Error; return false;
    }
    state_=State::Disconnected; return true;
}
bool Host::begin(unsigned controller) {
    if(task_ || installed_) return false;
    started_=xSemaphoreCreateBinary(); done_=xSemaphoreCreateBinary();
    if(!started_ || !done_) { state_=State::Error; stop(); return false; }
    controller_=controller; stopping_=false; startup_ok_=false;
    if(xTaskCreate(task,"p4_usb_midi",kStackBytes,this,kPriority,&task_)!=pdPASS) {
        task_=nullptr; state_=State::Error; stop(); return false;
    }
    xSemaphoreTake(started_,portMAX_DELAY);
    if(!startup_ok_) { stop(); state_=State::Error; return false; }
    return true;
}
void Host::stop() {
    if(task_) { stopping_=true; xSemaphoreTake(done_,portMAX_DELAY); task_=nullptr; }
    if(started_) { vSemaphoreDelete(started_); started_=nullptr; }
    if(done_) { vSemaphoreDelete(done_); done_=nullptr; }
}
void Host::task(void* ctx) {
    auto& h=*static_cast<Host*>(ctx);
    const bool ready=h.initialize(h.controller_);
    h.startup_ok_=ready;
    xSemaphoreGive(h.started_);
    if(ready) while(!h.stopping_) {
        // Client completion wakes immediately; only idle/enumeration waits
        // up to one scheduler tick. No fixed sleep per received MIDI packet.
        h.service(1); h.stack_free_=uxTaskGetStackHighWaterMark(nullptr);
    }
    while(!h.shutdown()) vTaskDelay(1); // Retain ownership until callbacks drain.
    xSemaphoreGive(h.done_); vTaskDelete(nullptr);
}
void Host::event(const usb_host_client_event_msg_t* e,void* ctx) {
    auto& h=*static_cast<Host*>(ctx);
    if(e->event==USB_HOST_CLIENT_EVENT_NEW_DEV) {
        const unsigned a=e->new_dev.address;
        if(a && a<128) h.pending_[a/8]|=1U<<(a%8);
    } else if(e->event==USB_HOST_CLIENT_EVENT_DEV_GONE && e->dev_gone.dev_hdl==h.device_) {
        h.deviceGone();
    }
}
void Host::fail(bool retry,const char* stage,int detail) {
    // Bounded diagnostics in the USB owner task, never in the audio path.
    if(errors_.load()<8) ESP_LOGW("P4_USB","failure stage=%s detail=%d address=%u inflight=%d closing=%d",
        stage,detail,address_,inflight_,closing_);
    ++errors_; if(!closing_) input_.disconnect(); state_=State::Error; closing_=true;
    retry_address_=retry?address_:0; retry_at_ms_=nowMs()+kRetryMs;
}
void Host::deviceGone() {
    // IDF may deliver NO_DEVICE before DEV_GONE. Count/cancel once and
    // await a fresh NEW_DEV instead of retrying the removed address.
    if(device_gone_) return;
    device_gone_=true; ++disconnects_; retry_address_=0; waiting_removal_=false;
    if(!closing_) input_.disconnect();
    closing_=true; state_=State::Disconnected;
}
void Host::open(uint8_t address) {
    address_=address; device_gone_=false; waiting_removal_=false; input_.disconnect();
    const auto opened=usb_host_device_open(client_,address,&device_);
    if(opened!=ESP_OK) { device_=nullptr; fail(false,"open",opened); return; }
    const usb_device_desc_t* dd=nullptr; const usb_config_desc_t* cfg=nullptr;
    if(usb_host_get_device_descriptor(device_,&dd)!=ESP_OK || !dd ||
       usb_host_get_active_config_descriptor(device_,&cfg)!=ESP_OK || !cfg) { fail(false); return; }
    ESP_LOGI("P4_USB","VID=%04x PID=%04x address=%u descriptor_bytes=%u",
        dd->idVendor,dd->idProduct,address,cfg->wTotalLength);
#ifdef CONFIG_SMKP4_USB_DESCRIPTOR_LOG
    ESP_LOG_BUFFER_HEX_LEVEL("P4_USB_DESC",cfg,cfg->wTotalLength<256?cfg->wTotalLength:256,ESP_LOG_INFO);
#endif
    Endpoint ep;
    if(!findEndpoint(reinterpret_cast<const uint8_t*>(cfg),cfg->wTotalLength,ep)) {
        fail(false); return;
    }
    interface_=ep.interface_number; endpoint_=ep.address;
    transfer_bytes_=ep.packet_bytes; // One packet, exact multiple of MPS.
    ESP_LOGI("P4_USB","MIDI1 interface=%u alt=%u IN=%02x MPS=%u",interface_,ep.alternate,endpoint_,transfer_bytes_);
    if(transfer_bytes_>kMaxTransferBytes || usb_host_interface_claim(client_,device_,interface_,ep.alternate)!=ESP_OK) {
        fail(false); return;
    }
    claimed_=true;
    if(usb_host_transfer_alloc(transfer_bytes_,0,&transfer_)!=ESP_OK) { fail(true); return; }
    transfer_->device_handle=device_; transfer_->bEndpointAddress=endpoint_;
    transfer_->callback=received; transfer_->context=this; transfer_->num_bytes=transfer_bytes_;
    if(usb_host_transfer_submit(transfer_)!=ESP_OK) { fail(true); return; }
    inflight_=true; state_=State::Connected; ++connects_;
}
void Host::received(usb_transfer_t* t) {
    auto& h=*static_cast<Host*>(t->context); h.inflight_=false;
    if(t->status==USB_TRANSFER_STATUS_NO_DEVICE) { h.deviceGone(); return; }
    if(h.closing_ || h.stopping_) return;
    if(t->status==USB_TRANSFER_STATUS_ERROR) {
        // HS removal can report a bus error before DEV_GONE. Closing the
        // device here unsubscribes this client from the later notification.
        // Cancel notes now, retain ownership while pumping events, then fail
        // normally if removal is not confirmed within the bounded grace.
        if(h.removal_trace_count_<8) {
            ++h.removal_trace_count_;
            ESP_LOGW("P4_USB","transfer error; awaiting removal address=%u",h.address_);
        }
        h.input_.disconnect(); h.closing_=true; h.waiting_removal_=true;
        h.removal_deadline_ms_=nowMs()+kRemovalGraceMs; h.state_=State::Error;
        return;
    }
    if(t->status!=USB_TRANSFER_STATUS_COMPLETED || t->actual_num_bytes<0 ||
       t->actual_num_bytes>t->num_bytes || t->actual_num_bytes>t->data_buffer_size ||
       !h.input_.transfer(t->data_buffer,static_cast<size_t>(t->actual_num_bytes),nowMs())) {
        h.fail(true,"receive",static_cast<int>(t->status)); return;
    }
    const auto submitted=usb_host_transfer_submit(t);
    if(submitted!=ESP_OK) { h.fail(true,"resubmit",submitted); return; }
    h.inflight_=true;
}
bool Host::cleanup() {
    if(inflight_) {
        if(usb_host_endpoint_halt(device_,endpoint_)==ESP_OK) usb_host_endpoint_flush(device_,endpoint_);
        return false;
    }
    if(transfer_) { if(usb_host_transfer_free(transfer_)!=ESP_OK) return false; transfer_=nullptr; }
    if(claimed_) { if(usb_host_interface_release(client_,device_,interface_)!=ESP_OK) return false; claimed_=false; }
    if(device_) { if(usb_host_device_close(client_,device_)!=ESP_OK) return false; device_=nullptr; }
    closing_=false; waiting_removal_=false; return true;
}
void Host::service(uint32_t wait_ticks) {
    uint32_t flags=0;
    const auto lib=usb_host_lib_handle_events(0,&flags);
    if(lib!=ESP_OK && lib!=ESP_ERR_TIMEOUT) { if(!closing_) fail(true,"library",lib); }
    if(client_) {
        const auto result=usb_host_client_handle_events(client_,wait_ticks);
        if(result!=ESP_OK && result!=ESP_ERR_TIMEOUT && !closing_) fail(true,"client",result);
    }
    if(waiting_removal_) {
        if(!amy::atOrAfter(nowMs(),removal_deadline_ms_)) return;
        waiting_removal_=false; fail(true,"unconfirmed removal",USB_TRANSFER_STATUS_ERROR);
    }
    if(closing_) { cleanup(); return; }
    if(device_) return;
    for(unsigned a=1;a<128;++a) if(pending_[a/8]&(1U<<(a%8))) {
        pending_[a/8]&=~(1U<<(a%8)); retry_address_=0; open(static_cast<uint8_t>(a)); return;
    }
    if(retry_address_ && amy::atOrAfter(nowMs(),retry_at_ms_)) {
        const auto a=retry_address_; retry_address_=0; open(a);
    }
}
bool Host::shutdown() {
    if(!installed_) return true;
    if(!closing_) { input_.disconnect(); closing_=true; }
    uint32_t flags=0; usb_host_lib_handle_events(0,&flags);
    if(client_) usb_host_client_handle_events(client_,0);
    if(!cleanup()) return false;
    if(client_) { if(usb_host_client_deregister(client_)!=ESP_OK) return false; client_=nullptr; }
    if(usb_host_device_free_all()!=ESP_OK && !(flags&USB_HOST_LIB_EVENT_FLAGS_ALL_FREE)) return false;
    if(usb_host_uninstall()!=ESP_OK) return false;
    installed_=false; state_=State::Disabled; return true;
}
}
