/*****************************************************************
 * File:      uart_cpu_bidirectional.cpp
 * Category:  communication/examples
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    CPU-side bidirectional UART communication main file.
 *    Simple setup and loop using CpuUartBidirectional class.
 *****************************************************************/

#include <Arduino.h>
#include "CpuUartBidirectional.h"

using namespace arcos::communication;

// Global instance
CpuUartBidirectional uart_comm;

void setup(){
  // Initialize UART communication
  if(!uart_comm.init()){
    Serial.println("CPU: Failed to initialize UART communication");
    while(1){
      delay(1000);
    }
  }
  
  Serial.println("CPU: Ready for 60Hz bidirectional data transfer\n");
  delay(100);
}

void loop(){
  // Process communication at 60Hz
  uart_comm.update();
}
