/*****************************************************************
 * @file AnimationDriver.hpp
 * @brief Main entry point - single include for the entire system
 * 
 * Include this file to get access to the complete animation driver.
 * 
 * QUICK START EXAMPLES:
 * 
 * Example 1: Simple solid color
 * -----------------------------
 *   AnimationManager anim;
 *   anim.setSolidColor(RGB::Red()).start();
 *   // In loop: anim.update(deltaTime);
 * 
 * Example 2: Rainbow animation
 * ----------------------------
 *   AnimationManager anim;
 *   anim.setRainbow(0.5f).start();
 * 
 * Example 3: Sensor-driven animation (any sensor type)
 * ----------------------------------------------------
 *   SensorHub sensors;
 *   
 *   // Setup accelerometer from SyncState
 *   SensorSetup::setupAccelerometer(sensors, "accel",
 *       []() { return Vec3(syncState.accelX, syncState.accelY, syncState.accelZ); },
 *       45.0f  // Mounting angle
 *   );
 *   
 *   // Setup humidity sensor
 *   SensorSetup::setupScalarSensor(sensors, "humidity",
 *       []() { return syncState.humidity; },
 *       SensorCategory::ENVIRONMENTAL, 0.0f, 100.0f
 *   );
 *   
 *   // Bind sensors to animation parameters
 *   MultiSensorBinding bindings(sensors);
 *   bindings.addBinding("accel_pitch", &pitchParam);
 *   bindings.addBinding("humidity", &colorIntensityParam, 0.01f);
 *   
 *   // Setup gesture detection
 *   SensorSetup::setupShakeGesture(sensors, "shake", "accel");
 *
 * Example 4: Multi-display rendering
 * ----------------------------------
 *   DisplayManager display;
 *   display.initHub75Combined();  // Both HUB75 as one 128x32 display
 *   display.initOled();           // OLED as separate 128x128 display
 *   
 *   // Draw on combined HUB75 (0-127, 0-31)
 *   display.hub75FillCircle(64, 16, 10, Color::red());
 *   
 *   // Draw on OLED (separate coordinate space)
 *   display.oledFillCircle(64, 64, 30, Color::white());
 *   
 *   display.flushAll();
 * 
 * Example 5: Custom scene with layers
 * -----------------------------------
 *   AnimationManager anim;
 *   AnimationScene& scene = anim.createScene("MyScene");
 *   
 *   RainbowHShader* bg = new RainbowHShader();
 *   SparkleShader* sparkle = new SparkleShader();
 *   
 *   scene.addLayer("background", bg)
 *        .addLayer("sparkle", sparkle, BlendMode::ADD);
 *   
 *   anim.setActiveScene("MyScene").start();
 * 
 * Example 6: Keyframe animation
 * -----------------------------
 *   AnimationClip clip("Pulse");
 *   clip.addFloatTrack("brightness")
 *       .addKey(0.0f, 0.0f)
 *       .addKeyEaseInOut(0.5f, 1.0f)
 *       .addKeyEaseInOut(1.0f, 0.0f)
 *       .setLoop(LoopMode::LOOP);
 *   clip.play();
 *   // Use: clip.evaluateFloat("brightness")
 * 
 *****************************************************************/

#pragma once

// Core types and utilities
#include "Core/Types.hpp"
#include "Core/Color.hpp"
#include "Core/Easing.hpp"
#include "Core/Parameter.hpp"

// Shader system
#include "Shaders/Shaders.hpp"

// Animation system
#include "Animation/Animation.hpp"

// Sensor system (generalized sensor handling)
#include "Sensor/Sensors.hpp"

// Display system (multi-display management)
#include "Display/Display.hpp"

// Binding system (external value integration)
#include "Binding/Bindings.hpp"

// Rendering system
#include "Render/Render.hpp"

// Main manager
#include "AnimationManager.hpp"

// ============================================================
// Convenience namespace aliases
// ============================================================

namespace AD = AnimationDriver;
