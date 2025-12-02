/*****************************************************************
 * File:      uart_perf_test.hpp
 * Category:  testing/experimentation
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    UART performance test interface for bidirectional throughput
 *    measurement between CPU (Arduino) and GPU (ESP-IDF) at 2Mbaud.
 *****************************************************************/

#ifndef UART_PERF_TEST_HPP_
#define UART_PERF_TEST_HPP_

#include <stdint.h>

namespace arcos::testing{

/** UART performance test result structure */
struct UartPerfResult{
  uint32_t bytes_sent = 0;
  uint32_t bytes_received = 0;
  float mbps = 0.0f;
};

/** Initialize UART for performance test
 * @param baud UART baud rate
 */
void initUart(uint32_t baud);

/** Run a bidirectional UART performance test
 * @param duration_ms Test duration in milliseconds
 * @param direction 0: CPU->GPU, 1: GPU->CPU
 * @param packet_size Size of packets to send/receive in bytes
 * @return UartPerfResult with stats
 */
UartPerfResult runUartPerfTest(uint32_t duration_ms, int direction, uint16_t packet_size = 1024);

} // namespace arcos::testing

#endif // UART_PERF_TEST_HPP_
