/*****************************************************************
 * File:      uart_perf_test_impl.hpp
 * Category:  testing/experimentation
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    UART performance test implementation for bidirectional
 *    throughput measurement between CPU (Arduino) and GPU (ESP-IDF)
 *    at 2Mbaud. Platform-specific code in each environment.
 *****************************************************************/

#include "uart_perf_test.hpp"

#if defined(ARDUINO)
#include <Arduino.h>
#include "driver/uart.h"
#include "esp_timer.h"
namespace arcos::testing{

void initUart(uint32_t baud){
  // CPU: TX=GPIO12, RX=GPIO11 using ESP-IDF UART driver
  const uart_port_t uart_num = UART_NUM_2;
  uart_config_t uart_config = {};
  uart_config.baud_rate = baud;
  uart_config.data_bits = UART_DATA_8_BITS;
  uart_config.parity = UART_PARITY_DISABLE;
  uart_config.stop_bits = UART_STOP_BITS_1;
  uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uart_config.rx_flow_ctrl_thresh = 122;
  #ifdef UART_SCLK_DEFAULT
  uart_config.source_clk = UART_SCLK_DEFAULT;
  #endif
  
  uart_param_config(uart_num, &uart_config);
  uart_set_pin(uart_num, 12, 11, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE); // TX=12, RX=11
  uart_driver_install(uart_num, 8192, 8192, 0, NULL, 0);
}

UartPerfResult runUartPerfTest(uint32_t duration_ms, int direction, uint16_t packet_size){
  UartPerfResult result;
  const uart_port_t uart_num = UART_NUM_2;
  uint8_t* buf = (uint8_t*)malloc(packet_size);
  if(!buf) return result;
  memset(buf, 0xA5, packet_size);
  int64_t start = esp_timer_get_time();
  
  if(direction == 0){
    // CPU->GPU: send only
    while((esp_timer_get_time() - start) < duration_ms * 1000LL){
      int written = uart_write_bytes(uart_num, (const char*)buf, packet_size);
      if(written > 0){
        result.bytes_sent += written;
      }
      uart_wait_tx_done(uart_num, 0); // Non-blocking flush
    }
  }else{
    // GPU->CPU: receive only
    while((esp_timer_get_time() - start) < duration_ms * 1000LL){
      int len = uart_read_bytes(uart_num, buf, packet_size, 5 / portTICK_PERIOD_MS);
      if(len > 0){
        result.bytes_received += len;
      }
    }
  }
  result.mbps = (result.bytes_sent * 8.0f) / (duration_ms * 1000.0f);
  free(buf);
  return result;
}

} // namespace arcos::testing


#elif defined(ESP_PLATFORM)
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <string.h>
namespace arcos::testing{

void initUart(uint32_t baud){
  const uart_port_t uart_num = UART_NUM_2;
  uart_config_t uart_config = {};
  uart_config.baud_rate = baud;
  uart_config.data_bits = UART_DATA_8_BITS;
  uart_config.parity = UART_PARITY_DISABLE;
  uart_config.stop_bits = UART_STOP_BITS_1;
  uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uart_config.rx_flow_ctrl_thresh = 122;
  uart_config.source_clk = UART_SCLK_DEFAULT;
  
  uart_param_config(uart_num, &uart_config);
  uart_set_pin(uart_num, 12, 13, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE); // GPU: TX=12, RX=13
  uart_driver_install(uart_num, 8192, 8192, 0, NULL, 0); // Larger buffers
}

UartPerfResult runUartPerfTest(uint32_t duration_ms, int direction, uint16_t packet_size){
  UartPerfResult result;
  const uart_port_t uart_num = UART_NUM_2;
  uint8_t* buf = (uint8_t*)malloc(packet_size);
  if(!buf) return result;
  memset(buf, 0xA5, packet_size);
  int64_t start = esp_timer_get_time();
  
  if(direction == 0){
    // CPU->GPU: receive only
    while((esp_timer_get_time() - start) < duration_ms * 1000LL){
      int len = uart_read_bytes(uart_num, buf, packet_size, 5 / portTICK_PERIOD_MS);
      if(len > 0){
        result.bytes_received += len;
      }
    }
  }else{
    // GPU->CPU: send only
    while((esp_timer_get_time() - start) < duration_ms * 1000LL){
      int written = uart_write_bytes(uart_num, (const char*)buf, packet_size);
      if(written > 0){
        result.bytes_sent += written;
      }
      uart_wait_tx_done(uart_num, 0); // Non-blocking flush
    }
  }
  result.mbps = (result.bytes_sent * 8.0f) / (duration_ms * 1000.0f);
  free(buf);
  return result;
}

} // namespace arcos::testing
#endif
