/*****************************************************************
 * File:      MetricsHub.hpp
 * Category:  include/FrameworkAPI
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    ROS2-like pub/sub system for sensor data and telemetry.
 *    Allows components to subscribe to specific data streams
 *    without tight coupling. Supports typed subscriptions.
 * 
 * Usage:
 *    MetricsHub hub;
 *    hub.init(50, 32);  // 50Hz publish rate, 32 message buffer
 *    
 *    // Subscribe to accelerometer data
 *    hub.subscribe<Vec3>("imu/accel", [](const Vec3& accel) {
 *      printf("Accel: %.2f, %.2f, %.2f\n", accel.x, accel.y, accel.z);
 *    });
 *    
 *    // Subscribe to orientation
 *    hub.subscribe<Quaternion>("imu/orientation", [](const Quaternion& q) {
 *      // Handle orientation update
 *    });
 *    
 *    // Publish custom data
 *    hub.publish("custom/data", myData);
 *****************************************************************/

#ifndef ARCOS_INCLUDE_FRAMEWORKAPI_METRICS_HUB_HPP_
#define ARCOS_INCLUDE_FRAMEWORKAPI_METRICS_HUB_HPP_

#include "FrameworkTypes.hpp"
#include "BaseAPI/Telemetry.hpp"
#include <cstring>
#include <vector>
#include <functional>

namespace arcos::framework {

// Forward declare for callback typing
class MetricsHub;

/**
 * Subscription handle for unsubscribing
 */
using SubscriptionId = uint32_t;
constexpr SubscriptionId INVALID_SUBSCRIPTION = 0;

/**
 * Typed callback wrapper
 */
template<typename T>
using TypedCallback = std::function<void(const T&)>;

/**
 * Internal subscription entry
 */
struct Subscription {
  SubscriptionId id;
  char topic[64];
  MetricCallback callback;
  size_t type_size;
  bool active;
};

/**
 * MetricsHub - Publish/Subscribe system for telemetry data
 * 
 * Provides a decoupled way for components to receive sensor
 * updates without direct dependencies. Similar to ROS2 topics.
 */
class MetricsHub {
public:
  static constexpr size_t MAX_SUBSCRIPTIONS = 64;
  static constexpr size_t MAX_TOPICS = 32;
  
  MetricsHub() = default;
  
  /**
   * Initialize the metrics hub
   * @param publish_rate_hz How often to publish updates
   * @param buffer_size Message buffer size
   */
  Result init(uint32_t publish_rate_hz = 50, uint32_t buffer_size = 32) {
    publish_rate_hz_ = publish_rate_hz;
    publish_interval_ms_ = 1000 / publish_rate_hz;
    buffer_size_ = buffer_size;
    next_subscription_id_ = 1;
    subscription_count_ = 0;
    last_publish_time_ = 0;
    initialized_ = true;
    return Result::OK;
  }
  
  /**
   * Subscribe to a topic with typed callback
   * @param topic Topic name (e.g., "imu/accel", "gps/position")
   * @param callback Function to call when data is published
   * @return Subscription ID for later unsubscribing
   */
  template<typename T>
  SubscriptionId subscribe(const char* topic, TypedCallback<T> callback) {
    if (!initialized_ || subscription_count_ >= MAX_SUBSCRIPTIONS) {
      return INVALID_SUBSCRIPTION;
    }
    
    // Create wrapper that casts void* back to T
    MetricCallback wrapper = [callback](const void* data, size_t size) {
      if (size >= sizeof(T)) {
        callback(*static_cast<const T*>(data));
      }
    };
    
    Subscription& sub = subscriptions_[subscription_count_++];
    sub.id = next_subscription_id_++;
    strncpy(sub.topic, topic, sizeof(sub.topic) - 1);
    sub.topic[sizeof(sub.topic) - 1] = '\0';
    sub.callback = wrapper;
    sub.type_size = sizeof(T);
    sub.active = true;
    
    return sub.id;
  }
  
  /**
   * Unsubscribe from a topic
   * @param id Subscription ID returned from subscribe()
   */
  void unsubscribe(SubscriptionId id) {
    for (size_t i = 0; i < subscription_count_; i++) {
      if (subscriptions_[i].id == id) {
        subscriptions_[i].active = false;
        return;
      }
    }
  }
  
  /**
   * Publish data to a topic
   * @param topic Topic name
   * @param data Data to publish
   */
  template<typename T>
  void publish(const char* topic, const T& data) {
    publishRaw(topic, &data, sizeof(T));
  }
  
  /**
   * Publish raw data to a topic
   */
  void publishRaw(const char* topic, const void* data, size_t size) {
    for (size_t i = 0; i < subscription_count_; i++) {
      Subscription& sub = subscriptions_[i];
      if (sub.active && strcmp(sub.topic, topic) == 0) {
        sub.callback(data, size);
      }
    }
  }
  
  /**
   * Publish telemetry data from BaseAPI to all relevant topics
   * Called automatically by Framework::update()
   */
  void publishTelemetry(const base::TelemetryData& telemetry) {
    uint32_t now = telemetry.uptime_ms;
    
    // Rate limit publishing
    if (now - last_publish_time_ < publish_interval_ms_) {
      return;
    }
    last_publish_time_ = now;
    
    // Publish motion data
    publish("imu/accel", telemetry.motion.linear_acceleration);
    publish("imu/gyro", telemetry.motion.angular_velocity);
    publish("imu/orientation", telemetry.motion.orientation);
    publish("imu/euler", telemetry.motion.euler);
    publish("imu/gravity", telemetry.motion.gravity);
    publish("imu/velocity", telemetry.motion.velocity);
    publish("imu/position", telemetry.motion.position);
    
    // Publish stability
    publish("imu/stable", telemetry.motion.is_stable);
    publish("imu/confidence", telemetry.motion.orientation_confidence);
    
    // Publish environment data
    if (telemetry.env_ok) {
      publish("env/temperature", telemetry.environment.temperature);
      publish("env/humidity", telemetry.environment.humidity);
      publish("env/pressure", telemetry.environment.pressure);
      publish("env/altitude", telemetry.environment.altitude);
    }
    
    // Publish GPS data
    if (telemetry.gps_ok && telemetry.location.has_fix) {
      publish("gps/latitude", telemetry.location.latitude);
      publish("gps/longitude", telemetry.location.longitude);
      publish("gps/altitude", telemetry.location.altitude);
      publish("gps/speed", telemetry.location.speed);
      publish("gps/heading", telemetry.location.heading);
      publish("gps/satellites", telemetry.location.satellites);
    }
    
    // Publish audio data
    if (telemetry.mic_ok) {
      publish("audio/db", telemetry.audio.db_level);
      publish("audio/rms", telemetry.audio.rms_level);
      publish("audio/peak", telemetry.audio.peak_amplitude);
      publish("audio/voice", telemetry.audio.voice_detected);
    }
    
    // Publish system info
    publish("system/uptime", telemetry.uptime_ms);
    publish("system/frame", telemetry.frame_number);
  }
  
  /**
   * Get list of active subscriptions for a topic
   */
  uint32_t getSubscriberCount(const char* topic) const {
    uint32_t count = 0;
    for (size_t i = 0; i < subscription_count_; i++) {
      if (subscriptions_[i].active && strcmp(subscriptions_[i].topic, topic) == 0) {
        count++;
      }
    }
    return count;
  }
  
  /**
   * Get total active subscription count
   */
  uint32_t getTotalSubscriptions() const {
    uint32_t count = 0;
    for (size_t i = 0; i < subscription_count_; i++) {
      if (subscriptions_[i].active) count++;
    }
    return count;
  }
  
private:
  bool initialized_ = false;
  uint32_t publish_rate_hz_ = 50;
  uint32_t publish_interval_ms_ = 20;
  uint32_t buffer_size_ = 32;
  uint32_t last_publish_time_ = 0;
  
  Subscription subscriptions_[MAX_SUBSCRIPTIONS];
  size_t subscription_count_ = 0;
  SubscriptionId next_subscription_id_ = 1;
};

} // namespace arcos::framework

#endif // ARCOS_INCLUDE_FRAMEWORKAPI_METRICS_HUB_HPP_
