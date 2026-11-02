#pragma once
#include "smk_protocol.h"
#include "driver/uart.h"
#include "esp_err.h"

namespace smk::link {
constexpr int kBaud = 115200;
constexpr unsigned kTxSlots = 4;
constexpr unsigned kRxRingBytes = 2048;
constexpr unsigned kServiceBytes = 64;
// Single control/UI owner; no calls from an ISR, synth worker or audio task.
class Uart {
public:
    esp_err_t begin(int port,int tx_gpio,int rx_gpio);
    bool send(const Frame& frame,bool release=false);
    bool poll(uint32_t now_ms,Frame& frame);
    uint32_t receiveErrors() const { return rx_errors_; }
    uint32_t transmitDrops() const { return tx_drops_; }
    uint32_t frameErrors() const { return decoder_.errors(); }
private:
    struct Packet { uint8_t bytes[kWireBytes]{}; size_t size=0,offset=0; } tx_[kTxSlots];
    Decoder decoder_;
    uart_port_t port_=UART_NUM_1;
    QueueHandle_t events_=nullptr;
    uint8_t rx_[kServiceBytes]{};
    unsigned head_=0,count_=0,rx_pos_=0,rx_size_=0;
    uint32_t rx_errors_=0,tx_drops_=0;
    bool ready_=false;
};
}
