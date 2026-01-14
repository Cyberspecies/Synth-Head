/*****************************************************************
 * @file SensorBinding.hpp
 * @brief General sensor-to-animation binding (replaces IMUBinding)
 *****************************************************************/

#pragma once

#include "ValueBinding.hpp"
#include "FilterChain.hpp"
#include "../Sensor/Sensors.hpp"

namespace AnimationDriver {

// ============================================================
// Sensor Binding - Connects any sensor to animation parameters
// ============================================================

class SensorBinding {
public:
    // Bind to a scalar sensor channel
    SensorBinding(SensorHub& hub, const char* channelName, Parameter* target)
        : _hub(&hub), _channelName(channelName), _target(target), _scale(1.0f), _offset(0.0f) {
    }
    
    // Set scaling and offset for the binding
    SensorBinding& setTransform(float scale, float offset = 0.0f) {
        _scale = scale;
        _offset = offset;
        return *this;
    }
    
    // Update binding (call each frame)
    void update() {
        if (!_target) return;
        
        float value = _hub->getScalar(_channelName);
        _target->setValue(value * _scale + _offset);
    }
    
private:
    SensorHub* _hub;
    const char* _channelName;
    Parameter* _target;
    float _scale;
    float _offset;
};

// ============================================================
// Multi-Sensor Binding - Bind multiple sensors at once
// ============================================================

class MultiSensorBinding {
public:
    static constexpr int MAX_BINDINGS = 16;
    
    MultiSensorBinding(SensorHub& hub) : _hub(&hub), _bindingCount(0) {}
    
    // Add a binding
    bool addBinding(const char* channelName, Parameter* target, 
                    float scale = 1.0f, float offset = 0.0f) {
        if (_bindingCount >= MAX_BINDINGS) return false;
        
        BindingEntry& entry = _bindings[_bindingCount++];
        entry.channelName = channelName;
        entry.target = target;
        entry.scale = scale;
        entry.offset = offset;
        
        return true;
    }
    
    // Update all bindings
    void update() {
        for (int i = 0; i < _bindingCount; i++) {
            BindingEntry& entry = _bindings[i];
            if (entry.target) {
                float value = _hub->getScalar(entry.channelName);
                entry.target->setValue(value * entry.scale + entry.offset);
            }
        }
    }
    
    int bindingCount() const { return _bindingCount; }
    
private:
    struct BindingEntry {
        const char* channelName;
        Parameter* target;
        float scale;
        float offset;
    };
    
    SensorHub* _hub;
    BindingEntry _bindings[MAX_BINDINGS];
    int _bindingCount;
};

// ============================================================
// Gesture Binding - React to detected gestures
// ============================================================

class GestureBinding {
public:
    using GestureCallback = void (*)(GestureType type, float magnitude);
    
    GestureBinding(SensorHub& hub, const char* gestureName)
        : _hub(&hub), _gestureName(gestureName), _callback(nullptr) {}
    
    // Set callback for when gesture is detected
    GestureBinding& onGesture(GestureCallback callback) {
        _callback = callback;
        return *this;
    }
    
    // Poll for gesture (call each frame)
    bool update() {
        GestureDetector* detector = _hub->getGesture(_gestureName);
        if (!detector) return false;
        
        if (detector->isDetected() && _callback) {
            _callback(detector->type(), detector->magnitude());
            return true;
        }
        return false;
    }
    
private:
    SensorHub* _hub;
    const char* _gestureName;
    GestureCallback _callback;
};

// ============================================================
// Orientation Binding - Bind orientation to parameters
// ============================================================

class OrientationBinding {
public:
    OrientationBinding(OrientationDetector& orientation)
        : _orientation(&orientation),
          _pitchTarget(nullptr), _rollTarget(nullptr), _yawTarget(nullptr) {}
    
    // Set target parameters for each axis
    OrientationBinding& bindPitch(Parameter* target) {
        _pitchTarget = target;
        return *this;
    }
    
    OrientationBinding& bindRoll(Parameter* target) {
        _rollTarget = target;
        return *this;
    }
    
    OrientationBinding& bindYaw(Parameter* target) {
        _yawTarget = target;
        return *this;
    }
    
    // Update bindings
    void update(float dt) {
        _orientation->update(dt);
        
        if (_pitchTarget) {
            _pitchTarget->setValue(_orientation->pitch());
        }
        if (_rollTarget) {
            _rollTarget->setValue(_orientation->roll());
        }
        if (_yawTarget) {
            _yawTarget->setValue(_orientation->yaw());
        }
    }
    
private:
    OrientationDetector* _orientation;
    Parameter* _pitchTarget;
    Parameter* _rollTarget;
    Parameter* _yawTarget;
};

// ============================================================
// Animated Sensor Response - Smooth response to sensor input
// ============================================================

class AnimatedSensorResponse {
public:
    AnimatedSensorResponse() 
        : _currentValue(0.0f), _targetValue(0.0f),
          _smoothing(0.1f), _springStiffness(0.0f), _velocity(0.0f) {}
    
    // Configure as simple smoothing
    AnimatedSensorResponse& setSmoothing(float factor) {
        _smoothing = factor;
        _springStiffness = 0.0f;
        return *this;
    }
    
    // Configure as spring system
    AnimatedSensorResponse& setSpring(float stiffness, float damping) {
        _springStiffness = stiffness;
        _springDamping = damping;
        return *this;
    }
    
    // Set target value (from sensor)
    void setTarget(float value) {
        _targetValue = value;
    }
    
    // Update animation
    float update(float dt) {
        if (_springStiffness > 0.0f) {
            // Spring physics
            float force = (_targetValue - _currentValue) * _springStiffness;
            _velocity += force * dt;
            _velocity *= (1.0f - _springDamping * dt);
            _currentValue += _velocity * dt;
        } else {
            // Simple smoothing
            _currentValue += (_targetValue - _currentValue) * _smoothing;
        }
        
        return _currentValue;
    }
    
    float value() const { return _currentValue; }
    
private:
    float _currentValue;
    float _targetValue;
    float _smoothing;
    float _springStiffness;
    float _springDamping;
    float _velocity;
};

} // namespace AnimationDriver
