# Refactored UART Bidirectional Communication

## Overview
The UART bidirectional communication system has been refactored to separate implementation from main application logic. This makes the code more maintainable and easier to understand.

## File Structure

### Protocol Definition
- `include/UartBidirectionalProtocol.h` - Core protocol definitions shared by both CPU and GPU

### CPU Side
- `include/CpuUartBidirectional.h` - CPU class header with configuration constants
- `src/CpuUartBidirectional.cpp` - Full CPU UART implementation (~307 lines)
- `src/uart_cpu_bidirectional.cpp` - Main CPU application (37 lines)

### GPU Side
- `include/GpuUartBidirectional.h` - GPU class header with configuration constants
- `src/GpuUartBidirectional.cpp` - Full GPU UART implementation (~293 lines)
- `src/uart_gpu_bidirectional.cpp` - Main GPU application (55 lines)

## Key Features

### Easy Configuration
All critical parameters are defined as constants at the top of the header files:

**CPU Configuration** (`CpuUartBidirectional.h`):
```cpp
constexpr int CPU_RX_PIN = 11;
constexpr int CPU_TX_PIN = 12;
constexpr uart_port_t CPU_UART_NUM = UART_NUM_2;
constexpr int CPU_SEND_BYTES = 40;   // 316 bits rounded up
constexpr int CPU_RECV_BYTES = 196;  // 1568 bits
constexpr int TARGET_FPS = 60;
constexpr int BAUD_RATE = 921600;
```

**GPU Configuration** (`GpuUartBidirectional.h`):
```cpp
constexpr int GPU_TX_PIN = 12;
constexpr int GPU_RX_PIN = 13;
constexpr uart_port_t GPU_UART_NUM = UART_NUM_1;
constexpr int GPU_SEND_BYTES = 196;  // 1568 bits
constexpr int GPU_RECV_BYTES = 40;   // 316 bits rounded up
constexpr int GPU_TARGET_FPS = 60;
constexpr int GPU_BAUD_RATE = 921600;
```

### Simple Main Files

**CPU Main** (`uart_cpu_bidirectional.cpp`):
```cpp
#include <Arduino.h>
#include "CpuUartBidirectional.h"

CpuUartBidirectional uart_comm;

void setup(){
  uart_comm.init();
  Serial.println("CPU: Ready for 60Hz bidirectional data transfer\n");
}

void loop(){
  uart_comm.update();
}
```

**GPU Main** (`uart_gpu_bidirectional.cpp`):
```cpp
#include "GpuUartBidirectional.h"

GpuUartBidirectional uart_comm;

void uart_communication_task(void* pvParameters){
  while(1){
    uart_comm.update();
    vTaskDelay(1);
  }
}

extern "C" void app_main(void){
  uart_comm.init();
  xTaskCreate(uart_communication_task, "uart_comm", 4096, NULL, 5, NULL);
}
```

## Implementation Details

### CPU Implementation
- ESP-IDF UART2 driver for high performance
- Non-blocking packet reception (max 5 packets per cycle)
- 60Hz frame transmission
- Sequence number tracking for packet loss detection
- Analytics display with progress bars
- Automatic checksum validation

### GPU Implementation
- ESP-IDF UART1 driver
- FreeRTOS task-based operation
- 2048-byte UART buffers
- Sequence number tracking
- ESP logging system for analytics
- Task watchdog disabled via sdkconfig

## Performance Metrics
- Baud Rate: 921600
- CPU TX: ~62.5 fps, ~22.5 kbps
- CPU RX: ~48.5 fps, ~78.4 kbps
- GPU TX: ~49.0 fps, ~78.8 kbps
- GPU RX: ~62.5 fps, ~23.0 kbps
- Packet Loss: 0%
- Errors: 0

## Modifying Data Sizes

To change the amount of data transmitted:

1. **CPU Side** - Edit `include/CpuUartBidirectional.h`:
   ```cpp
   constexpr int CPU_SEND_BYTES = 40;   // Change this value
   constexpr int CPU_RECV_BYTES = 196;  // Change this value
   ```

2. **GPU Side** - Edit `include/GpuUartBidirectional.h`:
   ```cpp
   constexpr int GPU_SEND_BYTES = 196;  // Change this value
   constexpr int GPU_RECV_BYTES = 40;   // Change this value
   ```

**Important**: CPU_SEND_BYTES must equal GPU_RECV_BYTES, and CPU_RECV_BYTES must equal GPU_SEND_BYTES.

Maximum payload size is defined in the protocol as `MAX_PAYLOAD_SIZE = 256` bytes.

## Build Configuration

The project uses PlatformIO with environment-specific source filters:

- **CPU Environment**: Compiles `uart_cpu_bidirectional.cpp` and `CpuUartBidirectional.cpp`
- **GPU Environment**: Compiles `uart_gpu_bidirectional.cpp` and `GpuUartBidirectional.cpp`

Build commands:
```bash
pio run -e CPU     # Build CPU firmware
pio run -e GPU     # Build GPU firmware
```

## Benefits of Refactoring

1. **Maintainability**: Implementation separated from application logic
2. **Readability**: Main files are ~37-55 lines instead of 300+
3. **Reusability**: Implementation classes can be used in other projects
4. **Configurability**: All parameters in one place (header files)
5. **Testability**: Classes can be unit tested independently
