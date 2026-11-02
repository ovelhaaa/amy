#include "p4_link.h"
#include "display_test.h"
#include "esp_log.h"
#include "sdkconfig.h"

namespace smk::p4 {
static const char* TAG="S3_LINK";
bool P4Link::begin() {
#if defined(CONFIG_SMK_LINK_ENABLED)
    const esp_err_t error=uart_.begin(CONFIG_SMK_LINK_UART,CONFIG_SMK_LINK_TX_GPIO,CONFIG_SMK_LINK_RX_GPIO);
    if(error!=ESP_OK) {
        ESP_LOGE(TAG,"UART init: %s; confirm GPIO configuration",esp_err_to_name(error));
        setLinkStatus("LINK CONFIG ERROR"); return false;
    }
    ready_=true; setLinkStatus("LINK WAITING S3"); return true;
#else
    return false;
#endif
}
void P4Link::service(uint32_t now_ms) {
    if(!ready_) return;
    link::Frame frame{};
    if(uart_.poll(now_ms,frame)) client_.receive(frame,now_ms);
    if(uart_.receiveErrors()!=uart_errors_) { uart_errors_=uart_.receiveErrors(); client_.disconnect(); has_state_=false; }
    if(client_.takeResponse(frame)) {
        if(frame.type==link::Type::StateSnapshot) {
            if(link::readState(frame,state_)) { has_state_=true; ++snapshots_; }
            else { client_.disconnect(); has_state_=false; }
        } else if(frame.type==link::Type::AudioDiagnostics) {
            i2s_errors_=link::get32(frame.payload+18); i2s_timeouts_=link::get32(frame.payload+22);
        } else if(frame.type==link::Type::CommandAck || frame.type==link::Type::CommandError || frame.type==link::Type::PatchState) {
            reply_=frame; reply_ready_=true;
        }
    }
    if(client_.next(now_ms,frame)) {
        // Bytes already in the hardware FIFO precede HELLO on the wire. Purge
        // unsent old-session frames; HELLO's leading delimiter resynchronizes
        // a partially transmitted frame without blocking/flushing hardware.
        if(frame.type==link::Type::Hello) uart_.discardPendingTx();
        uart_.send(frame,frame.type==link::Type::Panic || frame.type==link::Type::AuditionNoteOff);
    }
    if(!client_.connected()) { has_state_=false; reply_ready_=false; }
    setLinkStatus(client_.incompatible()?"LINK VERSION ERROR":client_.connected()?"LINK OK / CONTROL":"LINK WAITING S3");
    if(now_ms-last_log_ms_>=5000) {
        last_log_ms_=now_ms;
        ESP_LOGI(TAG,"connected=%d state=%d snapshots=%lu rx_errors=%lu tx_drops=%lu bad_frames=%lu client_errors_including_timeouts=%lu patch=%u engine_patch=%u i2s_errors=%lu timeouts=%lu",
                 client_.connected(),has_state_,static_cast<unsigned long>(snapshots_),static_cast<unsigned long>(uart_errors_),
                 static_cast<unsigned long>(uart_.transmitDrops()),static_cast<unsigned long>(uart_.frameErrors()),static_cast<unsigned long>(client_.errors()),state_.patch_id,state_.engine_patch,
                 static_cast<unsigned long>(i2s_errors_),static_cast<unsigned long>(i2s_timeouts_));
    }
}
}
