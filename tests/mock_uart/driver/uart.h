#pragma once
#include "esp_err.h"
#include "freertos/queue.h"
#include <algorithm>

using uart_port_t=int;
constexpr int UART_NUM_1=1, UART_NUM_MAX=3, UART_DATA_8_BITS=8, UART_PARITY_DISABLE=0;
constexpr int UART_STOP_BITS_1=1, UART_HW_FLOWCTRL_DISABLE=0, UART_SCLK_DEFAULT=0, UART_PIN_NO_CHANGE=-1;
constexpr int UART_FIFO_OVF=1, UART_BUFFER_FULL=2, UART_PARITY_ERR=3, UART_FRAME_ERR=4;
struct uart_config_t { int baud_rate,data_bits,parity,stop_bits,flow_ctrl,source_clk; };
struct uart_event_t { int type; };
namespace mock_uart { inline size_t allowance=0; inline std::vector<uint8_t> wire; }
inline int uart_param_config(int,const uart_config_t*) { return ESP_OK; }
inline int uart_set_pin(int,int,int,int,int) { return ESP_OK; }
inline int uart_driver_install(int,int,int,int,QueueHandle_t* events,int) {
    *events=xQueueCreate(8,sizeof(uart_event_t)); return ESP_OK;
}
inline int uart_tx_chars(int,const char* bytes,size_t count) {
    count=std::min(count,mock_uart::allowance);
    mock_uart::wire.insert(mock_uart::wire.end(),bytes,bytes+count);
    return static_cast<int>(count);
}
inline int uart_read_bytes(int,void*,size_t,int) { return 0; }
inline int uart_flush_input(int) { return ESP_OK; }
