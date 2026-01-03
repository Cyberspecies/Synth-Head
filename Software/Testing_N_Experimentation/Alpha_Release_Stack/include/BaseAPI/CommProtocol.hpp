/*****************************************************************
 * File:      CommProtocol.hpp
 * Category:  include/BaseAPI
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Bidirectional communication protocol for CPU<->GPU data transfer.
 *    Handles:
 *    - Frame data (display buffers)
 *    - Telemetry data (sensor state)
 *    - Commands (mode changes, settings)
 *    - Synchronization and flow control
 * 
 * Protocol:
 *    [SYNC][TYPE][SEQ][LEN:2][PAYLOAD][CRC16]
 *    - SYNC: 0xAA 0x55 (2 bytes)
 *    - TYPE: Packet type (1 byte)
 *    - SEQ: Sequence number (1 byte)
 *    - LEN: Payload length (2 bytes, little-endian)
 *    - PAYLOAD: Variable length data
 *    - CRC16: CRC-16-CCITT (2 bytes)
 * 
 * Layer:
 *    HAL Layer -> [Base System API - Comm] -> Application
 *****************************************************************/

#ifndef ARCOS_INCLUDE_BASEAPI_COMM_PROTOCOL_HPP_
#define ARCOS_INCLUDE_BASEAPI_COMM_PROTOCOL_HPP_

#include "BaseTypes.hpp"
#include "Telemetry.hpp"
#include <cstring>

namespace arcos::base{

// ============================================================
// Protocol Constants
// ============================================================

namespace protocol{
  constexpr uint8_t SYNC_BYTE_1 = 0xAA;
  constexpr uint8_t SYNC_BYTE_2 = 0x55;
  constexpr uint16_t MAX_PAYLOAD_SIZE = 4096;  // Max payload bytes
  constexpr uint16_t HEADER_SIZE = 6;          // Sync(2) + Type(1) + Seq(1) + Len(2)
  constexpr uint16_t FOOTER_SIZE = 2;          // CRC16
  constexpr uint32_t UART_BAUD_RATE = 10000000;  // 10 Mbps
  constexpr uint32_t ACK_TIMEOUT_MS = 100;
  constexpr uint8_t MAX_RETRIES = 3;
}

// ============================================================
// Packet Types
// ============================================================

enum class PacketType : uint8_t{
  // System packets (0x00-0x0F)
  PING          = 0x01,  // Request response
  PONG          = 0x02,  // Response to ping
  HEARTBEAT     = 0x03,  // Keep-alive
  ACK           = 0x04,  // Acknowledge receipt
  NACK          = 0x05,  // Negative acknowledge
  RESET         = 0x06,  // Reset request
  
  // Configuration (0x10-0x1F)
  CONFIG_SET    = 0x10,  // Set configuration
  CONFIG_GET    = 0x11,  // Get configuration
  CONFIG_RESP   = 0x12,  // Configuration response
  MODE_CHANGE   = 0x13,  // Change operating mode
  
  // Telemetry (0x20-0x2F)
  TELEMETRY     = 0x20,  // Full telemetry packet
  MOTION_STATE  = 0x21,  // Motion state only
  ENV_STATE     = 0x22,  // Environmental state only
  LOCATION      = 0x23,  // GPS location only
  AUDIO_STATE   = 0x24,  // Audio state only
  
  // Display (0x30-0x3F)
  FRAME_START   = 0x30,  // Start of frame transfer
  FRAME_DATA    = 0x31,  // Frame data chunk
  FRAME_END     = 0x32,  // End of frame transfer
  FRAME_SYNC    = 0x33,  // Frame synchronization
  DISPLAY_CMD   = 0x34,  // Display command
  
  // LED Control (0x40-0x4F)
  LED_FRAME     = 0x40,  // LED strip frame data
  LED_CMD       = 0x41,  // LED command
  LED_PATTERN   = 0x42,  // LED pattern preset
  
  // Input (0x50-0x5F)
  BUTTON_EVENT  = 0x50,  // Button press/release
  INPUT_STATE   = 0x51,  // Full input state
  
  // Status (0xF0-0xFF)
  ERROR         = 0xF0,  // Error notification
  STATUS        = 0xF1,  // Status report
  DEBUG         = 0xFE,  // Debug message
};

// ============================================================
// Packet Header
// ============================================================

#pragma pack(push, 1)

/** Packet header structure */
struct PacketHeader{
  uint8_t sync1;        // SYNC_BYTE_1
  uint8_t sync2;        // SYNC_BYTE_2
  uint8_t type;         // PacketType
  uint8_t sequence;     // Sequence number
  uint16_t length;      // Payload length (little-endian)
};

/** Frame transfer header (inside FRAME_DATA packets) */
struct FrameHeader{
  uint16_t frame_id;    // Frame identifier
  uint16_t width;       // Frame width
  uint16_t height;      // Frame height
  uint8_t format;       // Color format (0=RGB565, 1=RGB888)
  uint8_t chunk_index;  // Chunk index (for multi-packet frames)
  uint8_t chunk_count;  // Total chunks
  uint8_t flags;        // Frame flags
};

/** LED frame header */
struct LedFrameHeader{
  uint8_t strip_id;     // Which strip (0=left, 1=right, 2=tongue, 3=scale)
  uint8_t led_count;    // Number of LEDs in this packet
  uint8_t format;       // 0=RGB, 1=RGBW
  uint8_t flags;        // Flags (bit 0: show immediately)
};

/** Telemetry packet (compact version) */
struct TelemetryPacket{
  // Timestamp
  uint32_t timestamp_ms;
  uint32_t frame_number;
  
  // Orientation (quaternion as int16, scaled by 10000)
  int16_t quat_w;
  int16_t quat_x;
  int16_t quat_y;
  int16_t quat_z;
  
  // Euler angles (int16, scaled by 100, in degrees)
  int16_t roll_deg;
  int16_t pitch_deg;
  int16_t yaw_deg;
  
  // Linear acceleration (int16, scaled by 1000, m/s²)
  int16_t accel_x;
  int16_t accel_y;
  int16_t accel_z;
  
  // Angular velocity (int16, scaled by 100, deg/s)
  int16_t gyro_x;
  int16_t gyro_y;
  int16_t gyro_z;
  
  // Environmental (int16)
  int16_t temperature_c10;  // Celsius * 10
  uint16_t humidity_pct10;  // Percent * 10
  uint16_t pressure_pa10;   // Pascals / 10
  
  // GPS (int32 for precision)
  int32_t latitude_e7;      // Latitude * 1e7
  int32_t longitude_e7;     // Longitude * 1e7
  int16_t altitude_dm;      // Altitude in decimeters
  uint16_t speed_cm_s;      // Speed in cm/s
  uint16_t heading_deg10;   // Heading * 10
  uint8_t satellites;
  
  // Audio
  int8_t db_level;          // dB level (-128 to 127)
  uint8_t rms_level_pct;    // RMS as percentage
  
  // Status flags
  uint8_t status_flags;     // Bit flags for sensor status
};

/** Button event packet */
struct ButtonEventPacket{
  uint8_t button_id;     // Button identifier
  uint8_t event_type;    // 0=release, 1=press, 2=hold, 3=double-click
  uint32_t timestamp;    // Event timestamp
};

/** Error packet */
struct ErrorPacket{
  uint8_t error_code;    // Error code
  uint8_t severity;      // 0=info, 1=warning, 2=error, 3=fatal
  char message[60];      // Error message (null-terminated)
};

#pragma pack(pop)

// ============================================================
// CRC-16-CCITT
// ============================================================

/** Calculate CRC-16-CCITT */
inline uint16_t calculateCRC16(const uint8_t* data, size_t length){
  uint16_t crc = 0xFFFF;
  for(size_t i = 0; i < length; i++){
    crc ^= (uint16_t)data[i] << 8;
    for(int j = 0; j < 8; j++){
      if(crc & 0x8000){
        crc = (crc << 1) ^ 0x1021;
      }else{
        crc <<= 1;
      }
    }
  }
  return crc;
}

// ============================================================
// Packet Builder
// ============================================================

/**
 * PacketBuilder - Helper to construct packets
 */
class PacketBuilder{
public:
  PacketBuilder(uint8_t* buffer, size_t buffer_size)
    : buffer_(buffer)
    , buffer_size_(buffer_size)
    , write_pos_(0)
    , sequence_(0)
  {}
  
  /** Start building a new packet */
  bool begin(PacketType type){
    if(buffer_size_ < protocol::HEADER_SIZE + protocol::FOOTER_SIZE){
      return false;
    }
    
    write_pos_ = 0;
    buffer_[write_pos_++] = protocol::SYNC_BYTE_1;
    buffer_[write_pos_++] = protocol::SYNC_BYTE_2;
    buffer_[write_pos_++] = static_cast<uint8_t>(type);
    buffer_[write_pos_++] = sequence_++;
    write_pos_ += 2;  // Reserve space for length
    
    return true;
  }
  
  /** Add data to payload */
  bool addData(const void* data, size_t length){
    if(write_pos_ + length > buffer_size_ - protocol::FOOTER_SIZE){
      return false;
    }
    memcpy(buffer_ + write_pos_, data, length);
    write_pos_ += length;
    return true;
  }
  
  /** Add a single byte */
  bool addByte(uint8_t byte){
    return addData(&byte, 1);
  }
  
  /** Add a 16-bit value (little-endian) */
  bool addU16(uint16_t value){
    uint8_t data[2] = { (uint8_t)(value & 0xFF), (uint8_t)(value >> 8) };
    return addData(data, 2);
  }
  
  /** Add a 32-bit value (little-endian) */
  bool addU32(uint32_t value){
    uint8_t data[4] = {
      (uint8_t)(value & 0xFF),
      (uint8_t)((value >> 8) & 0xFF),
      (uint8_t)((value >> 16) & 0xFF),
      (uint8_t)((value >> 24) & 0xFF)
    };
    return addData(data, 4);
  }
  
  /** Add telemetry data */
  bool addTelemetry(const TelemetryData& telem){
    TelemetryPacket pkt;
    
    pkt.timestamp_ms = telem.timestamp;
    pkt.frame_number = telem.frame_number;
    
    // Convert quaternion (float to int16 * 10000)
    pkt.quat_w = (int16_t)(telem.motion.orientation.w * 10000.0f);
    pkt.quat_x = (int16_t)(telem.motion.orientation.x * 10000.0f);
    pkt.quat_y = (int16_t)(telem.motion.orientation.y * 10000.0f);
    pkt.quat_z = (int16_t)(telem.motion.orientation.z * 10000.0f);
    
    // Convert euler to degrees * 100
    pkt.roll_deg = (int16_t)(telem.motion.euler.x * math::RAD_TO_DEG * 100.0f);
    pkt.pitch_deg = (int16_t)(telem.motion.euler.y * math::RAD_TO_DEG * 100.0f);
    pkt.yaw_deg = (int16_t)(telem.motion.euler.z * math::RAD_TO_DEG * 100.0f);
    
    // Linear accel * 1000
    pkt.accel_x = (int16_t)(telem.motion.linear_acceleration.x * 1000.0f);
    pkt.accel_y = (int16_t)(telem.motion.linear_acceleration.y * 1000.0f);
    pkt.accel_z = (int16_t)(telem.motion.linear_acceleration.z * 1000.0f);
    
    // Angular vel (rad/s to deg/s * 100)
    pkt.gyro_x = (int16_t)(telem.motion.angular_velocity.x * math::RAD_TO_DEG * 100.0f);
    pkt.gyro_y = (int16_t)(telem.motion.angular_velocity.y * math::RAD_TO_DEG * 100.0f);
    pkt.gyro_z = (int16_t)(telem.motion.angular_velocity.z * math::RAD_TO_DEG * 100.0f);
    
    // Environmental
    pkt.temperature_c10 = (int16_t)(telem.environment.temperature * 10.0f);
    pkt.humidity_pct10 = (uint16_t)(telem.environment.humidity * 10.0f);
    pkt.pressure_pa10 = (uint16_t)(telem.environment.pressure / 10.0f);
    
    // GPS
    pkt.latitude_e7 = (int32_t)(telem.location.latitude * 1e7);
    pkt.longitude_e7 = (int32_t)(telem.location.longitude * 1e7);
    pkt.altitude_dm = (int16_t)(telem.location.altitude * 10.0f);
    pkt.speed_cm_s = (uint16_t)(telem.location.speed * 100.0f);
    pkt.heading_deg10 = (uint16_t)(telem.location.heading * 10.0f);
    pkt.satellites = telem.location.satellites;
    
    // Audio
    pkt.db_level = (int8_t)telem.audio.db_level;
    pkt.rms_level_pct = (uint8_t)(telem.audio.rms_level * 100.0f);
    
    // Status
    pkt.status_flags = 
      (telem.imu_ok ? 0x01 : 0) |
      (telem.env_ok ? 0x02 : 0) |
      (telem.gps_ok ? 0x04 : 0) |
      (telem.mic_ok ? 0x08 : 0) |
      (telem.motion.is_calibrated ? 0x10 : 0) |
      (telem.motion.is_stable ? 0x20 : 0) |
      (telem.location.has_fix ? 0x40 : 0);
    
    return addData(&pkt, sizeof(pkt));
  }
  
  /** Finalize packet (add length and CRC) */
  size_t finalize(){
    // Calculate payload length
    uint16_t payload_len = write_pos_ - protocol::HEADER_SIZE;
    buffer_[4] = payload_len & 0xFF;
    buffer_[5] = (payload_len >> 8) & 0xFF;
    
    // Calculate CRC over header + payload (excluding sync bytes)
    uint16_t crc = calculateCRC16(buffer_ + 2, write_pos_ - 2);
    buffer_[write_pos_++] = crc & 0xFF;
    buffer_[write_pos_++] = (crc >> 8) & 0xFF;
    
    return write_pos_;
  }
  
  /** Get current write position */
  size_t size() const{ return write_pos_; }
  
  /** Get buffer pointer */
  const uint8_t* data() const{ return buffer_; }

private:
  uint8_t* buffer_;
  size_t buffer_size_;
  size_t write_pos_;
  uint8_t sequence_;
};

// ============================================================
// Packet Parser
// ============================================================

/**
 * PacketParser - State machine for parsing incoming packets
 */
class PacketParser{
public:
  enum class State{
    SYNC1,
    SYNC2,
    TYPE,
    SEQ,
    LEN_LOW,
    LEN_HIGH,
    PAYLOAD,
    CRC_LOW,
    CRC_HIGH
  };
  
  PacketParser(uint8_t* buffer, size_t buffer_size)
    : buffer_(buffer)
    , buffer_size_(buffer_size)
    , state_(State::SYNC1)
    , payload_length_(0)
    , payload_index_(0)
  {}
  
  /** Feed a single byte, returns true when packet is complete */
  bool feed(uint8_t byte){
    switch(state_){
      case State::SYNC1:
        if(byte == protocol::SYNC_BYTE_1){
          state_ = State::SYNC2;
        }
        break;
        
      case State::SYNC2:
        if(byte == protocol::SYNC_BYTE_2){
          state_ = State::TYPE;
        }else{
          state_ = State::SYNC1;
        }
        break;
        
      case State::TYPE:
        packet_type_ = byte;
        state_ = State::SEQ;
        break;
        
      case State::SEQ:
        sequence_ = byte;
        state_ = State::LEN_LOW;
        break;
        
      case State::LEN_LOW:
        payload_length_ = byte;
        state_ = State::LEN_HIGH;
        break;
        
      case State::LEN_HIGH:
        payload_length_ |= ((uint16_t)byte << 8);
        if(payload_length_ > buffer_size_){
          state_ = State::SYNC1;  // Invalid length
        }else if(payload_length_ == 0){
          state_ = State::CRC_LOW;
        }else{
          payload_index_ = 0;
          state_ = State::PAYLOAD;
        }
        break;
        
      case State::PAYLOAD:
        buffer_[payload_index_++] = byte;
        if(payload_index_ >= payload_length_){
          state_ = State::CRC_LOW;
        }
        break;
        
      case State::CRC_LOW:
        crc_received_ = byte;
        state_ = State::CRC_HIGH;
        break;
        
      case State::CRC_HIGH:
        crc_received_ |= ((uint16_t)byte << 8);
        state_ = State::SYNC1;  // Reset for next packet
        return validatePacket();
    }
    
    return false;
  }
  
  /** Get packet type */
  PacketType getType() const{ return static_cast<PacketType>(packet_type_); }
  
  /** Get sequence number */
  uint8_t getSequence() const{ return sequence_; }
  
  /** Get payload pointer */
  const uint8_t* getPayload() const{ return buffer_; }
  
  /** Get payload length */
  uint16_t getPayloadLength() const{ return payload_length_; }
  
  /** Parse telemetry from payload */
  bool parseTelemetry(TelemetryData& telem) const{
    if(payload_length_ < sizeof(TelemetryPacket)) return false;
    
    const TelemetryPacket* pkt = reinterpret_cast<const TelemetryPacket*>(buffer_);
    
    telem.timestamp = pkt->timestamp_ms;
    telem.frame_number = pkt->frame_number;
    
    // Quaternion
    telem.motion.orientation.w = pkt->quat_w / 10000.0f;
    telem.motion.orientation.x = pkt->quat_x / 10000.0f;
    telem.motion.orientation.y = pkt->quat_y / 10000.0f;
    telem.motion.orientation.z = pkt->quat_z / 10000.0f;
    
    // Euler
    telem.motion.euler.x = pkt->roll_deg / 100.0f * math::DEG_TO_RAD;
    telem.motion.euler.y = pkt->pitch_deg / 100.0f * math::DEG_TO_RAD;
    telem.motion.euler.z = pkt->yaw_deg / 100.0f * math::DEG_TO_RAD;
    
    // Acceleration
    telem.motion.linear_acceleration.x = pkt->accel_x / 1000.0f;
    telem.motion.linear_acceleration.y = pkt->accel_y / 1000.0f;
    telem.motion.linear_acceleration.z = pkt->accel_z / 1000.0f;
    
    // Angular velocity
    telem.motion.angular_velocity.x = pkt->gyro_x / 100.0f * math::DEG_TO_RAD;
    telem.motion.angular_velocity.y = pkt->gyro_y / 100.0f * math::DEG_TO_RAD;
    telem.motion.angular_velocity.z = pkt->gyro_z / 100.0f * math::DEG_TO_RAD;
    
    // Environmental
    telem.environment.temperature = pkt->temperature_c10 / 10.0f;
    telem.environment.humidity = pkt->humidity_pct10 / 10.0f;
    telem.environment.pressure = pkt->pressure_pa10 * 10.0f;
    
    // GPS
    telem.location.latitude = pkt->latitude_e7 / 1e7;
    telem.location.longitude = pkt->longitude_e7 / 1e7;
    telem.location.altitude = pkt->altitude_dm / 10.0f;
    telem.location.speed = pkt->speed_cm_s / 100.0f;
    telem.location.heading = pkt->heading_deg10 / 10.0f;
    telem.location.satellites = pkt->satellites;
    
    // Audio
    telem.audio.db_level = pkt->db_level;
    telem.audio.rms_level = pkt->rms_level_pct / 100.0f;
    
    // Status
    telem.imu_ok = pkt->status_flags & 0x01;
    telem.env_ok = pkt->status_flags & 0x02;
    telem.gps_ok = pkt->status_flags & 0x04;
    telem.mic_ok = pkt->status_flags & 0x08;
    telem.motion.is_calibrated = pkt->status_flags & 0x10;
    telem.motion.is_stable = pkt->status_flags & 0x20;
    telem.location.has_fix = pkt->status_flags & 0x40;
    
    return true;
  }
  
  /** Reset parser state */
  void reset(){
    state_ = State::SYNC1;
    payload_index_ = 0;
    payload_length_ = 0;
  }

private:
  uint8_t* buffer_;
  size_t buffer_size_;
  State state_;
  
  uint8_t packet_type_;
  uint8_t sequence_;
  uint16_t payload_length_;
  uint16_t payload_index_;
  uint16_t crc_received_;
  
  /** Validate packet CRC */
  bool validatePacket(){
    // Build header for CRC calculation
    uint8_t header[4] = {
      packet_type_,
      sequence_,
      (uint8_t)(payload_length_ & 0xFF),
      (uint8_t)(payload_length_ >> 8)
    };
    
    // Calculate CRC over header + payload
    uint16_t crc = 0xFFFF;
    
    // CRC header
    for(int i = 0; i < 4; i++){
      crc ^= (uint16_t)header[i] << 8;
      for(int j = 0; j < 8; j++){
        if(crc & 0x8000) crc = (crc << 1) ^ 0x1021;
        else crc <<= 1;
      }
    }
    
    // CRC payload
    for(uint16_t i = 0; i < payload_length_; i++){
      crc ^= (uint16_t)buffer_[i] << 8;
      for(int j = 0; j < 8; j++){
        if(crc & 0x8000) crc = (crc << 1) ^ 0x1021;
        else crc <<= 1;
      }
    }
    
    return crc == crc_received_;
  }
};

// ============================================================
// Communication Manager Interface
// ============================================================

/**
 * ICommManager - Interface for CPU-GPU communication
 * 
 * Abstracts the communication channel, allowing different
 * implementations (UART, SPI, etc.) to be swapped.
 */
class ICommManager{
public:
  virtual ~ICommManager() = default;
  
  /** Initialize communication */
  virtual Result init() = 0;
  
  /** Process incoming data */
  virtual void update() = 0;
  
  /** Send a packet */
  virtual Result sendPacket(PacketType type, const void* data, size_t length) = 0;
  
  /** Send telemetry data */
  virtual Result sendTelemetry(const TelemetryData& telem) = 0;
  
  /** Send frame data (for display) */
  virtual Result sendFrame(uint16_t width, uint16_t height, 
                          const uint16_t* rgb565_data) = 0;
  
  /** Send LED data */
  virtual Result sendLedFrame(uint8_t strip_id, const Color* colors, 
                             uint8_t count, bool show = true) = 0;
  
  /** Check if connected */
  virtual bool isConnected() const = 0;
  
  /** Get last received telemetry */
  virtual const TelemetryData* getReceivedTelemetry() const = 0;
  
  /** Register callback for received packets */
  using PacketCallback = void(*)(PacketType type, const uint8_t* data, 
                                 uint16_t length, void* user);
  virtual void setPacketCallback(PacketCallback callback, void* user) = 0;
};

} // namespace arcos::base

#endif // ARCOS_INCLUDE_BASEAPI_COMM_PROTOCOL_HPP_
