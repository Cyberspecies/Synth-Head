/*****************************************************************
 * File:      Neo8mGps.h
 * Category:  drivers/sensors
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    NEO-8M GPS module wrapper with NMEA sentence parsing
 *    using UART serial communication for ESP32-S3.
 *****************************************************************/

#ifndef ALPHA_RELEASE_STACK_DRIVERS_SENSORS_NEO8M_GPS_H_
#define ALPHA_RELEASE_STACK_DRIVERS_SENSORS_NEO8M_GPS_H_

#include <Arduino.h>
#include <HardwareSerial.h>

namespace sensors{

/** GPS fix quality indicator */
enum class GpsFixQuality{
  NO_FIX = 0,
  GPS_FIX = 1,
  DGPS_FIX = 2
};

/** NEO-8M GPS data structure */
struct Neo8mGpsData{
  // Position
  float latitude;      // Decimal degrees (positive = North)
  float longitude;     // Decimal degrees (positive = East)
  float altitude;      // Meters above sea level
  
  // Fix information
  GpsFixQuality fix_quality;
  uint8_t satellites;  // Number of satellites in use
  
  // Time (UTC)
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  
  // Speed and course
  float speed_knots;   // Speed in knots
  float course;        // Course over ground in degrees
  
  // Validity
  bool valid;          // True if position data is valid
};

/** NEO-8M GPS module wrapper
 * 
 * Parses NMEA sentences (GGA, RMC) from NEO-8M GPS module
 * via UART serial communication. Provides simple interface
 * for GPS position, time, and fix information.
 */
class Neo8mGps{
private:
  static constexpr const char* TAG = "NEO8M";
  static constexpr uint32_t GPS_BAUD = 9600;
  static constexpr uint16_t BUFFER_SIZE = 128;
  
  HardwareSerial* serial_;
  uint8_t tx_pin_;
  uint8_t rx_pin_;
  bool initialized_;
  
  char buffer_[BUFFER_SIZE];
  uint16_t buffer_index_;
  
  Neo8mGpsData current_data_;

  /** Parse NMEA sentence
   * @param sentence NMEA sentence string
   * @return true if successfully parsed
   */
  bool parseNmeaSentence(const char* sentence);
  
  /** Parse GGA sentence (position and fix data)
   * @param sentence GGA sentence
   * @return true if successfully parsed
   */
  bool parseGGA(const char* sentence);
  
  /** Parse RMC sentence (recommended minimum data)
   * @param sentence RMC sentence
   * @return true if successfully parsed
   */
  bool parseRMC(const char* sentence);
  
  /** Convert NMEA coordinate to decimal degrees
   * @param nmea_coord NMEA coordinate (DDMM.MMMM or DDDMM.MMMM)
   * @param direction Direction character (N/S/E/W)
   * @return Decimal degrees
   */
  float nmeaToDecimal(float nmea_coord, char direction);
  
  /** Validate NMEA checksum
   * @param sentence NMEA sentence with checksum
   * @return true if checksum is valid
   */
  bool validateChecksum(const char* sentence);

public:
  /** Constructor with UART pins
   * @param tx_pin TX pin number
   * @param rx_pin RX pin number
   */
  Neo8mGps(uint8_t tx_pin, uint8_t rx_pin);

  /** Initialize UART communication
   * @return true if initialization successful
   */
  bool init();

  /** Check if GPS is initialized
   * @return true if initialized
   */
  bool isInitialized() const{ return initialized_; }

  /** Update GPS data by reading available serial data
   * Call this frequently in loop() to process incoming data
   * @return true if new valid data was received
   */
  bool update();

  /** Get current GPS data
   * @param data Reference to Neo8mGpsData structure to fill
   * @return true if data is valid
   */
  bool getData(Neo8mGpsData& data);

  /** Print GPS data to Serial
   * @param data GPS data to print
   */
  void printData(const Neo8mGpsData& data) const;
};

} // namespace sensors

// Include implementation
#include "Neo8mGps.impl.hpp"

#endif // ALPHA_RELEASE_STACK_DRIVERS_SENSORS_NEO8M_GPS_H_
