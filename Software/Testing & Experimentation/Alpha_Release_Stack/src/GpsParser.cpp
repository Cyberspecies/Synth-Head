#include "GpsParser.h"

// Constructor
GpsParser::GpsParser(uint8_t uart_num){
  gps_serial = new HardwareSerial(uart_num);
  gps_latitude = 0.0;
  gps_longitude = 0.0;
  gps_satellites = 0;
  gps_fix_quality = 0;
  gps_altitude = 0.0;
  gps_speed_knots = 0.0;
  gps_time = "";
  last_update = 0;
  total_sentences = 0;
  nmea_sentence = "";
}

// Initialize GPS serial connection
void GpsParser::begin(int rx_pin, int tx_pin, unsigned long baud_rate){
  gps_serial->begin(baud_rate, SERIAL_8N1, rx_pin, tx_pin);
}

// Update - call this frequently in loop() to process incoming data
void GpsParser::update(){
  while(gps_serial->available() > 0){
    char c = gps_serial->read();
    
    // Build NMEA sentence
    if(c == '$'){
      nmea_sentence = "$";  // Start new sentence
      total_sentences++;
    }else if(c == '\n'){
      // End of sentence - parse it
      if(nmea_sentence.startsWith("$GPGGA") || nmea_sentence.startsWith("$GNGGA")){
        parseGpgga(nmea_sentence);
      }else if(nmea_sentence.startsWith("$GPRMC") || nmea_sentence.startsWith("$GNRMC")){
        parseGprmc(nmea_sentence);
      }
      nmea_sentence = "";
    }else if(nmea_sentence.length() < 100){  // Prevent overflow
      nmea_sentence += c;
    }
  }
}

// Getters
double GpsParser::getLatitude(){
  return gps_latitude;
}

double GpsParser::getLongitude(){
  return gps_longitude;
}

uint8_t GpsParser::getSatellites(){
  return gps_satellites;
}

uint8_t GpsParser::getFixQuality(){
  return gps_fix_quality;
}

float GpsParser::getAltitude(){
  return gps_altitude;
}

float GpsParser::getSpeedKnots(){
  return gps_speed_knots;
}

float GpsParser::getSpeedKmh(){
  return gps_speed_knots * 1.852;
}

String GpsParser::getTimeUtc(){
  return gps_time;
}

unsigned long GpsParser::getLastUpdateMs(){
  return millis() - last_update;
}

int GpsParser::getTotalSentences(){
  return total_sentences;
}

bool GpsParser::hasFix(){
  return gps_fix_quality > 0;
}

String GpsParser::getFixQualityString(){
  switch(gps_fix_quality){
    case 0: return "No Fix";
    case 1: return "GPS Fix";
    case 2: return "DGPS Fix";
    default: return String(gps_fix_quality);
  }
}

// Get location data (lat, lon, alt) - more efficient than calling 3 separate getters
GpsLocation GpsParser::getLocation(){
  GpsLocation loc;
  loc.latitude = gps_latitude;
  loc.longitude = gps_longitude;
  loc.altitude = gps_altitude;
  return loc;
}

// Get all GPS data at once - most efficient for getting all information
GpsData GpsParser::getAll(){
  GpsData data;
  data.latitude = gps_latitude;
  data.longitude = gps_longitude;
  data.altitude = gps_altitude;
  data.satellites = gps_satellites;
  data.fix_quality = gps_fix_quality;
  data.speed_knots = gps_speed_knots;
  data.speed_kmh = gps_speed_knots * 1.852;
  data.time_utc = gps_time;
  data.last_update_ms = millis() - last_update;
  data.total_sentences = total_sentences;
  data.has_fix = gps_fix_quality > 0;
  return data;
}

// Parse NMEA GPGGA/GNGGA sentence: $GPGGA or $GNGGA,time,lat,N/S,lon,E/W,fix,sats,hdop,alt,M,...
// Example: $GPGGA,135025.00,3343.61042,S,15055.10503,E,1,07,1.17,79.1,M,19.6,M,,*75
// Example: $GNGGA,110827.00,4114.32485,N,00831.79799,W,1,10,0.93,130.6,M,50.1,M,*5F
void GpsParser::parseGpgga(String sentence){
  int field_index = 0;
  int start_pos = 0;
  String fields[15];
  
  // Split by commas
  for(int i = 0; i < sentence.length(); i++){
    if(sentence[i] == ',' || sentence[i] == '*'){
      fields[field_index++] = sentence.substring(start_pos, i);
      start_pos = i + 1;
      if(field_index >= 15) break;
    }
  }
  
  if(field_index >= 10 && (fields[0] == "$GPGGA" || fields[0] == "$GNGGA")){
    // Field 1: Time (HHMMSS.SS)
    if(fields[1].length() >= 6){
      gps_time = fields[1].substring(0, 2) + ":" + 
                 fields[1].substring(2, 4) + ":" + 
                 fields[1].substring(4, 6);
    }
    
    // Field 6: Fix quality (0=none, 1=GPS, 2=DGPS)
    gps_fix_quality = fields[6].toInt();
    
    // Field 7: Number of satellites
    gps_satellites = fields[7].toInt();
    
    // Field 2-3: Latitude (DDMM.MMMMM format)
    if(fields[2].length() > 0 && fields[3].length() > 0){
      double lat_raw = fields[2].toDouble();
      int lat_degrees = (int)(lat_raw / 100);
      double lat_minutes = lat_raw - (lat_degrees * 100);
      gps_latitude = lat_degrees + (lat_minutes / 60.0);
      if(fields[3] == "S") gps_latitude = -gps_latitude;
    }
    
    // Field 4-5: Longitude (DDDMM.MMMMM format)
    if(fields[4].length() > 0 && fields[5].length() > 0){
      double lon_raw = fields[4].toDouble();
      int lon_degrees = (int)(lon_raw / 100);
      double lon_minutes = lon_raw - (lon_degrees * 100);
      gps_longitude = lon_degrees + (lon_minutes / 60.0);
      if(fields[5] == "W") gps_longitude = -gps_longitude;
    }
    
    // Field 9: Altitude in meters
    if(fields[9].length() > 0){
      gps_altitude = fields[9].toFloat();
    }
    
    last_update = millis();
  }
}

// Parse NMEA GPRMC/GNRMC sentence: $GPRMC or $GNRMC,time,status,lat,N/S,lon,E/W,speed,track,date,...
// Example: $GPRMC,135026.00,A,3343.61039,S,15055.10501,E,0.146,,151025,,,A*64
// Example: $GNRMC,110827.00,A,4114.32485,N,00831.79799,W,0.0,,date,,,A*XX
void GpsParser::parseGprmc(String sentence){
  int field_index = 0;
  int start_pos = 0;
  String fields[13];
  
  // Split by commas
  for(int i = 0; i < sentence.length(); i++){
    if(sentence[i] == ',' || sentence[i] == '*'){
      fields[field_index++] = sentence.substring(start_pos, i);
      start_pos = i + 1;
      if(field_index >= 13) break;
    }
  }
  
  if(field_index >= 8 && (fields[0] == "$GPRMC" || fields[0] == "$GNRMC")){
    // Field 7: Speed over ground in knots
    if(fields[7].length() > 0){
      gps_speed_knots = fields[7].toFloat();
    }
  }
}
