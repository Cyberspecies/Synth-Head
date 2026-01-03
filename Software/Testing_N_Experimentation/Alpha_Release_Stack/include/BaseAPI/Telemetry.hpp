/*****************************************************************
 * File:      Telemetry.hpp
 * Category:  include/BaseAPI
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Unified telemetry system that fuses sensor data into a
 *    single, hardware-agnostic state structure. Provides:
 *    - Position estimation
 *    - Orientation (quaternion, euler, gravity vector)
 *    - Velocity and acceleration
 *    - Environmental conditions
 *    - GPS data
 *    - Audio levels
 * 
 * Layer:
 *    HAL Layer -> [Base System API - Telemetry] -> Application
 *****************************************************************/

#ifndef ARCOS_INCLUDE_BASEAPI_TELEMETRY_HPP_
#define ARCOS_INCLUDE_BASEAPI_TELEMETRY_HPP_

#include "BaseTypes.hpp"
#include <cstring>

namespace arcos::base{

// ============================================================
// Telemetry Data Structures
// ============================================================

/** Motion/orientation state */
struct MotionState{
  // Orientation
  Quaternion orientation;     // Current orientation as quaternion
  Vec3 euler;                 // Roll, pitch, yaw in radians
  Vec3 gravity;               // Gravity vector in body frame (m/s²)
  
  // Angular motion
  Vec3 angular_velocity;      // rad/s in body frame
  Vec3 angular_acceleration;  // rad/s² (derived)
  
  // Linear motion
  Vec3 linear_acceleration;   // m/s² in world frame (gravity removed)
  Vec3 velocity;              // m/s estimated velocity
  Vec3 position;              // m estimated position (relative)
  
  // Quality indicators
  float orientation_confidence;  // 0.0-1.0
  bool is_stable;                // True if motion is minimal
  bool is_calibrated;            // True if sensors are calibrated
  
  MotionState(){
    memset(this, 0, sizeof(*this));
    orientation.w = 1.0f;  // Identity quaternion
    orientation_confidence = 0.0f;
    is_stable = false;
    is_calibrated = false;
  }
};

/** Environmental state */
struct EnvironmentState{
  float temperature;         // Celsius
  float humidity;            // Percentage (0-100)
  float pressure;            // Pascals
  float altitude;            // Meters (derived from pressure)
  
  bool valid;                // Data is valid
  Timestamp last_update;     // Last update time
  
  EnvironmentState(){
    memset(this, 0, sizeof(*this));
  }
};

/** GPS/Location state */
struct LocationState{
  double latitude;           // Degrees
  double longitude;          // Degrees
  float altitude;            // Meters
  float speed;               // m/s ground speed
  float heading;             // Degrees (0-360)
  
  uint8_t satellites;        // Number of satellites
  float hdop;                // Horizontal dilution of precision
  
  bool has_fix;              // Valid GPS fix
  uint8_t fix_quality;       // 0=none, 1=GPS, 2=DGPS
  Timestamp last_update;
  
  LocationState(){
    memset(this, 0, sizeof(*this));
  }
};

/** Audio state */
struct AudioState{
  float db_level;            // Current dB level
  float rms_level;           // RMS amplitude (0.0-1.0)
  int32_t peak_amplitude;    // Peak sample value
  
  float frequency_dominant;  // Dominant frequency (Hz)
  float frequency_low;       // Low freq energy (0.0-1.0)
  float frequency_mid;       // Mid freq energy (0.0-1.0)
  float frequency_high;      // High freq energy (0.0-1.0)
  
  bool is_clipping;          // Audio clipping detected
  bool voice_detected;       // Voice activity detected
  Timestamp last_update;
  
  AudioState(){
    memset(this, 0, sizeof(*this));
    db_level = -100.0f;
  }
};

/** Complete system telemetry */
struct TelemetryData{
  // Sub-states
  MotionState motion;
  EnvironmentState environment;
  LocationState location;
  AudioState audio;
  
  // System info
  Timestamp timestamp;       // Current time (ms)
  uint32_t frame_number;     // Incrementing frame counter
  uint32_t uptime_ms;        // System uptime
  
  // Status flags
  bool imu_ok;
  bool env_ok;
  bool gps_ok;
  bool mic_ok;
};

// ============================================================
// Sensor Fusion Configuration
// ============================================================

/** Fusion filter configuration */
struct FusionConfig{
  // Complementary filter weights
  float gyro_weight;         // Weight for gyroscope (0.9-0.99)
  float accel_weight;        // Weight for accelerometer
  float mag_weight;          // Weight for magnetometer
  
  // Filter parameters
  float sample_rate_hz;      // Expected sample rate
  float low_pass_alpha;      // Low-pass filter coefficient
  
  // Calibration
  Vec3 accel_bias;           // Accelerometer bias offset
  Vec3 gyro_bias;            // Gyroscope bias offset
  Vec3 mag_hard_iron;        // Magnetometer hard iron offset
  
  // Motion detection thresholds
  float motion_threshold;    // Threshold for motion detection
  float stability_time_ms;   // Time to consider stable
  
  FusionConfig(){
    gyro_weight = 0.98f;
    accel_weight = 0.02f;
    mag_weight = 0.0f;
    sample_rate_hz = 100.0f;
    low_pass_alpha = 0.2f;
    motion_threshold = 0.1f;
    stability_time_ms = 500.0f;
  }
};

// ============================================================
// Telemetry Processor Interface
// ============================================================

/** 
 * ITelemetryProcessor - Interface for telemetry processing
 * 
 * Implementations fuse raw sensor data into unified telemetry.
 * This interface allows different fusion algorithms (complementary,
 * Kalman, Madgwick, etc.) to be swapped without changing higher layers.
 */
class ITelemetryProcessor{
public:
  virtual ~ITelemetryProcessor() = default;
  
  /** Initialize the telemetry processor */
  virtual Result init(const FusionConfig& config) = 0;
  
  /** Update with new IMU data
   * @param accel Accelerometer reading (m/s²)
   * @param gyro Gyroscope reading (rad/s)
   * @param mag Magnetometer reading (optional, µT)
   * @param dt Time since last update (seconds)
   */
  virtual void updateIMU(const Vec3& accel, const Vec3& gyro, 
                         const Vec3& mag, float dt) = 0;
  
  /** Update with new environmental data */
  virtual void updateEnvironment(float temp, float humidity, 
                                 float pressure) = 0;
  
  /** Update with new GPS data */
  virtual void updateGPS(double lat, double lon, float alt,
                        float speed, float heading, uint8_t sats) = 0;
  
  /** Update with new audio data */
  virtual void updateAudio(float db, float rms, int32_t peak) = 0;
  
  /** Get current telemetry state */
  virtual const TelemetryData& getTelemetry() const = 0;
  
  /** Reset state */
  virtual void reset() = 0;
  
  /** Calibrate sensors (should be called when stationary) */
  virtual Result calibrate() = 0;
  
  /** Check if system is calibrated */
  virtual bool isCalibrated() const = 0;
};

// ============================================================
// Default Implementation: Complementary Filter
// ============================================================

/**
 * TelemetryProcessor - Default implementation using complementary filter
 * 
 * Uses a complementary filter for sensor fusion, which is computationally
 * efficient and works well for real-time applications.
 */
class TelemetryProcessor : public ITelemetryProcessor{
public:
  TelemetryProcessor();
  ~TelemetryProcessor() override = default;
  
  Result init(const FusionConfig& config) override;
  
  void updateIMU(const Vec3& accel, const Vec3& gyro, 
                 const Vec3& mag, float dt) override;
  
  void updateEnvironment(float temp, float humidity, 
                         float pressure) override;
  
  void updateGPS(double lat, double lon, float alt,
                float speed, float heading, uint8_t sats) override;
  
  void updateAudio(float db, float rms, int32_t peak) override;
  
  const TelemetryData& getTelemetry() const override{ return telemetry_; }
  
  void reset() override;
  
  Result calibrate() override;
  
  bool isCalibrated() const override{ return calibrated_; }
  
  /** Get gravity vector in world frame */
  Vec3 getGravityWorld() const{ return Vec3(0, 0, -math::GRAVITY); }
  
  /** Get gravity vector in body frame */
  Vec3 getGravityBody() const;
  
  /** Get linear acceleration (gravity removed) */
  Vec3 getLinearAcceleration() const;

private:
  FusionConfig config_;
  TelemetryData telemetry_;
  bool initialized_;
  bool calibrated_;
  
  // Calibration accumulators
  static constexpr int CALIBRATION_SAMPLES = 100;
  Vec3 accel_sum_;
  Vec3 gyro_sum_;
  int calibration_count_;
  
  // Motion detection
  Timestamp last_motion_time_;
  float motion_magnitude_;
  
  // Velocity integration
  Vec3 velocity_estimate_;
  
  // Reference pressure for altitude
  float reference_pressure_;
  
  /** Calculate roll and pitch from accelerometer */
  void calculateAccelAngles(const Vec3& accel, float& roll, float& pitch);
  
  /** Apply complementary filter */
  void applyComplementaryFilter(const Vec3& accel, const Vec3& gyro, float dt);
  
  /** Update derived values (gravity, linear accel, etc.) */
  void updateDerived();
  
  /** Calculate altitude from pressure */
  float calculateAltitude(float pressure);
};

// ============================================================
// Implementation
// ============================================================

inline TelemetryProcessor::TelemetryProcessor()
  : initialized_(false)
  , calibrated_(false)
  , calibration_count_(0)
  , last_motion_time_(0)
  , motion_magnitude_(0)
  , reference_pressure_(101325.0f)  // Sea level
{
  memset(&telemetry_, 0, sizeof(telemetry_));
  telemetry_.motion.orientation.w = 1.0f;
}

inline Result TelemetryProcessor::init(const FusionConfig& config){
  config_ = config;
  reset();
  initialized_ = true;
  return Result::OK;
}

inline void TelemetryProcessor::reset(){
  memset(&telemetry_, 0, sizeof(telemetry_));
  telemetry_.motion.orientation.w = 1.0f;
  calibrated_ = false;
  calibration_count_ = 0;
  accel_sum_ = Vec3();
  gyro_sum_ = Vec3();
  velocity_estimate_ = Vec3();
}

inline Result TelemetryProcessor::calibrate(){
  if(calibration_count_ < CALIBRATION_SAMPLES){
    return Result::BUSY;  // Still collecting samples
  }
  
  // Calculate biases
  config_.accel_bias = accel_sum_ / (float)calibration_count_;
  config_.gyro_bias = gyro_sum_ / (float)calibration_count_;
  
  // Adjust accel bias to account for gravity (assuming Z-up)
  config_.accel_bias.z -= math::GRAVITY;
  
  calibrated_ = true;
  telemetry_.motion.is_calibrated = true;
  calibration_count_ = 0;
  accel_sum_ = Vec3();
  gyro_sum_ = Vec3();
  
  return Result::OK;
}

inline void TelemetryProcessor::calculateAccelAngles(const Vec3& accel, 
                                                     float& roll, float& pitch){
  // Calculate roll and pitch from accelerometer
  // Assumes Z points up when level
  roll = atan2f(accel.y, accel.z);
  pitch = atan2f(-accel.x, sqrtf(accel.y * accel.y + accel.z * accel.z));
}

inline void TelemetryProcessor::applyComplementaryFilter(const Vec3& accel, 
                                                          const Vec3& gyro, 
                                                          float dt){
  // Get current euler angles
  float roll, pitch, yaw;
  telemetry_.motion.orientation.toEuler(roll, pitch, yaw);
  
  // Integrate gyroscope
  roll += gyro.x * dt;
  pitch += gyro.y * dt;
  yaw += gyro.z * dt;
  
  // Calculate accelerometer angles
  float accel_roll, accel_pitch;
  calculateAccelAngles(accel, accel_roll, accel_pitch);
  
  // Apply complementary filter
  roll = config_.gyro_weight * roll + config_.accel_weight * accel_roll;
  pitch = config_.gyro_weight * pitch + config_.accel_weight * accel_pitch;
  // Yaw from gyro only (no magnetometer correction in basic version)
  
  // Update quaternion
  telemetry_.motion.orientation = Quaternion::fromEuler(roll, pitch, yaw);
  telemetry_.motion.orientation.normalize();
  
  // Store euler angles
  telemetry_.motion.euler = Vec3(roll, pitch, yaw);
}

inline void TelemetryProcessor::updateDerived(){
  // Calculate gravity vector in body frame
  telemetry_.motion.gravity = getGravityBody();
  
  // Calculate linear acceleration (gravity removed)
  telemetry_.motion.linear_acceleration = getLinearAcceleration();
  
  // Detect motion
  motion_magnitude_ = telemetry_.motion.angular_velocity.magnitude() +
                     telemetry_.motion.linear_acceleration.magnitude() * 0.1f;
  
  // Update stability flag
  if(motion_magnitude_ > config_.motion_threshold){
    last_motion_time_ = telemetry_.timestamp;
    telemetry_.motion.is_stable = false;
  }else if(telemetry_.timestamp - last_motion_time_ > (Timestamp)config_.stability_time_ms){
    telemetry_.motion.is_stable = true;
  }
  
  // Update orientation confidence (basic heuristic)
  if(calibrated_){
    telemetry_.motion.orientation_confidence = telemetry_.motion.is_stable ? 0.9f : 0.7f;
  }else{
    telemetry_.motion.orientation_confidence = 0.3f;
  }
}

inline Vec3 TelemetryProcessor::getGravityBody() const{
  // Transform world gravity to body frame using inverse rotation
  Vec3 gravity_world(0, 0, -math::GRAVITY);
  return telemetry_.motion.orientation.conjugate().rotate(gravity_world);
}

inline Vec3 TelemetryProcessor::getLinearAcceleration() const{
  // This would be set during updateIMU based on raw accel - gravity
  return telemetry_.motion.linear_acceleration;
}

inline float TelemetryProcessor::calculateAltitude(float pressure){
  // Barometric formula: altitude from pressure
  // h = 44330 * (1 - (P/P0)^(1/5.255))
  if(pressure <= 0) return 0;
  return 44330.0f * (1.0f - powf(pressure / reference_pressure_, 0.190284f));
}

inline void TelemetryProcessor::updateIMU(const Vec3& accel, const Vec3& gyro,
                                          const Vec3& mag, float dt){
  (void)mag;  // Unused in basic implementation
  
  // Apply bias correction if calibrated
  Vec3 corrected_accel = accel;
  Vec3 corrected_gyro = gyro;
  
  if(calibrated_){
    corrected_accel = accel - config_.accel_bias;
    corrected_gyro = gyro - config_.gyro_bias;
  }else{
    // Accumulate for calibration
    accel_sum_ += accel;
    gyro_sum_ += gyro;
    calibration_count_++;
  }
  
  // Store raw angular velocity
  telemetry_.motion.angular_velocity = corrected_gyro;
  
  // Apply sensor fusion
  applyComplementaryFilter(corrected_accel, corrected_gyro, dt);
  
  // Calculate linear acceleration (remove gravity)
  Vec3 gravity_body = getGravityBody();
  telemetry_.motion.linear_acceleration = corrected_accel - gravity_body;
  
  // Simple velocity integration (note: will drift without GPS correction)
  velocity_estimate_ += telemetry_.motion.linear_acceleration * dt;
  velocity_estimate_ *= 0.99f;  // Decay to prevent unbounded drift
  telemetry_.motion.velocity = velocity_estimate_;
  
  // Update derived values
  updateDerived();
  
  // Update status
  telemetry_.imu_ok = true;
  telemetry_.frame_number++;
}

inline void TelemetryProcessor::updateEnvironment(float temp, float humidity,
                                                   float pressure){
  telemetry_.environment.temperature = temp;
  telemetry_.environment.humidity = humidity;
  telemetry_.environment.pressure = pressure;
  telemetry_.environment.altitude = calculateAltitude(pressure);
  telemetry_.environment.valid = true;
  telemetry_.env_ok = true;
}

inline void TelemetryProcessor::updateGPS(double lat, double lon, float alt,
                                          float speed, float heading, uint8_t sats){
  telemetry_.location.latitude = lat;
  telemetry_.location.longitude = lon;
  telemetry_.location.altitude = alt;
  telemetry_.location.speed = speed;
  telemetry_.location.heading = heading;
  telemetry_.location.satellites = sats;
  telemetry_.location.has_fix = (sats >= 3);
  telemetry_.location.fix_quality = (sats >= 4) ? 1 : 0;
  telemetry_.gps_ok = true;
  
  // GPS can correct velocity drift
  if(telemetry_.location.has_fix){
    // Convert speed/heading to velocity vector
    float heading_rad = heading * math::DEG_TO_RAD;
    telemetry_.motion.velocity.x = speed * sinf(heading_rad);
    telemetry_.motion.velocity.y = speed * cosf(heading_rad);
    velocity_estimate_ = telemetry_.motion.velocity;
  }
}

inline void TelemetryProcessor::updateAudio(float db, float rms, int32_t peak){
  telemetry_.audio.db_level = db;
  telemetry_.audio.rms_level = rms;
  telemetry_.audio.peak_amplitude = peak;
  telemetry_.audio.is_clipping = (peak > 30000 || peak < -30000);
  telemetry_.mic_ok = true;
}

} // namespace arcos::base

#endif // ARCOS_INCLUDE_BASEAPI_TELEMETRY_HPP_
