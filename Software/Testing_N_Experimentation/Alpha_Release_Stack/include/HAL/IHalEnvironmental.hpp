/*****************************************************************
 * File:      IHalEnvironmental.hpp
 * Category:  include/HAL
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Environmental Sensor Hardware Abstraction Layer interface.
 *    Provides platform-independent access to environmental sensors
 *    like BME280 for temperature, humidity, and pressure.
 * 
 * Note:
 *    This is a sensor HAL interface. The middleware layer will
 *    use this to build higher-level telemetry services.
 *****************************************************************/

#ifndef ARCOS_INCLUDE_HAL_IHAL_ENVIRONMENTAL_HPP_
#define ARCOS_INCLUDE_HAL_IHAL_ENVIRONMENTAL_HPP_

#include "HalTypes.hpp"

namespace arcos::hal{

// ============================================================
// Environmental Data Structures
// ============================================================

/** Environmental sensor data */
struct EnvironmentalData{
  float temperature = 0.0f;      // Temperature in °C
  float humidity = 0.0f;         // Relative humidity in %
  float pressure = 0.0f;         // Pressure in Pa (or hPa)
  
  // Timestamp when data was read
  timestamp_ms_t timestamp = 0;
  
  // Validity flags
  bool temperature_valid = false;
  bool humidity_valid = false;
  bool pressure_valid = false;
};

/** Environmental sensor configuration */
struct EnvironmentalConfig{
  i2c_addr_t address = 0x76;     // Default BME280 address
  
  // Oversampling settings (1, 2, 4, 8, 16)
  uint8_t temp_oversampling = 1;
  uint8_t humidity_oversampling = 1;
  uint8_t pressure_oversampling = 1;
  
  // Mode: 0 = sleep, 1 = forced, 3 = normal
  uint8_t mode = 3;
  
  // Standby time in ms (for normal mode)
  uint16_t standby_ms = 1000;
};

// ============================================================
// Environmental Sensor Interface
// ============================================================

/** Environmental Sensor Hardware Abstraction Interface
 * 
 * Provides platform-independent access to environmental sensors.
 * Supports temperature, humidity, and pressure measurements.
 */
class IHalEnvironmental{
public:
  virtual ~IHalEnvironmental() = default;
  
  /** Initialize environmental sensor
   * @param config Sensor configuration
   * @return HalResult::OK on success
   */
  virtual HalResult init(const EnvironmentalConfig& config) = 0;
  
  /** Deinitialize sensor
   * @return HalResult::OK on success
   */
  virtual HalResult deinit() = 0;
  
  /** Check if sensor is initialized
   * @return true if initialized
   */
  virtual bool isInitialized() const = 0;
  
  /** Read all environmental data
   * @param data Reference to EnvironmentalData structure to fill
   * @return HalResult::OK on success
   */
  virtual HalResult readAll(EnvironmentalData& data) = 0;
  
  /** Read temperature only
   * @param temperature Reference to store temperature (°C)
   * @return HalResult::OK on success
   */
  virtual HalResult readTemperature(float& temperature) = 0;
  
  /** Read humidity only
   * @param humidity Reference to store humidity (%)
   * @return HalResult::OK on success
   */
  virtual HalResult readHumidity(float& humidity) = 0;
  
  /** Read pressure only
   * @param pressure Reference to store pressure (Pa)
   * @return HalResult::OK on success
   */
  virtual HalResult readPressure(float& pressure) = 0;
  
  /** Calculate altitude from pressure
   * @param sea_level_pressure Sea level pressure in Pa (default 101325 Pa)
   * @return Altitude in meters
   */
  virtual float calculateAltitude(float sea_level_pressure = 101325.0f) = 0;
  
  /** Force measurement (for forced mode)
   * @return HalResult::OK on success
   */
  virtual HalResult triggerMeasurement() = 0;
  
  /** Check if measurement is complete
   * @return true if data ready
   */
  virtual bool dataReady() = 0;
};

} // namespace arcos::hal

#endif // ARCOS_INCLUDE_HAL_IHAL_ENVIRONMENTAL_HPP_
