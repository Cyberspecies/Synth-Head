/*****************************************************************
 * File:      IHalGps.hpp
 * Category:  include/HAL
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    GPS Sensor Hardware Abstraction Layer interface.
 *    Provides platform-independent access to GPS modules
 *    like NEO-8M for position, time, and velocity data.
 * 
 * Note:
 *    This is a sensor HAL interface. The middleware layer will
 *    use this to build higher-level telemetry/navigation services.
 *****************************************************************/

#ifndef ARCOS_INCLUDE_HAL_IHAL_GPS_HPP_
#define ARCOS_INCLUDE_HAL_IHAL_GPS_HPP_

#include "HalTypes.hpp"

namespace arcos::hal{

// ============================================================
// GPS Data Structures
// ============================================================

/** GPS fix quality */
enum class GpsFixQuality : uint8_t{
  NO_FIX = 0,
  GPS_FIX = 1,
  DGPS_FIX = 2,
  PPS_FIX = 3,
  RTK_FIXED = 4,
  RTK_FLOAT = 5,
  ESTIMATED = 6
};

/** GPS time data */
struct GpsTime{
  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
  uint16_t millisecond = 0;
  bool valid = false;
};

/** GPS position data */
struct GpsPosition{
  double latitude = 0.0;         // Decimal degrees (positive = North)
  double longitude = 0.0;        // Decimal degrees (positive = East)
  float altitude = 0.0f;         // Meters above sea level
  float hdop = 99.99f;           // Horizontal dilution of precision
  float vdop = 99.99f;           // Vertical dilution of precision
  bool valid = false;
};

/** GPS velocity data */
struct GpsVelocity{
  float speed_knots = 0.0f;      // Speed in knots
  float speed_kmh = 0.0f;        // Speed in km/h
  float speed_mps = 0.0f;        // Speed in m/s
  float course = 0.0f;           // Course over ground in degrees
  bool valid = false;
};

/** Complete GPS data */
struct GpsData{
  GpsPosition position;
  GpsVelocity velocity;
  GpsTime time;
  
  GpsFixQuality fix_quality = GpsFixQuality::NO_FIX;
  uint8_t satellites_used = 0;
  uint8_t satellites_visible = 0;
  
  // Timestamp when data was last updated
  timestamp_ms_t timestamp = 0;
  
  /** Check if GPS has valid fix */
  bool hasFix() const{
    return fix_quality != GpsFixQuality::NO_FIX && position.valid;
  }
};

/** GPS configuration */
struct GpsConfig{
  gpio_pin_t tx_pin = 0;
  gpio_pin_t rx_pin = 0;
  uint32_t baud_rate = 9600;
  uint16_t update_rate_ms = 1000;  // Update rate in ms
};

// ============================================================
// GPS Interface
// ============================================================

/** GPS Hardware Abstraction Interface
 * 
 * Provides platform-independent access to GPS modules.
 * Handles NMEA parsing and provides structured data.
 */
class IHalGps{
public:
  virtual ~IHalGps() = default;
  
  /** Initialize GPS module
   * @param config GPS configuration
   * @return HalResult::OK on success
   */
  virtual HalResult init(const GpsConfig& config) = 0;
  
  /** Deinitialize GPS module
   * @return HalResult::OK on success
   */
  virtual HalResult deinit() = 0;
  
  /** Check if GPS is initialized
   * @return true if initialized
   */
  virtual bool isInitialized() const = 0;
  
  /** Update GPS data (call frequently in main loop)
   * @return HalResult::OK if new data processed
   */
  virtual HalResult update() = 0;
  
  /** Get complete GPS data
   * @param data Reference to GpsData structure to fill
   * @return HalResult::OK on success
   */
  virtual HalResult getData(GpsData& data) = 0;
  
  /** Get position only
   * @param position Reference to GpsPosition to fill
   * @return HalResult::OK on success
   */
  virtual HalResult getPosition(GpsPosition& position) = 0;
  
  /** Get velocity only
   * @param velocity Reference to GpsVelocity to fill
   * @return HalResult::OK on success
   */
  virtual HalResult getVelocity(GpsVelocity& velocity) = 0;
  
  /** Get time only
   * @param time Reference to GpsTime to fill
   * @return HalResult::OK on success
   */
  virtual HalResult getTime(GpsTime& time) = 0;
  
  /** Check if GPS has valid fix
   * @return true if fix is valid
   */
  virtual bool hasFix() const = 0;
  
  /** Get fix quality
   * @return Current fix quality
   */
  virtual GpsFixQuality getFixQuality() const = 0;
  
  /** Get number of satellites used
   * @return Satellites used for fix
   */
  virtual uint8_t getSatellites() const = 0;
  
  /** Calculate distance to another position
   * @param lat Target latitude in decimal degrees
   * @param lon Target longitude in decimal degrees
   * @return Distance in meters
   */
  virtual float distanceTo(double lat, double lon) = 0;
  
  /** Calculate bearing to another position
   * @param lat Target latitude in decimal degrees
   * @param lon Target longitude in decimal degrees
   * @return Bearing in degrees (0-360)
   */
  virtual float bearingTo(double lat, double lon) = 0;
};

} // namespace arcos::hal

#endif // ARCOS_INCLUDE_HAL_IHAL_GPS_HPP_
