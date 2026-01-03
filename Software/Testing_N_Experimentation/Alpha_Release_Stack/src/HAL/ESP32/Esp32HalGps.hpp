/*****************************************************************
 * File:      Esp32HalGps.hpp
 * Category:  src/HAL/ESP32
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    ESP32 implementation of GPS HAL interface.
 *    Designed for NEO-8M and similar NMEA GPS modules.
 *    Includes NMEA sentence parsing.
 *****************************************************************/

#ifndef ARCOS_SRC_HAL_ESP32_HAL_GPS_HPP_
#define ARCOS_SRC_HAL_ESP32_HAL_GPS_HPP_

#include "HAL/IHalGps.hpp"
#include "HAL/IHalLog.hpp"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <math.h>

namespace arcos::hal::esp32{

/** ESP32 GPS Implementation with NMEA parsing */
class Esp32HalGps : public IHalGps{
private:
  static constexpr const char* TAG = "GPS";
  static constexpr size_t NMEA_MAX_LENGTH = 128;
  static constexpr float KNOTS_TO_KMH = 1.852f;
  static constexpr float KNOTS_TO_MPS = 0.514444f;
  static constexpr float DEGREES_TO_RADIANS = 0.01745329252f;
  static constexpr float RADIANS_TO_DEGREES = 57.29577951f;
  static constexpr float EARTH_RADIUS_M = 6371000.0f;
  
  IHalLog* log_ = nullptr;
  HardwareSerial* serial_ = nullptr;
  GpsConfig config_;
  bool initialized_ = false;
  
  // GPS data
  GpsData gps_data_;
  
  // NMEA parsing buffer
  char nmea_buffer_[NMEA_MAX_LENGTH];
  size_t nmea_index_ = 0;
  
  // Serial port allocation
  uint8_t serial_port_ = 1;
  
  // Parse NMEA sentence
  bool parseNmea(const char* sentence){
    if(!sentence || sentence[0] != '$') return false;
    
    // Verify checksum
    if(!verifyChecksum(sentence)) return false;
    
    // Determine sentence type
    if(strncmp(sentence + 3, "GGA", 3) == 0){
      return parseGGA(sentence);
    } else if(strncmp(sentence + 3, "RMC", 3) == 0){
      return parseRMC(sentence);
    } else if(strncmp(sentence + 3, "VTG", 3) == 0){
      return parseVTG(sentence);
    }
    
    return false;
  }
  
  bool verifyChecksum(const char* sentence){
    const char* asterisk = strchr(sentence, '*');
    if(!asterisk) return false;
    
    uint8_t checksum = 0;
    for(const char* p = sentence + 1; p < asterisk; p++){
      checksum ^= *p;
    }
    
    uint8_t provided = strtol(asterisk + 1, nullptr, 16);
    return checksum == provided;
  }
  
  // Parse GGA sentence (fix data)
  bool parseGGA(const char* sentence){
    char* fields[15];
    char buffer[NMEA_MAX_LENGTH];
    strncpy(buffer, sentence, NMEA_MAX_LENGTH - 1);
    buffer[NMEA_MAX_LENGTH - 1] = '\0';
    
    int field_count = splitFields(buffer, fields, 15);
    if(field_count < 14) return false;
    
    // Time (field 1) - HHMMSS.ss
    if(strlen(fields[1]) >= 6){
      gps_data_.time.hour = (fields[1][0] - '0') * 10 + (fields[1][1] - '0');
      gps_data_.time.minute = (fields[1][2] - '0') * 10 + (fields[1][3] - '0');
      gps_data_.time.second = (fields[1][4] - '0') * 10 + (fields[1][5] - '0');
      if(strlen(fields[1]) > 7){
        gps_data_.time.millisecond = atoi(fields[1] + 7) * 10;
      }
    }
    
    // Latitude (fields 2-3)
    if(strlen(fields[2]) > 0){
      gps_data_.position.latitude = parseLatLon(fields[2], fields[3][0]);
    }
    
    // Longitude (fields 4-5)
    if(strlen(fields[4]) > 0){
      gps_data_.position.longitude = parseLatLon(fields[4], fields[5][0]);
    }
    
    // Fix quality (field 6)
    int quality = atoi(fields[6]);
    gps_data_.fix_quality = static_cast<GpsFixQuality>(quality);
    
    // Satellites used (field 7)
    gps_data_.satellites_used = atoi(fields[7]);
    
    // HDOP (field 8)
    gps_data_.position.hdop = atof(fields[8]);
    
    // Altitude (field 9)
    gps_data_.position.altitude = atof(fields[9]);
    
    gps_data_.position.valid = (quality > 0);
    gps_data_.time.valid = true;
    gps_data_.timestamp = millis();
    
    return true;
  }
  
  // Parse RMC sentence (recommended minimum)
  bool parseRMC(const char* sentence){
    char* fields[15];
    char buffer[NMEA_MAX_LENGTH];
    strncpy(buffer, sentence, NMEA_MAX_LENGTH - 1);
    buffer[NMEA_MAX_LENGTH - 1] = '\0';
    
    int field_count = splitFields(buffer, fields, 15);
    if(field_count < 12) return false;
    
    // Status (field 2) - A=valid, V=invalid
    bool valid = (fields[2][0] == 'A');
    
    // Latitude (fields 3-4)
    if(strlen(fields[3]) > 0 && valid){
      gps_data_.position.latitude = parseLatLon(fields[3], fields[4][0]);
    }
    
    // Longitude (fields 5-6)
    if(strlen(fields[5]) > 0 && valid){
      gps_data_.position.longitude = parseLatLon(fields[5], fields[6][0]);
    }
    
    // Speed in knots (field 7)
    if(strlen(fields[7]) > 0){
      gps_data_.velocity.speed_knots = atof(fields[7]);
      gps_data_.velocity.speed_kmh = gps_data_.velocity.speed_knots * KNOTS_TO_KMH;
      gps_data_.velocity.speed_mps = gps_data_.velocity.speed_knots * KNOTS_TO_MPS;
      gps_data_.velocity.valid = valid;
    }
    
    // Course (field 8)
    if(strlen(fields[8]) > 0){
      gps_data_.velocity.course = atof(fields[8]);
    }
    
    // Date (field 9) - DDMMYY
    if(strlen(fields[9]) >= 6){
      gps_data_.time.day = (fields[9][0] - '0') * 10 + (fields[9][1] - '0');
      gps_data_.time.month = (fields[9][2] - '0') * 10 + (fields[9][3] - '0');
      gps_data_.time.year = 2000 + (fields[9][4] - '0') * 10 + (fields[9][5] - '0');
    }
    
    gps_data_.position.valid = valid;
    gps_data_.timestamp = millis();
    
    return true;
  }
  
  // Parse VTG sentence (velocity)
  bool parseVTG(const char* sentence){
    char* fields[12];
    char buffer[NMEA_MAX_LENGTH];
    strncpy(buffer, sentence, NMEA_MAX_LENGTH - 1);
    buffer[NMEA_MAX_LENGTH - 1] = '\0';
    
    int field_count = splitFields(buffer, fields, 12);
    if(field_count < 9) return false;
    
    // Course true (field 1)
    if(strlen(fields[1]) > 0){
      gps_data_.velocity.course = atof(fields[1]);
    }
    
    // Speed in knots (field 5)
    if(strlen(fields[5]) > 0){
      gps_data_.velocity.speed_knots = atof(fields[5]);
    }
    
    // Speed in km/h (field 7)
    if(strlen(fields[7]) > 0){
      gps_data_.velocity.speed_kmh = atof(fields[7]);
      gps_data_.velocity.speed_mps = gps_data_.velocity.speed_kmh / 3.6f;
      gps_data_.velocity.valid = true;
    }
    
    return true;
  }
  
  double parseLatLon(const char* field, char direction){
    double raw = atof(field);
    int degrees = static_cast<int>(raw / 100);
    double minutes = raw - (degrees * 100);
    double result = degrees + (minutes / 60.0);
    
    if(direction == 'S' || direction == 'W'){
      result = -result;
    }
    
    return result;
  }
  
  int splitFields(char* str, char** fields, int max_fields){
    int count = 0;
    char* token = strtok(str, ",");
    while(token && count < max_fields){
      fields[count++] = token;
      token = strtok(nullptr, ",");
    }
    return count;
  }

public:
  explicit Esp32HalGps(IHalLog* log = nullptr) : log_(log){}
  
  ~Esp32HalGps() override{
    deinit();
  }
  
  HalResult init(const GpsConfig& config) override{
    if(initialized_){
      if(log_) log_->warn(TAG, "GPS already initialized");
      return HalResult::ALREADY_INITIALIZED;
    }
    
    config_ = config;
    
    // Use Serial1 or Serial2
    serial_port_ = 1;  // Default to Serial1
    serial_ = &Serial1;
    
    // Initialize serial port with GPS pins
    serial_->begin(config_.baud_rate, SERIAL_8N1, config_.rx_pin, config_.tx_pin);
    
    // Clear buffer
    while(serial_->available()){
      serial_->read();
    }
    
    nmea_index_ = 0;
    memset(nmea_buffer_, 0, NMEA_MAX_LENGTH);
    
    initialized_ = true;
    if(log_) log_->info(TAG, "GPS initialized: TX=%d, RX=%d, baud=%lu",
                        config_.tx_pin, config_.rx_pin, config_.baud_rate);
    return HalResult::OK;
  }
  
  HalResult deinit() override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
    serial_->end();
    serial_ = nullptr;
    
    initialized_ = false;
    if(log_) log_->info(TAG, "GPS deinitialized");
    return HalResult::OK;
  }
  
  bool isInitialized() const override{
    return initialized_;
  }
  
  HalResult update() override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    
    bool new_data = false;
    
    while(serial_->available()){
      char c = serial_->read();
      
      if(c == '$'){
        // Start of new NMEA sentence
        nmea_index_ = 0;
      }
      
      if(nmea_index_ < NMEA_MAX_LENGTH - 1){
        nmea_buffer_[nmea_index_++] = c;
        nmea_buffer_[nmea_index_] = '\0';
      }
      
      if(c == '\n'){
        // End of sentence, parse it
        if(parseNmea(nmea_buffer_)){
          new_data = true;
        }
        nmea_index_ = 0;
      }
    }
    
    return new_data ? HalResult::OK : HalResult::NO_DATA;
  }
  
  HalResult getData(GpsData& data) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    data = gps_data_;
    return HalResult::OK;
  }
  
  HalResult getPosition(GpsPosition& position) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    position = gps_data_.position;
    return HalResult::OK;
  }
  
  HalResult getVelocity(GpsVelocity& velocity) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    velocity = gps_data_.velocity;
    return HalResult::OK;
  }
  
  HalResult getTime(GpsTime& time) override{
    if(!initialized_) return HalResult::NOT_INITIALIZED;
    time = gps_data_.time;
    return HalResult::OK;
  }
  
  bool hasFix() const override{
    return gps_data_.hasFix();
  }
  
  GpsFixQuality getFixQuality() const override{
    return gps_data_.fix_quality;
  }
  
  uint8_t getSatellites() const override{
    return gps_data_.satellites_used;
  }
  
  float distanceTo(double lat, double lon) override{
    if(!gps_data_.position.valid) return 0.0f;
    
    // Haversine formula
    double lat1 = gps_data_.position.latitude * DEGREES_TO_RADIANS;
    double lat2 = lat * DEGREES_TO_RADIANS;
    double delta_lat = (lat - gps_data_.position.latitude) * DEGREES_TO_RADIANS;
    double delta_lon = (lon - gps_data_.position.longitude) * DEGREES_TO_RADIANS;
    
    double a = sin(delta_lat / 2) * sin(delta_lat / 2) +
               cos(lat1) * cos(lat2) *
               sin(delta_lon / 2) * sin(delta_lon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    
    return EARTH_RADIUS_M * c;
  }
  
  float bearingTo(double lat, double lon) override{
    if(!gps_data_.position.valid) return 0.0f;
    
    double lat1 = gps_data_.position.latitude * DEGREES_TO_RADIANS;
    double lat2 = lat * DEGREES_TO_RADIANS;
    double delta_lon = (lon - gps_data_.position.longitude) * DEGREES_TO_RADIANS;
    
    double x = sin(delta_lon) * cos(lat2);
    double y = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(delta_lon);
    
    float bearing = atan2(x, y) * RADIANS_TO_DEGREES;
    
    // Normalize to 0-360
    if(bearing < 0) bearing += 360.0f;
    
    return bearing;
  }
};

} // namespace arcos::hal::esp32

#endif // ARCOS_SRC_HAL_ESP32_HAL_GPS_HPP_
