/*****************************************************************
 * File:      Application.hpp
 * Category:  Application
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Master header for the Application Layer.
 *    Provides dual-core architecture for Synth-Head:
 * 
 *    Core 0 (General):
 *    - Main application logic
 *    - Sensor polling (IMU, GPS, Mic, Environment)
 *    - Network/Web server
 *    - User input (buttons)
 *    - Eye controller (animation parameters)
 * 
 *    Core 1 (GPU Pipeline):
 *    - Animation compositing
 *    - Shader evaluation
 *    - Frame rendering
 *    - GPU command generation
 *    - UART transmission to GPU
 * 
 * Usage:
 *    #include "Application/Application.hpp"
 *    
 *    // In boot mode:
 *    Application::init();
 *    Application::start();
 *    
 *    // In current mode loop:
 *    auto& eye = Application::getEyeController();
 *    eye.setLook(0.5f, 0.0f);
 *    eye.setShader(1);  // Rainbow
 *    eye.update(deltaTime);
 *****************************************************************/

#ifndef ARCOS_APPLICATION_HPP_
#define ARCOS_APPLICATION_HPP_

// Core components
#include "Core/ApplicationCore.hpp"
#include "Core/TaskManager.hpp"
#include "Core/SyncBuffer.hpp"

// Pipeline components (Core 1)
#include "Pipeline/GpuPipeline.hpp"
#include "Pipeline/AnimationPipeline.hpp"
#include "Pipeline/SpriteSystem.hpp"  // Sprite system for cached GPU sprites

// Controllers (Core 0)
#include "Controllers/EyeController.hpp"

namespace Application {

// ============================================================
// Global Application State
// ============================================================

/** Global animation buffer for inter-core communication
 *  Defined in ApplicationCore.cpp
 */
AnimationBuffer& getAnimationBuffer();

/** Global sensor data buffer */
inline SensorBuffer& getSensorBuffer() {
  static SensorBuffer instance;
  return instance;
}

/** Global event queue */
inline EventQueue& getEventQueue() {
  static EventQueue instance;
  return instance;
}

// ============================================================
// Application Initialization
// ============================================================

/** Initialize the application layer
 * Call this from BootMode::onBoot()
 * @return true if successful
 */
inline bool init() {
  // Initialize application core
  if (!getApplicationCore().init()) {
    return false;
  }
  
  // Initialize task manager
  if (!getTaskManager().init()) {
    return false;
  }
  
  // Initialize eye controller with animation buffer
  getEyeController().init(&getAnimationBuffer());
  
  return true;
}

/** Start the dual-core application
 * Call this after init()
 * @return true if successful
 */
inline bool start() {
  return getApplicationCore().start();
}

/** Stop the application */
inline void stop() {
  getApplicationCore().stop();
}

/** Shutdown and cleanup */
inline void shutdown() {
  getApplicationCore().shutdown();
}

// ============================================================
// Convenience Accessors
// ============================================================

/** Get the eye controller for animation control */
inline EyeController& eye() {
  return getEyeController();
}

/** Get the application core */
inline ApplicationCore& core() {
  return getApplicationCore();
}

/** Get the task manager */
inline TaskManager& tasks() {
  return getTaskManager();
}

/** Get the sprite manager for sprite caching and rendering */
inline SpriteManager& sprites() {
  return getSpriteManager();
}

// ============================================================
// Update Function (Call from Core 0)
// ============================================================

/** Update application state
 * Call this from CurrentMode::onUpdate()
 * @param deltaMs Milliseconds since last update
 */
inline void update(uint32_t deltaMs) {
  float deltaTime = deltaMs / 1000.0f;
  
  // Update eye controller (publishes to Core 1)
  getEyeController().update(deltaTime);
}

// ============================================================
// Sensor Data Publishing
// ============================================================

/** Publish sensor data (call from sensor task or main loop)
 * @param data Sensor data to publish
 */
inline void publishSensorData(const SensorData& data) {
  SensorData& buf = getSensorBuffer().getWriteBuffer();
  buf = data;
  getSensorBuffer().publishWrite();
}

/** Get latest sensor data */
inline bool getSensorData(SensorData& out) {
  return getSensorBuffer().swapAndRead(out);
}

// ============================================================
// Event System
// ============================================================

/** Push an event to the queue */
inline bool pushEvent(const Event& event) {
  return getEventQueue().push(event);
}

/** Pop an event from the queue */
inline bool popEvent(Event& event) {
  return getEventQueue().pop(event);
}

/** Push button press event */
inline void buttonPressed(uint8_t buttonId) {
  Event e(EventType::BUTTON_PRESS);
  e.data.buttonId = buttonId;
  e.timestamp = (uint32_t)(esp_timer_get_time() / 1000);
  pushEvent(e);
}

/** Push button release event */
inline void buttonReleased(uint8_t buttonId) {
  Event e(EventType::BUTTON_RELEASE);
  e.data.buttonId = buttonId;
  e.timestamp = (uint32_t)(esp_timer_get_time() / 1000);
  pushEvent(e);
}

} // namespace Application

// Convenience namespace alias
namespace App = Application;

#endif // ARCOS_APPLICATION_HPP_
