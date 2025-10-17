#ifndef GPS_PARSER_H
#define GPS_PARSER_H

#include <Arduino.h>
#include <HardwareSerial.h>

// Struct for location data
struct GpsLocation{
  double latitude;
  double longitude;
  float altitude;
};

// Struct for all GPS data
struct GpsData{
  double latitude;
  double longitude;
  float altitude;
  uint8_t satellites;
  uint8_t fix_quality;
  float speed_knots;
  float speed_kmh;
  String time_utc;
  unsigned long last_update_ms;
  int total_sentences;
  bool has_fix;
};

class GpsParser{
public:
  // Constructor
  GpsParser(uint8_t uart_num = 2);
  
  // Initialize GPS serial connection
  void begin(int rx_pin, int tx_pin, unsigned long baud_rate = 9600);
  
  // Update - call this frequently in loop() to process incoming data
  void update();
  
  // Getters for individual GPS data
  double getLatitude();
  double getLongitude();
  uint8_t getSatellites();
  uint8_t getFixQuality();
  float getAltitude();
  float getSpeedKnots();
  float getSpeedKmh();
  String getTimeUtc();
  unsigned long getLastUpdateMs();
  int getTotalSentences();
  
  // Get location data (lat, lon, alt)
  GpsLocation getLocation();
  
  // Get all GPS data at once
  GpsData getAll();
  
  // Check if we have a valid fix
  bool hasFix();
  
  // Get fix quality as string
  String getFixQualityString();

private:
  // Hardware serial instance
  HardwareSerial* gps_serial;
  
  // GPS data variables
  String nmea_sentence;
  double gps_latitude;
  double gps_longitude;
  uint8_t gps_satellites;
  uint8_t gps_fix_quality;
  float gps_altitude;
  float gps_speed_knots;
  String gps_time;
  unsigned long last_update;
  int total_sentences;
  
  // Parse NMEA sentences
  void parseGpgga(String sentence);
  void parseGprmc(String sentence);
};

#include "GpsParser.impl.hpp"

#endif // GPS_PARSER_H
