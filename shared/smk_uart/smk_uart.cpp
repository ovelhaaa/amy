#include "smk_uart.h"
#include "driver/gpio.h"
#include "freertos/queue.h"
#include "sdkconfig.h"
#include <algorithm>

namespace smk::link {
esp_err_t Uart::begin(int port,int tx_gpio,int rx_gpio) {
    if(ready_) return ESP_ERR_INVALID_STATE;
    if(port<0 || port>=UART_NUM_MAX || tx_gpio<0 || rx_gpio<0 || tx_gpio==rx_gpio ||
       !GPIO_IS_VALID_OUTPUT_GPIO(tx_gpio) || !GPIO_IS_VALID_GPIO(rx_gpio)) return ESP_ERR_INVALID_ARG;
#if CONFIG_ESP_CONSOLE_UART_NUM >= 0
    if(port==CONFIG_ESP_CONSOLE_UART_NUM) return ESP_ERR_INVALID_ARG;
#endif
    port_=static_cast<uart_port_t>(port);
    uart_config_t config{}; config.baud_rate=kBaud; config.data_bits=UART_DATA_8_BITS;
    config.parity=UART_PARITY_DISABLE; config.stop_bits=UART_STOP_BITS_1;
    config.flow_ctrl=UART_HW_FLOWCTRL_DISABLE; config.source_clk=UART_SCLK_DEFAULT;
    esp_err_t error=uart_param_config(port_,&config);
    if(error!=ESP_OK) return error;
    error=uart_set_pin(port_,tx_gpio,rx_gpio,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);
    if(error!=ESP_OK) return error;
    error=uart_driver_install(port_,kRxRingBytes,0,8,&events_,0);
    if(error!=ESP_OK) return error;
    ready_=true;
    return ESP_OK;
}
bool Uart::send(const Frame& frame,bool release) {
    if(!ready_ || count_>=(release?kTxSlots:kTxSlots-1)) { ++tx_drops_; return false; }
    auto& packet=tx_[(head_+count_)%kTxSlots];
    packet.size=encode(frame,packet.bytes,sizeof(packet.bytes)); packet.offset=0;
    if(!packet.size) { ++tx_drops_; return false; }
    ++count_; return true;
}
bool Uart::poll(uint32_t now_ms,Frame& frame) {
    if(!ready_) return false;
    decoder_.expire(now_ms);
    uart_event_t event{};
    for(unsigned i=0;i<2 && xQueueReceive(events_,&event,0)==pdTRUE;++i) {
        if(event.type==UART_FIFO_OVF || event.type==UART_BUFFER_FULL || event.type==UART_PARITY_ERR || event.type==UART_FRAME_ERR) {
            ++rx_errors_; decoder_.reset(); rx_pos_=rx_size_=0;
            uart_flush_input(port_);
        }
    }
    if(count_) {
        auto& packet=tx_[head_];
        const size_t size=std::min(packet.size-packet.offset,size_t(kServiceBytes));
        const int written=uart_tx_chars(port_,reinterpret_cast<const char*>(packet.bytes+packet.offset),size);
        if(written<0) { ++tx_drops_; ++rx_errors_; ready_=false; return false; }
        if(written>0) packet.offset+=static_cast<size_t>(written);
        if(packet.offset==packet.size) { head_=(head_+1)%kTxSlots; --count_; }
    }
    if(rx_pos_==rx_size_) {
        const int read=uart_read_bytes(port_,rx_,sizeof(rx_),0);
        rx_pos_=0; rx_size_=read>0?static_cast<unsigned>(read):0;
        if(read<0) ++rx_errors_;
    }
    for(unsigned i=0;i<kServiceBytes && rx_pos_<rx_size_;++i)
        if(decoder_.feed(rx_[rx_pos_++],now_ms,frame)==Decoder::Result::Packet) return true;
    return false;
}
}
