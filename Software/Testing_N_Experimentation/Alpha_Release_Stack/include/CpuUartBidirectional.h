/*****************************************************************
 * File:      CpuUartBidirectional.h
 * Category:  communication/implementations
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    CPU-side UART bidirectional communication class header.
 *    Sends 316 bits (40 bytes) to GPU at 60Hz.
 *    Receives 1568 bits (196 bytes) from GPU at 60Hz.
 *****************************************************************/

#ifndef ARCOS_COMMUNICATION_CPU_UART_BIDIRECTIONAL_H_
#define ARCOS_COMMUNICATION_CPU_UART_BIDIRECTIONAL_H_

#include "driver/uart.h"
#include "UartBidirectionalProtocol.h"

namespace arcos::communication{

/** CPU UART Configuration */
constexpr int CPU_RX_PIN = 11;
constexpr int CPU_TX_PIN = 12;
constexpr uart_port_t CPU_UART_NUM = UART_NUM_2;

/** Data size configuration */
constexpr int CPU_SEND_BYTES = 40;   // 316 bits rounded up to 40 bytes
constexpr int CPU_RECV_BYTES = 196;  // 1568 bits = 196 bytes
constexpr int TARGET_FPS = 60;
constexpr int FRAME_TIME_MS = 1000 / TARGET_FPS;

/** Analytics structure */
struct Analytics{
  uint32_t frames_sent = 0;
  uint32_t frames_received = 0;
  uint32_t packets_lost = 0;
  uint32_t checksum_errors = 0;
  uint32_t timeout_errors = 0;
  uint32_t total_bytes_sent = 0;
  uint32_t total_bytes_received = 0;
  unsigned long start_time = 0;
  unsigned long last_report_time = 0;
  uint32_t expected_sequence = 0;
};

/** CPU-side UART bidirectional implementation */
class CpuUartBidirectional : public IUartBidirectional{
public:
  CpuUartBidirectional();
  
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
  unsigned long last_frame_time_;
  Analytics analytics_;
  
  bool sendDataFrame();
  void printAnalytics();
  void handleReceivedPacket(const UartPacket& packet);
};

} // namespace arcos::communication

#endif // ARCOS_COMMUNICATION_CPU_UART_BIDIRECTIONAL_H_
