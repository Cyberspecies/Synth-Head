/*****************************************************************
 * File:      GpuUartBidirectional.h
 * Category:  communication/implementations
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    GPU-side UART bidirectional communication class header.
 *    Sends 1568 bits (196 bytes) to CPU at 60Hz.
 *    Receives 316 bits (40 bytes) from CPU at 60Hz.
 *****************************************************************/

#ifndef ARCOS_COMMUNICATION_GPU_UART_BIDIRECTIONAL_H_
#define ARCOS_COMMUNICATION_GPU_UART_BIDIRECTIONAL_H_

#include "driver/uart.h"
#include "UartBidirectionalProtocol.h"

namespace arcos::communication{

/** GPU UART Configuration */
constexpr int GPU_TX_PIN = 12;
constexpr int GPU_RX_PIN = 13;
constexpr uart_port_t GPU_UART_NUM = UART_NUM_1;

/** Data size configuration */
constexpr int GPU_SEND_BYTES = 196;  // 1568 bits = 196 bytes
constexpr int GPU_RECV_BYTES = 40;   // 316 bits rounded up to 40 bytes
constexpr int GPU_TARGET_FPS = 60;
constexpr int GPU_FRAME_TIME_MS = 1000 / GPU_TARGET_FPS;

/** Analytics structure */
struct GpuAnalytics{
  uint32_t frames_sent;
  uint32_t frames_received;
  uint32_t packets_lost;
  uint32_t checksum_errors;
  uint32_t timeout_errors;
  uint32_t total_bytes_sent;
  uint32_t total_bytes_received;
  uint32_t start_time;
  uint32_t last_report_time;
  uint32_t expected_sequence;
};

/** GPU-side UART bidirectional implementation */
class GpuUartBidirectional : public IUartBidirectional{
public:
  GpuUartBidirectional();
  
  bool init(int baud_rate = BAUD_RATE) override;
  bool sendPacket(MessageType type, const uint8_t* payload, uint8_t length) override;
  bool receivePacket(UartPacket& packet) override;
  int available() override;
  bool sendPing() override;
  bool sendAck(uint8_t ack_data = 0) override;
  void update() override;
  
private:
  bool initialized_;
  uint32_t frame_counter_;
  uint32_t last_frame_time_;
  GpuAnalytics analytics_;
  
  bool sendDataFrame();
  void printAnalytics();
  void handleReceivedPacket(const UartPacket& packet);
};

} // namespace arcos::communication

#endif // ARCOS_COMMUNICATION_GPU_UART_BIDIRECTIONAL_H_
