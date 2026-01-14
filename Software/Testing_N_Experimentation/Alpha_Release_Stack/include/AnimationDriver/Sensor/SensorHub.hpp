/*****************************************************************
 * @file SensorHub.hpp
 * @brief Central registry for all sensors in the system
 * 
 * Manages multiple sensor channels and gesture detectors.
 * Provides a unified interface for accessing sensor data.
 *****************************************************************/

#pragma once

#include "SensorTypes.hpp"
#include "SensorChannel.hpp"
#include "GestureDetector.hpp"
#include <cstring>

namespace AnimationDriver {

// ============================================================
// Sensor Hub
// ============================================================

class SensorHub {
public:
    static constexpr int MAX_CHANNELS = 16;
    static constexpr int MAX_GESTURES = 16;
    
    SensorHub() : channelCount_(0), gestureCount_(0) {}
    
    // ========================================================
    // Channel Management
    // ========================================================
    
    // Create a new channel
    SensorChannel& createChannel(const char* name) {
        if (channelCount_ < MAX_CHANNELS) {
            channels_[channelCount_].setName(name);
            return channels_[channelCount_++];
        }
        return channels_[MAX_CHANNELS - 1];
    }
    
    // Get channel by name
    SensorChannel* getChannel(const char* name) {
        for (int i = 0; i < channelCount_; i++) {
            if (strcmp(channels_[i].getName(), name) == 0) {
                return &channels_[i];
            }
        }
        return nullptr;
    }
    
    // Get channel by index
    SensorChannel* getChannel(int index) {
        return (index >= 0 && index < channelCount_) ? &channels_[index] : nullptr;
    }
    
    int getChannelCount() const { return channelCount_; }
    
    // ========================================================
    // Gesture Management
    // ========================================================
    
    // Create a new gesture detector
    GestureDetector& createGesture(const char* name, const GestureConfig& config) {
        if (gestureCount_ < MAX_GESTURES) {
            gestures_[gestureCount_].setName(name);
            gestures_[gestureCount_].setConfig(config);
            return gestures_[gestureCount_++];
        }
        return gestures_[MAX_GESTURES - 1];
    }
    
    // Get gesture by name
    GestureDetector* getGesture(const char* name) {
        for (int i = 0; i < gestureCount_; i++) {
            if (strcmp(gestures_[i].getName(), name) == 0) {
                return &gestures_[i];
            }
        }
        return nullptr;
    }
    
    // Get gesture by index
    GestureDetector* getGesture(int index) {
        return (index >= 0 && index < gestureCount_) ? &gestures_[index] : nullptr;
    }
    
    int getGestureCount() const { return gestureCount_; }
    
    // ========================================================
    // Update
    // ========================================================
    
    // Update all channels and gestures
    void update(float deltaTime) {
        // Update all channels
        for (int i = 0; i < channelCount_; i++) {
            channels_[i].update();
        }
        
        // Update gestures that are linked to channels
        // (gestures must be updated manually with their source data)
        (void)deltaTime;
    }
    
    // ========================================================
    // Quick Access Methods
    // ========================================================
    
    // Get scalar value from named channel
    float getScalar(const char* channelName, float defaultVal = 0.0f) const {
        for (int i = 0; i < channelCount_; i++) {
            if (strcmp(channels_[i].getName(), channelName) == 0) {
                return channels_[i].getScalar();
            }
        }
        return defaultVal;
    }
    
    // Get Vec3 from named channel
    Vec3 getVec3(const char* channelName) const {
        for (int i = 0; i < channelCount_; i++) {
            if (strcmp(channels_[i].getName(), channelName) == 0) {
                return channels_[i].getVec3();
            }
        }
        return Vec3(0, 0, 0);
    }
    
    // Check if gesture is active
    bool isGestureActive(const char* gestureName) const {
        for (int i = 0; i < gestureCount_; i++) {
            if (strcmp(gestures_[i].getName(), gestureName) == 0) {
                return gestures_[i].isActive();
            }
        }
        return false;
    }
    
    // Get gesture intensity
    float getGestureIntensity(const char* gestureName) const {
        for (int i = 0; i < gestureCount_; i++) {
            if (strcmp(gestures_[i].getName(), gestureName) == 0) {
                return gestures_[i].getIntensity();
            }
        }
        return 0.0f;
    }
    
    // ========================================================
    // Reset
    // ========================================================
    
    void reset() {
        for (int i = 0; i < channelCount_; i++) {
            channels_[i].reset();
        }
        for (int i = 0; i < gestureCount_; i++) {
            gestures_[i].reset();
        }
    }
    
    void clear() {
        channelCount_ = 0;
        gestureCount_ = 0;
    }
    
private:
    SensorChannel channels_[MAX_CHANNELS];
    int channelCount_;
    
    GestureDetector gestures_[MAX_GESTURES];
    int gestureCount_;
};

// ============================================================
// Common Sensor Setup Helpers
// ============================================================

namespace SensorSetup {

/**
 * Setup accelerometer channel
 * @param hub SensorHub to add channel to
 * @param xProv, yProv, zProv Raw data providers
 * @param mountPitch Device mounting pitch angle
 */
template<typename T>
inline SensorChannel& setupAccelerometer(
    SensorHub& hub,
    T& state,
    float mountPitch = 0.0f,
    float mountRoll = 0.0f
) {
    SensorChannel& ch = hub.createChannel("accel");
    ch.setCategory(SensorCategory::MOTION)
      .setValueType(SensorValueType::VEC3)
      .setRange(-16.0f, 16.0f)
      .setVec3Provider(
          [&state]() { return state.accelX; },
          [&state]() { return state.accelY; },
          [&state]() { return state.accelZ; }
      )
      .setMounting(mountPitch, mountRoll)
      .smooth(0.1f);
    
    return ch;
}

/**
 * Setup gyroscope channel
 */
template<typename T>
inline SensorChannel& setupGyroscope(
    SensorHub& hub,
    T& state,
    float mountPitch = 0.0f,
    float mountRoll = 0.0f
) {
    SensorChannel& ch = hub.createChannel("gyro");
    ch.setCategory(SensorCategory::MOTION)
      .setValueType(SensorValueType::VEC3)
      .setRange(-2000.0f, 2000.0f)
      .setVec3Provider(
          [&state]() { return state.gyroX; },
          [&state]() { return state.gyroY; },
          [&state]() { return state.gyroZ; }
      )
      .setMounting(mountPitch, mountRoll)
      .smooth(0.05f)
      .deadzone(5.0f);
    
    return ch;
}

/**
 * Setup generic scalar sensor (temperature, humidity, etc.)
 */
inline SensorChannel& setupScalarSensor(
    SensorHub& hub,
    const char* name,
    ScalarProvider provider,
    float minVal,
    float maxVal,
    SensorCategory category = SensorCategory::ENVIRONMENTAL
) {
    SensorChannel& ch = hub.createChannel(name);
    ch.setCategory(category)
      .setValueType(SensorValueType::SCALAR)
      .setRange(minVal, maxVal)
      .setScalarProvider(provider)
      .smooth(0.5f);  // Environmental sensors usually need more smoothing
    
    return ch;
}

/**
 * Setup shake detection gesture
 */
inline GestureDetector& setupShakeGesture(
    SensorHub& hub,
    float threshold = 1.5f
) {
    return hub.createGesture("shake", GestureConfig::Shake(threshold));
}

/**
 * Setup tap detection gesture
 */
inline GestureDetector& setupTapGesture(
    SensorHub& hub,
    float threshold = 2.0f
) {
    return hub.createGesture("tap", GestureConfig::Tap(threshold));
}

} // namespace SensorSetup

} // namespace AnimationDriver
