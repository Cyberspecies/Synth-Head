/*****************************************************************
 * File:      DebugPages.hpp
 * Category:  UI/OLED
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    OLED debug mode pages showing sensor data.
 *    Pages: IMU, Environmental, GPS, Microphone, System Info
 * 
 * Usage:
 *    renderImuPage(oled_manager, sensor_data);
 *****************************************************************/

#ifndef DEBUG_PAGES_HPP
#define DEBUG_PAGES_HPP

#include "Manager/OLEDDisplayManager.hpp"
#include "Drivers/UART Comms/GpuUartBidirectional.hpp"
#include <cstdio>

namespace arcos::ui::oled {

/**
 * @brief Render IMU data page (accelerometer, gyro, magnetometer)
 */
inline void renderImuPage(arcos::manager::OLEDDisplayManager& oled,
                          const arcos::communication::SensorDataPayload& data) {
  oled.clear();
  oled.drawText(0, 0, "===== IMU DATA =====", true);
  
  if(data.getImuValid()){
    char buf[32];
    oled.drawText(0, 12, "Accel (g):", true);
    snprintf(buf, sizeof(buf), " X:%.2f", data.accel_x);
    oled.drawText(0, 22, buf, true);
    snprintf(buf, sizeof(buf), " Y:%.2f", data.accel_y);
    oled.drawText(0, 32, buf, true);
    snprintf(buf, sizeof(buf), " Z:%.2f", data.accel_z);
    oled.drawText(0, 42, buf, true);
    
    oled.drawText(0, 54, "Gyro (dps):", true);
    snprintf(buf, sizeof(buf), " X:%.1f", data.gyro_x);
    oled.drawText(0, 64, buf, true);
    snprintf(buf, sizeof(buf), " Y:%.1f", data.gyro_y);
    oled.drawText(0, 74, buf, true);
    snprintf(buf, sizeof(buf), " Z:%.1f", data.gyro_z);
    oled.drawText(0, 84, buf, true);
    
    oled.drawText(0, 96, "Mag (uT):", true);
    snprintf(buf, sizeof(buf), " X:%.1f", data.mag_x);
    oled.drawText(0, 106, buf, true);
    snprintf(buf, sizeof(buf), " Y:%.1f Z:%.1f", data.mag_y, data.mag_z);
    oled.drawText(0, 116, buf, true);
  }else{
    oled.drawText(10, 60, "NO IMU DATA", true);
  }
  
  oled.show();
}

/**
 * @brief Render environmental data page (temperature, humidity, pressure)
 */
inline void renderEnvironmentalPage(arcos::manager::OLEDDisplayManager& oled,
                                    const arcos::communication::SensorDataPayload& data) {
  oled.clear();
  oled.drawText(0, 0, "=== ENVIRONMENT ===", true);
  
  if(data.getEnvValid()){
    char buf[32];
    oled.drawText(0, 20, "Temperature:", true);
    snprintf(buf, sizeof(buf), "  %.2f C", data.temperature);
    oled.drawText(0, 32, buf, true);
    
    oled.drawText(0, 50, "Humidity:", true);
    snprintf(buf, sizeof(buf), "  %.1f %%", data.humidity);
    oled.drawText(0, 62, buf, true);
    
    oled.drawText(0, 80, "Pressure:", true);
    snprintf(buf, sizeof(buf), "  %.2f hPa", data.pressure / 100.0f);
    oled.drawText(0, 92, buf, true);
  }else{
    oled.drawText(10, 60, "NO ENV DATA", true);
  }
  
  oled.show();
}

/**
 * @brief Render GPS data page (position, navigation, time)
 */
inline void renderGpsPage(arcos::manager::OLEDDisplayManager& oled,
                          const arcos::communication::SensorDataPayload& data) {
  oled.clear();
  oled.drawText(0, 0, "===== GPS DATA =====", true);
  
  if(data.getGpsValid()){
    char buf[32];
    oled.drawText(0, 12, "Position:", true);
    snprintf(buf, sizeof(buf), " Lat:%.5f", data.latitude);
    oled.drawText(0, 22, buf, true);
    snprintf(buf, sizeof(buf), " Lon:%.5f", data.longitude);
    oled.drawText(0, 32, buf, true);
    snprintf(buf, sizeof(buf), " Alt:%.1fm", data.altitude);
    oled.drawText(0, 42, buf, true);
    
    oled.drawText(0, 54, "Navigation:", true);
    snprintf(buf, sizeof(buf), " Spd:%.1fkn", data.speed_knots);
    oled.drawText(0, 64, buf, true);
    snprintf(buf, sizeof(buf), " Crs:%.1fdeg", data.course);
    oled.drawText(0, 74, buf, true);
    
    oled.drawText(0, 86, "Status:", true);
    snprintf(buf, sizeof(buf), " Sats:%u Fix:%u", data.gps_satellites, data.getGpsFixQuality());
    oled.drawText(0, 96, buf, true);
    
    snprintf(buf, sizeof(buf), "Time: %02u:%02u:%02u", 
      data.gps_hour, data.gps_minute, data.gps_second);
    oled.drawText(0, 108, buf, true);
  }else{
    oled.drawText(10, 60, "NO GPS FIX", true);
  }
  
  oled.show();
}

/**
 * @brief Render microphone data page (level, peak, bar graph)
 */
inline void renderMicrophonePage(arcos::manager::OLEDDisplayManager& oled,
                                 const arcos::communication::SensorDataPayload& data) {
  oled.clear();
  oled.drawText(0, 0, "==== MIC DATA =====", true);
  
  if(data.getMicValid()){
    char buf[32];
    oled.drawText(0, 12, "Level:", true);
    snprintf(buf, sizeof(buf), " %.1f dB", data.mic_db_level);
    oled.drawText(42, 12, buf, true);
    
    if(data.getMicClipping()){
      oled.drawText(90, 12, "[CLIP]", true);
    }
    
    oled.drawText(0, 30, "Peak:", true);
    snprintf(buf, sizeof(buf), " %ld", data.mic_peak_amplitude);
    oled.drawText(36, 30, buf, true);
    
    // Simple level bar
    int bar_width = static_cast<int>((data.mic_db_level + 60.0f) / 60.0f * 100.0f);
    if(bar_width < 0) bar_width = 0;
    if(bar_width > 100) bar_width = 100;
    
    oled.drawRect(10, 50, 108, 20, false, true);
    oled.fillRect(12, 52, bar_width, 16, true);
  }else{
    oled.drawText(10, 60, "NO MIC DATA", true);
  }
  
  oled.show();
}

/**
 * @brief Render system info page (frame rates, sensors, buttons)
 */
inline void renderSystemInfoPage(arcos::manager::OLEDDisplayManager& oled,
                                  const arcos::communication::SensorDataPayload& data,
                                  uint32_t sensor_fps,
                                  uint32_t led_fps,
                                  uint8_t fan_speed) {
  oled.clear();
  oled.drawText(0, 0, "==== SYSTEM INFO ====", true);
  
  char buf[32];
  oled.drawText(0, 12, "Data Rate:", true);
  snprintf(buf, sizeof(buf), " RX:%lu TX:%lu FPS", sensor_fps, led_fps);
  oled.drawText(0, 22, buf, true);
  
  oled.drawText(0, 34, "Fan Speed:", true);
  snprintf(buf, sizeof(buf), " %u%%", (fan_speed * 100) / 255);
  oled.drawText(0, 44, buf, true);
  
  oled.drawText(0, 56, "Buttons:", true);
  snprintf(buf, sizeof(buf), " A:%u B:%u C:%u D:%u",
    data.getButtonA(), data.getButtonB(), data.getButtonC(), data.getButtonD());
  oled.drawText(0, 66, buf, true);
  
  oled.drawText(0, 78, "Sensors:", true);
  snprintf(buf, sizeof(buf), " IMU:%u ENV:%u",
    data.getImuValid(), data.getEnvValid());
  oled.drawText(0, 88, buf, true);
  snprintf(buf, sizeof(buf), " GPS:%u MIC:%u",
    data.getGpsValidFlag(), data.getMicValid());
  oled.drawText(0, 98, buf, true);
  
  oled.show();
}

} // namespace arcos::ui::oled

#endif // DEBUG_PAGES_HPP
