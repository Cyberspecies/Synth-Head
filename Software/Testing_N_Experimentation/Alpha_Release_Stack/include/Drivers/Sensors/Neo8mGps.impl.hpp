/*****************************************************************
 * File:      Neo8mGps.impl.hpp
 * Category:  drivers/sensors
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Implementation file for NEO-8M GPS module with NMEA
 *    sentence parsing via UART serial communication.
 *****************************************************************/

#ifndef ALPHA_RELEASE_STACK_DRIVERS_SENSORS_NEO8M_GPS_IMPL_HPP_
#define ALPHA_RELEASE_STACK_DRIVERS_SENSORS_NEO8M_GPS_IMPL_HPP_

namespace sensors{

Neo8mGps::Neo8mGps(uint8_t tx_pin, uint8_t rx_pin)
  : serial_(nullptr),
    tx_pin_(tx_pin),
    rx_pin_(rx_pin),
    initialized_(false),
    buffer_index_(0){
  
  // Initialize data structure
  current_data_.valid = false;
  current_data_.fix_quality = GpsFixQuality::NO_FIX;
  current_data_.satellites = 0;
  current_data_.latitude = 0.0f;
  current_data_.longitude = 0.0f;
  current_data_.altitude = 0.0f;
  current_data_.speed_knots = 0.0f;
  current_data_.course = 0.0f;
  current_data_.hour = 0;
  current_data_.minute = 0;
  current_data_.second = 0;
}

bool Neo8mGps::init(){
  if(initialized_){
    Serial.println("[NEO8M] Already initialized");
    return true;
  }

  // Initialize UART (Serial2 for ESP32-S3)
  serial_ = new HardwareSerial(2);
  Serial.printf("[NEO8M] Initializing UART: TX=%d, RX=%d, Baud=%lu\n", 
                tx_pin_, rx_pin_, GPS_BAUD);
  
  serial_->begin(GPS_BAUD, SERIAL_8N1, rx_pin_, tx_pin_);
  
  delay(100); // Allow UART to stabilize

  initialized_ = true;
  Serial.println("[NEO8M] Initialization complete");
  Serial.println("[NEO8M] Waiting for GPS fix...");
  
  return true;
}

bool Neo8mGps::update(){
  if(!initialized_){
    return false;
  }

  bool new_data = false;

  // Read all available characters
  while(serial_->available()){
    char c = serial_->read();
    
    // Start of NMEA sentence
    if(c == '$'){
      buffer_index_ = 0;
    }
    
    // Add character to buffer
    if(buffer_index_ < BUFFER_SIZE - 1){
      buffer_[buffer_index_++] = c;
    }
    
    // End of NMEA sentence
    if(c == '\n'){
      buffer_[buffer_index_] = '\0';
      
      // Parse the sentence
      if(parseNmeaSentence(buffer_)){
        new_data = true;
      }
      
      buffer_index_ = 0;
    }
  }

  return new_data;
}

bool Neo8mGps::getData(Neo8mGpsData& data){
  if(!initialized_ || !current_data_.valid){
    return false;
  }

  data = current_data_;
  return true;
}

bool Neo8mGps::parseNmeaSentence(const char* sentence){
  // Check if sentence starts with $
  if(sentence[0] != '$'){
    return false;
  }

  // Validate checksum
  if(!validateChecksum(sentence)){
    return false;
  }

  // Parse GGA sentence (position and fix)
  if(strncmp(sentence + 3, "GGA", 3) == 0){
    return parseGGA(sentence);
  }
  
  // Parse RMC sentence (recommended minimum)
  if(strncmp(sentence + 3, "RMC", 3) == 0){
    return parseRMC(sentence);
  }

  return false;
}

bool Neo8mGps::parseGGA(const char* sentence){
  // $GPGGA,HHMMSS.SS,LLLL.LL,N,YYYYY.YY,E,Q,NN,D.D,AAA.A,M,GGG.G,M,,*CS
  
  char buffer[128];
  strncpy(buffer, sentence, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';
  
  char* token = strtok(buffer, ",");
  int field = 0;
  
  float time_utc = 0;
  float lat = 0;
  char lat_dir = 'N';
  float lon = 0;
  char lon_dir = 'E';
  int quality = 0;
  int sats = 0;
  float alt = 0;
  
  while(token != nullptr && field < 15){
    switch(field){
      case 1: // Time UTC
        time_utc = atof(token);
        break;
      case 2: // Latitude
        lat = atof(token);
        break;
      case 3: // Latitude direction
        lat_dir = token[0];
        break;
      case 4: // Longitude
        lon = atof(token);
        break;
      case 5: // Longitude direction
        lon_dir = token[0];
        break;
      case 6: // Fix quality
        quality = atoi(token);
        break;
      case 7: // Number of satellites
        sats = atoi(token);
        break;
      case 9: // Altitude
        alt = atof(token);
        break;
    }
    token = strtok(nullptr, ",");
    field++;
  }
  
  // Update data if we have a fix
  if(quality > 0 && sats > 0){
    current_data_.latitude = nmeaToDecimal(lat, lat_dir);
    current_data_.longitude = nmeaToDecimal(lon, lon_dir);
    current_data_.altitude = alt;
    current_data_.fix_quality = static_cast<GpsFixQuality>(quality);
    current_data_.satellites = sats;
    current_data_.valid = true;
    
    // Parse time
    int time_int = static_cast<int>(time_utc);
    current_data_.hour = time_int / 10000;
    current_data_.minute = (time_int / 100) % 100;
    current_data_.second = time_int % 100;
    
    return true;
  }
  
  return false;
}

bool Neo8mGps::parseRMC(const char* sentence){
  // $GPRMC,HHMMSS.SS,A,LLLL.LL,N,YYYYY.YY,E,SSS.S,CCC.C,DDMMYY,,,A*CS
  
  char buffer[128];
  strncpy(buffer, sentence, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';
  
  char* token = strtok(buffer, ",");
  int field = 0;
  
  char status = 'V';
  float speed = 0;
  float course = 0;
  
  while(token != nullptr && field < 12){
    switch(field){
      case 2: // Status (A = valid, V = invalid)
        status = token[0];
        break;
      case 7: // Speed in knots
        speed = atof(token);
        break;
      case 8: // Course over ground
        course = atof(token);
        break;
    }
    token = strtok(nullptr, ",");
    field++;
  }
  
  // Update speed and course if status is valid
  if(status == 'A'){
    current_data_.speed_knots = speed;
    current_data_.course = course;
    return true;
  }
  
  return false;
}

float Neo8mGps::nmeaToDecimal(float nmea_coord, char direction){
  // NMEA format: DDMM.MMMM or DDDMM.MMMM
  int degrees = static_cast<int>(nmea_coord / 100);
  float minutes = nmea_coord - (degrees * 100);
  
  float decimal = degrees + (minutes / 60.0f);
  
  // Apply direction (South and West are negative)
  if(direction == 'S' || direction == 'W'){
    decimal = -decimal;
  }
  
  return decimal;
}

bool Neo8mGps::validateChecksum(const char* sentence){
  // Find the * character
  const char* asterisk = strchr(sentence, '*');
  if(asterisk == nullptr){
    return false;
  }
  
  // Calculate checksum (XOR of all characters between $ and *)
  uint8_t checksum = 0;
  for(const char* p = sentence + 1; p < asterisk; p++){
    checksum ^= *p;
  }
  
  // Parse the checksum from the sentence
  uint8_t sentence_checksum = strtol(asterisk + 1, nullptr, 16);
  
  return checksum == sentence_checksum;
}

void Neo8mGps::printData(const Neo8mGpsData& data) const{
  const char* fix_str = "NO FIX";
  if(data.fix_quality == GpsFixQuality::GPS_FIX) fix_str = "GPS   ";
  else if(data.fix_quality == GpsFixQuality::DGPS_FIX) fix_str = "DGPS  ";
  
  Serial.printf("GPS: Lat=%10.6f° Lon=%11.6f° Alt=%7.2fm | Fix: %s Sats:%2d | Speed:%6.2fkn Course:%6.2f° | Time: %02d:%02d:%02d UTC\n",
                data.latitude, data.longitude, data.altitude,
                fix_str, data.satellites,
                data.speed_knots, data.course,
                data.hour, data.minute, data.second);
}

} // namespace sensors

#endif // ALPHA_RELEASE_STACK_DRIVERS_SENSORS_NEO8M_GPS_IMPL_HPP_
