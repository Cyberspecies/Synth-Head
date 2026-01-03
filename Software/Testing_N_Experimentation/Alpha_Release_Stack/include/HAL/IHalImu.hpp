/*****************************************************************
 * File:      IHalImu.hpp
 * Category:  include/HAL
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    IMU Sensor Hardware Abstraction Layer interface.
 *    Provides platform-independent access to 9-axis IMU sensors
 *    (accelerometer, gyroscope, magnetometer) like ICM20948.
 * 
 * Note:
 *    This is a sensor HAL interface. The middleware layer will
 *    use this to build higher-level odometry/telemetry services.
 *****************************************************************/

#ifndef ARCOS_INCLUDE_HAL_IHAL_IMU_HPP_
#define ARCOS_INCLUDE_HAL_IHAL_IMU_HPP_

#include "HalTypes.hpp"

namespace arcos::hal{

// ============================================================
// IMU Data Structures
// ============================================================

/** IMU sensor data (raw) */
struct ImuData{
  // Accelerometer (g or m/s²)
  Vec3f accel;
  
  // Gyroscope (degrees/second or rad/s)
  Vec3f gyro;
  
  // Magnetometer (μT or gauss)
  Vec3f mag;
  
  // Temperature (°C)
  float temperature = 0.0f;
  
  // Timestamp when data was read
  timestamp_ms_t timestamp = 0;
  
  // Validity flags
  bool accel_valid = false;
  bool gyro_valid = false;
  bool mag_valid = false;
};

/** IMU configuration */
struct ImuConfig{
  i2c_addr_t address = 0x68;
  
  // Accelerometer settings
  uint8_t accel_range = 4;      // ±2, ±4, ±8, ±16 g
  uint16_t accel_rate = 100;    // Sample rate in Hz
  
  // Gyroscope settings
  uint16_t gyro_range = 500;    // ±250, ±500, ±1000, ±2000 dps
  uint16_t gyro_rate = 100;     // Sample rate in Hz
  
  // Magnetometer settings
  bool mag_enabled = true;
  uint8_t mag_rate = 100;       // Sample rate in Hz
};

// ============================================================
// IMU Interface
// ============================================================

/** IMU Hardware Abstraction Interface
 * 
 * Provides platform-independent access to IMU sensors.
 * This is a low-level interface - middleware will use this
 * to provide sensor fusion and orientation estimation.
 */
class IHalImu{
public:
  virtual ~IHalImu() = default;
  
  /** Initialize IMU sensor
   * @param config IMU configuration
   * @return HalResult::OK on success
   */
  virtual HalResult init(const ImuConfig& config) = 0;
  
  /** Deinitialize IMU sensor
   * @return HalResult::OK on success
   */
  virtual HalResult deinit() = 0;
  
  /** Check if IMU is initialized
   * @return true if initialized
   */
  virtual bool isInitialized() const = 0;
  
  /** Read all IMU data
   * @param data Reference to ImuData structure to fill
   * @return HalResult::OK on success
   */
  virtual HalResult readAll(ImuData& data) = 0;
  
  /** Read accelerometer data only
   * @param accel Reference to Vec3f to fill (g or m/s²)
   * @return HalResult::OK on success
   */
  virtual HalResult readAccel(Vec3f& accel) = 0;
  
  /** Read gyroscope data only
   * @param gyro Reference to Vec3f to fill (dps or rad/s)
   * @return HalResult::OK on success
   */
  virtual HalResult readGyro(Vec3f& gyro) = 0;
  
  /** Read magnetometer data only
   * @param mag Reference to Vec3f to fill (μT or gauss)
   * @return HalResult::OK on success
   */
  virtual HalResult readMag(Vec3f& mag) = 0;
  
  /** Read temperature
   * @param temperature Reference to store temperature (°C)
   * @return HalResult::OK on success
   */
  virtual HalResult readTemperature(float& temperature) = 0;
  
  /** Calibrate accelerometer (device should be stationary)
   * @return HalResult::OK on success
   */
  virtual HalResult calibrateAccel() = 0;
  
  /** Calibrate gyroscope (device should be stationary)
   * @return HalResult::OK on success
   */
  virtual HalResult calibrateGyro() = 0;
  
  /** Set accelerometer range
   * @param range_g Range in ±g (2, 4, 8, or 16)
   * @return HalResult::OK on success
   */
  virtual HalResult setAccelRange(uint8_t range_g) = 0;
  
  /** Set gyroscope range
   * @param range_dps Range in ±dps (250, 500, 1000, or 2000)
   * @return HalResult::OK on success
   */
  virtual HalResult setGyroRange(uint16_t range_dps) = 0;
  
  /** Check if new data is available
   * @return true if new data ready
   */
  virtual bool dataReady() = 0;
};

} // namespace arcos::hal

#endif // ARCOS_INCLUDE_HAL_IHAL_IMU_HPP_
