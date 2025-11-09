/*****************************************************************
 * File:      uart_gpu_bidirectional.cpp
 * Category:  communication/examples
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    GPU-side bidirectional UART communication main file.
 *    Simple task and app_main using GpuUartBidirectional class.
 *****************************************************************/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "GpuUartBidirectional.h"

using namespace arcos::communication;

static const char* TAG = "GPU_MAIN";

// Global instance
GpuUartBidirectional uart_comm;

/** UART communication task */
void uart_communication_task(void* pvParameters){
  ESP_LOGI(TAG, "UART communication task started");
  
  while(1){
    uart_comm.update();
    vTaskDelay(1);  // Minimal delay for other tasks
  }
}

/** Application entry point */
extern "C" void app_main(void){
  ESP_LOGI(TAG, "Starting GPU UART bidirectional communication");
  
  // Initialize UART
  if(!uart_comm.init()){
    ESP_LOGE(TAG, "Failed to initialize UART");
    return;
  }
  
  // Create communication task
  xTaskCreate(
    uart_communication_task,
    "uart_comm",
    4096,
    NULL,
    5,
    NULL
  );
  
  ESP_LOGI(TAG, "GPU ready for 60Hz bidirectional data transfer");
}
