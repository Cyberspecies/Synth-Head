/*****************************************************************
 * @file SensorDrivenExample.hpp
 * @brief Example: Sensor-driven animation with any data source
 * 
 * Demonstrates using the generalized sensor system to drive
 * animations from any sensor type (IMU, humidity, temperature,
 * GPS, audio, etc.)
 *****************************************************************/

#pragma once

#include "../AnimationDriver.hpp"

namespace AnimationDriver {
namespace Examples {

/**
 * @brief Sensor-Driven Animation Example
 * 
 * This example shows how to:
 * 1. Set up generic sensor channels for any data source
 * 2. Configure mounting angle compensation for motion sensors
 * 3. Create gesture detection (shake, tap, tilt)
 * 4. Bind sensor values to animation parameters
 * 
 * Usage:
 *   SensorDrivenAnimation anim;
 *   anim.setup(syncState);
 *   anim.setMountingAngle(45.0f);  // If mounted at 45° angle
 *   
 *   // In your update loop:
 *   anim.update(deltaTime);
 */
class SensorDrivenAnimation {
public:
    SensorDrivenAnimation() : _manager(), _sensors(), _rainbow() {}
    
    /**
     * @brief Set up the animation with SyncState data providers
     * 
     * @tparam SyncStateType Your SyncState struct type
     * @param state Reference to your SyncState
     */
    template<typename SyncStateType>
    void setup(SyncStateType& state) {
        // --------------------------------------------------------
        // Set up accelerometer channel
        // --------------------------------------------------------
        SensorSetup::setupAccelerometer(_sensors, "accel",
            [&state]() { 
                return Vec3(state.accelX, state.accelY, state.accelZ); 
            },
            0.0f  // Mounting angle (can be set later)
        );
        
        // --------------------------------------------------------
        // Set up gyroscope channel
        // --------------------------------------------------------
        SensorSetup::setupGyroscope(_sensors, "gyro",
            [&state]() { 
                return Vec3(state.gyroX, state.gyroY, state.gyroZ); 
            }
        );
        
        // --------------------------------------------------------
        // Set up shake detection
        // --------------------------------------------------------
        SensorSetup::setupShakeGesture(_sensors, "shake", "accel");
        
        // --------------------------------------------------------
        // Create orientation detector
        // --------------------------------------------------------
        _orientation.setAccelProvider([&state]() {
            return Vec3(state.accelX, state.accelY, state.accelZ);
        });
        _orientation.setGyroProvider([&state]() {
            return Vec3(state.gyroX, state.gyroY, state.gyroZ);
        });
        
        // --------------------------------------------------------
        // Set up bindings
        // --------------------------------------------------------
        _bindings = MultiSensorBinding(_sensors);
        _bindings.addBinding("accel_x", &_speedParam, 2.0f, 1.0f);  // Map to speed 0-3
        
        // --------------------------------------------------------
        // Configure rainbow shader
        // --------------------------------------------------------
        _rainbow.setSpeed(1.0f);
        _rainbow.setSaturation(1.0f);
        _rainbow.setValue(1.0f);
        
        // --------------------------------------------------------
        // Set up manager
        // --------------------------------------------------------
        _manager.setShader(&_rainbow).start();
    }
    
    /**
     * @brief Set mounting angle compensation
     */
    void setMountingAngle(float pitchOffset, float rollOffset = 0.0f) {
        // Get the accel channel and update its transform
        SensorChannel* accel = _sensors.getChannel("accel");
        if (accel) {
            TransformConfig transform;
            transform.mountingPitch = pitchOffset;
            transform.mountingRoll = rollOffset;
            accel->transform(transform);
        }
        
        _orientation.setMountingOffset(pitchOffset, rollOffset);
    }
    
    /**
     * @brief Update animation - call every frame
     */
    void update(float deltaTime) {
        // Update all sensor channels
        _sensors.update(deltaTime);
        
        // Update orientation
        _orientation.update(deltaTime);
        
        // Update bindings
        _bindings.update();
        
        // Apply parameters to shader
        _rainbow.setSpeed(_speedParam.value());
        
        // Check for shake gesture
        GestureDetector* shake = _sensors.getGesture("shake");
        if (shake && shake->isDetected()) {
            // Trigger effect on shake
            _rainbow.setSaturation(0.0f);  // Flash white
        } else {
            _rainbow.setSaturation(1.0f);
        }
        
        // Update animation manager
        _manager.update(deltaTime);
    }
    
    // Accessors
    const uint8_t* getFrameData() const { return _manager.getHUB75Data(); }
    size_t getFrameDataSize() const { return _manager.getHUB75DataSize(); }
    
    float getPitch() const { return _orientation.pitch(); }
    float getRoll() const { return _orientation.roll(); }
    
private:
    AnimationManager _manager;
    SensorHub _sensors;
    MultiSensorBinding _bindings;
    OrientationDetector _orientation;
    
    RainbowHShader _rainbow;
    Parameter _speedParam;
};


/**
 * @brief Multi-Sensor Animation Example
 * 
 * Shows how to combine multiple sensor types:
 * - Motion (accelerometer/gyroscope)
 * - Environmental (temperature, humidity)
 * - Light level
 */
class MultiSensorAnimation {
public:
    MultiSensorAnimation() = default;
    
    template<typename SyncStateType>
    void setup(SyncStateType& state) {
        // --------------------------------------------------------
        // Motion sensors
        // --------------------------------------------------------
        SensorSetup::setupAccelerometer(_sensors, "accel",
            [&state]() { 
                return Vec3(state.accelX, state.accelY, state.accelZ); 
            }
        );
        
        // --------------------------------------------------------
        // Environmental sensors (example - adapt to your sensors)
        // --------------------------------------------------------
        // Humidity sensor (0-100%)
        _sensors.createChannel("humidity", SensorInfo::Scalar(
            SensorCategory::ENVIRONMENTAL, -1.0f, 1.0f
        ));
        // Note: Set provider when the sensor is available
        // _sensors.getChannel("humidity")->setProvider([]() { return readHumidity(); });
        
        // Temperature sensor (example range)
        _sensors.createChannel("temperature", SensorInfo::Scalar(
            SensorCategory::ENVIRONMENTAL, 0.0f, 50.0f
        ));
        
        // Light level (example)
        _sensors.createChannel("light", SensorInfo::Scalar(
            SensorCategory::LIGHT, 0.0f, 1000.0f
        ));
        
        // --------------------------------------------------------
        // Gesture detection
        // --------------------------------------------------------
        SensorSetup::setupShakeGesture(_sensors, "shake", "accel");
        SensorSetup::setupTapGesture(_sensors, "tap", "accel");
        
        // Tilt gesture (triggers when tilted > 30 degrees)
        GestureConfig tiltConfig = GestureConfig::Tilt(30.0f);
        _sensors.createGesture("tilt", tiltConfig);
        GestureDetector* tilt = _sensors.getGesture("tilt");
        if (tilt) {
            tilt->setProvider([this]() {
                return Vec3(_sensors.getScalar("accel_x"),
                           _sensors.getScalar("accel_y"),
                           _sensors.getScalar("accel_z"));
            });
        }
        
        // --------------------------------------------------------
        // Set up bindings
        // --------------------------------------------------------
        _bindings = MultiSensorBinding(_sensors);
        
        // Motion affects speed
        _bindings.addBinding("accel_x", &_speedParam, 2.0f, 1.0f);
        
        // Temperature affects hue (warm=red, cold=blue)
        // Normalized: 0°C -> blue, 25°C -> white, 50°C -> red
        _bindings.addBinding("temperature", &_hueParam, 0.02f, 0.0f);
        
        // Humidity affects saturation
        _bindings.addBinding("humidity", &_satParam, 0.01f, 0.5f);
        
        // Light affects brightness
        _bindings.addBinding("light", &_brightParam, 0.001f, 0.2f);
        
        // --------------------------------------------------------
        // Configure shaders
        // --------------------------------------------------------
        _rainbow.setSpeed(1.0f).setSaturation(1.0f).setValue(1.0f);
        _sparkle.setDensity(0.05f).setSpeed(3.0f);
        _sparkle.setBaseColor(RGB::Black());
        _sparkle.setSparkleColor(RGB::White());
        
        // --------------------------------------------------------
        // Create scene with layers
        // --------------------------------------------------------
        _scene.setName("Multi-Sensor Scene");
        _scene.addLayer("rainbow", &_rainbow);
        _scene.addLayer("sparkle", &_sparkle, BlendMode::ADD);
        _scene.setLayerEnabled(1, false);  // Start with sparkle disabled
        
        _manager.setActiveScene(&_scene).start();
    }
    
    void update(float deltaTime) {
        // Update sensors
        _sensors.update(deltaTime);
        
        // Update bindings
        _bindings.update();
        
        // Apply parameters
        _rainbow.setSpeed(_speedParam.value());
        _rainbow.setValue(_brightParam.value());
        
        // Check gestures
        GestureDetector* shake = _sensors.getGesture("shake");
        if (shake && shake->isDetected()) {
            _sparkleOpacity = 1.0f;
            _scene.setLayerEnabled(1, true);
        }
        
        // Fade sparkle
        if (_sparkleOpacity > 0.0f) {
            _sparkleOpacity -= deltaTime * 2.0f;
            if (_sparkleOpacity <= 0.0f) {
                _sparkleOpacity = 0.0f;
                _scene.setLayerEnabled(1, false);
            } else {
                _scene.setLayerOpacity(1, _sparkleOpacity);
            }
        }
        
        // Update manager
        _manager.update(deltaTime);
    }
    
    const uint8_t* getFrameData() const { return _manager.getHUB75Data(); }
    size_t getFrameDataSize() const { return _manager.getHUB75DataSize(); }

private:
    AnimationManager _manager;
    AnimationScene _scene;
    SensorHub _sensors;
    MultiSensorBinding _bindings;
    
    RainbowHShader _rainbow;
    SparkleShader _sparkle;
    
    Parameter _speedParam;
    Parameter _hueParam;
    Parameter _satParam;
    Parameter _brightParam;
    
    float _sparkleOpacity = 0.0f;
};


/**
 * @brief Display System Example
 * 
 * Shows how to use the multi-display system:
 * - HUB75 panels as combined display
 * - OLED as separate display
 */
class DisplaySystemExample {
public:
    DisplaySystemExample() = default;
    
    void setup() {
        // Initialize HUB75 as combined display (128x32)
        _display.initHub75Combined();
        
        // Initialize OLED (128x128)
        _display.initOled();
    }
    
    void update(float deltaTime) {
        // Draw on HUB75 (full 128x32 coordinate space)
        _display.hub75Clear();
        
        // Draw a circle that moves across both panels
        int cx = static_cast<int>(64 + 50 * sinf(_time));
        _display.hub75FillCircle(cx, 16, 8, Color::red());
        
        // Draw a line across both panels
        _display.hub75DrawLine(0, 0, 127, 31, Color::blue());
        
        // Draw on OLED (separate 128x128 coordinate space)
        _display.oledClear(Color::black());
        
        // Draw concentric circles
        for (int r = 10; r < 60; r += 10) {
            _display.oledDrawCircle(64, 64, r, Color::white());
        }
        
        // Draw animated dot
        int ox = static_cast<int>(64 + 40 * cosf(_time * 2));
        int oy = static_cast<int>(64 + 40 * sinf(_time * 2));
        _display.oledFillCircle(ox, oy, 5, Color::white());
        
        _time += deltaTime;
    }
    
    DisplayManager& display() { return _display; }

private:
    DisplayManager _display;
    float _time = 0.0f;
};

} // namespace Examples
} // namespace AnimationDriver
