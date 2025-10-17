# ARCOS Coding Style Guide

## Overview

This document defines the coding style for the ARCOS hardware abstraction framework. Consistency in style improves readability and maintainability across the codebase.

## Indentation

- **Use 2 spaces** for indentation (no tabs)
- Indent continuation lines by 2 spaces

```cpp
if(condition){
  doSomething();
  doSomethingElse();
}
```

## Braces

- **Tight brace style**: No space before opening brace `{`
- Opening brace on same line as statement
- Closing brace on its own line

```cpp
// Correct
if(condition){
  doSomething();
}

// Incorrect
if(condition) {
  doSomething();
}
```

### Functions

```cpp
void myFunction(){
  // body
}

class MyClass{
public:
  void method(){
    // body
  }
};
```

### Control Structures

```cpp
if(condition){
  // body
}else if(other_condition){
  // body
}else{
  // body
}

for(int i = 0; i < count; i++){
  // body
}

while(running){
  // body
}

switch(value){
  case 1:
    break;
  case 2:
    break;
  default:
    break;
}
```

## Naming Conventions

### Variables

- **snake_case** for local variables and member variables
- **ALL_CAPS** for constants and macros

```cpp
int buffer_size = 0;
uint16_t* front_buffer = nullptr;
bool is_initialized = false;

constexpr int MAX_PANELS = 4;
constexpr int R0_BIT = 0;
```

### Functions and Methods

- **camelCase** for functions and methods
- Start with lowercase letter
- Descriptive names

```cpp
void initializeDriver();
bool setPixel(int x, int y, const RGB& color);
uint16_t* getWritableBuffer();
```

### Classes and Structs

- **PascalCase** for class and struct names
- Start with uppercase letter

```cpp
class HUB75Driver{
  // ...
};

struct HUB75Config{
  // ...
};

struct RGB{
  uint8_t r, g, b;
};
```

### Interfaces

- Prefix with `I` for interface classes

```cpp
class IHUB75Protocol{
public:
  virtual bool init() = 0;
  virtual ~IHUB75Protocol() = default;
};
```

### Namespaces

- **snake_case** for nested namespaces
- Use `arcos::abstraction::drivers` hierarchy

```cpp
namespace arcos::abstraction::drivers{
  // Driver implementations
}

namespace arcos::abstraction::parallel{
  // Parallel protocol abstractions
}
```

## Spacing

### Operators

- Space around binary operators
- No space after unary operators

```cpp
int result = a + b;
int value = -x;
bool flag = !condition;
int index = i++;
```

### Parentheses

- No space after function name
- No space inside parentheses
- Space after control structure keywords

```cpp
// Functions
myFunction(arg1, arg2);
int value = calculate(x, y);

// Control structures
if(condition){
  // body
}

for(int i = 0; i < count; i++){
  // body
}
```

### Pointer and Reference Declarations

- Attach `*` and `&` to the type, not the variable

```cpp
uint16_t* buffer;
const RGB& color;
IParallelHardware* hardware;
```

## Comments

### File Headers

All files must include a standardized header with the following format:

```cpp
/*****************************************************************
 * File:      driver_hub75.hpp
 * Category:  abstraction/drivers/components/HUB75
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    HUB75 LED matrix display driver abstraction with protocol
 *    layer separation for modular hardware backend support.
 *****************************************************************/
```

**Required Fields:**
- `File:` - The filename with extension
- `Category:` - The category path (e.g., abstraction, abstraction/drivers)
- `Author:` - Author name and organization
- `Purpose:` - Brief description of the file's functionality (can be multi-line)

**Example for HAL file:**
```cpp
/*****************************************************************
 * File:      hal_gpio_digital.hpp
 * Category:  abstraction
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Provides a template for GPIO HAL in both single and parallel
 *    operations of digital pins. Compile time HAL abstraction for
 *    a single port with zero runtime overhead.
 *****************************************************************/
```

### Documentation Comments

- Use `/** ... */` for documentation
- Document public APIs, parameters, and return values

```cpp
/** Initialize the driver with injected protocol implementation
 * @param config Display configuration 
 * @param protocol Pointer to IHUB75Protocol implementation (REQUIRED)
 * @return true if initialization successful
 */
bool init(const HUB75Config& config, IHUB75Protocol* protocol);
```

### Inline Comments

- Use `//` for single-line comments
- Use `/** ... */` for multi-line explanations

```cpp
// Simple inline comment
int value = 42;

/** Complex multi-line explanation
 *  Additional context about the implementation
 *  More details here
 */
```

## Structure

### Header Guards

- Use `#ifndef` / `#define` / `#endif`
- Format: `PROJECT_FOLDERPATH-FROM-PROJECT-ROOT_FILENAME_HPP_`

```cpp
#ifndef ARCOS_ABSTRACTION_DRIVERS_DRIVER_HUB75_HPP_
#define ARCOS_ABSTRACTION_DRIVERS_DRIVER_HUB75_HPP_

// content

#endif // ARCOS_ABSTRACTION_DRIVERS_DRIVER_HUB75_HPP_
```

### Include Order

1. Corresponding header (for .cpp files)
2. C system headers
3. C++ standard library headers
4. Platform/framework headers
5. Project headers (relative paths)

```cpp
#include "driver_hub75.hpp"

#include <stdint.h>
#include <cstring>
#include <cmath>

#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "../../../core/platform_hal.hpp"
```

### Class Organization

```cpp
class MyClass{
public:
  // Public types and constants
  enum Mode { MODE_A, MODE_B };
  
  // Constructors and destructor
  MyClass();
  ~MyClass();
  
  // Public methods
  bool init();
  void process();
  
private:
  // Private types
  struct InternalData{
    int value;
  };
  
  // Private member variables
  int state_;
  bool initialized_;
  
  // Private methods
  void internalProcess();
};
```

## Best Practices

### Memory Management

- Use RAII principles
- Clear ownership semantics
- Document who owns pointers

```cpp
/** NOTE: Driver does NOT own protocol
 *  Application is responsible for lifecycle management */
IHUB75Protocol* protocol;
```

### Initialization

- Initialize members in constructor initializer lists
- Use nullptr for pointers
- Zero-initialize POD types

```cpp
HUB75Driver::HUB75Driver()
  : protocol(nullptr)
  , initialized(false)
  , buffer_size(0)
{
  // Constructor body
}
```

### Error Handling

- Return bool for success/failure
- Log errors with descriptive messages
- Validate inputs

```cpp
bool init(const HUB75Config& config, IHUB75Protocol* protocol){
  if(!protocol){
    PLATFORM_LOG_E(TAG, "Protocol must be provided (cannot be null)");
    return false;
  }
  
  if(!validateConfig(config)){
    PLATFORM_LOG_E(TAG, "Invalid configuration");
    return false;
  }
  
  // ... initialization
  return true;
}
```

### Const Correctness

- Use `const` for parameters that won't be modified
- Use `const` for methods that don't modify state
- Use `const` for member variables that never change

```cpp
bool setPixel(int x, int y, const RGB& color);
RGB getPixel(int x, int y) const;
const HUB75Config& getConfig() const { return config; }
```

## Platform Logging

Use platform-agnostic logging macros:

```cpp
PLATFORM_LOG_E(TAG, "Error: %s", message);    // Error
PLATFORM_LOG_W(TAG, "Warning: %d", value);    // Warning
PLATFORM_LOG_I(TAG, "Info: initialized");     // Info
PLATFORM_LOG_D(TAG, "Debug: %04X", data);     // Debug
```

## Example Complete File

```cpp
/*****************************************************************
 * File:      driver_example.hpp
 * Category:  abstraction/drivers/components/EXAMPLE
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Example driver showing coding style conventions and best
 *    practices for the ARCOS abstraction framework.
 *****************************************************************/

#ifndef ARCOS_ABSTRACTION_DRIVERS_DRIVER_EXAMPLE_HPP_
#define ARCOS_ABSTRACTION_DRIVERS_DRIVER_EXAMPLE_HPP_

#include <stdint.h>

namespace arcos::abstraction::drivers{

/** Example configuration structure */
struct ExampleConfig{
  int buffer_size = 1024;
  bool enable_feature = false;
};

/** Example driver class */
class ExampleDriver{
public:
  ExampleDriver();
  ~ExampleDriver();
  
  /** Initialize the driver
   * @param config Configuration parameters
   * @return true if successful
   */
  bool init(const ExampleConfig& config);
  
  /** Process data
   * @param data Input data pointer
   * @param size Data size in bytes
   * @return Number of bytes processed
   */
  int process(const uint8_t* data, int size);
  
  /** Check if initialized */
  bool isInitialized() const { return initialized_; }
  
private:
  /** Internal state */
  bool initialized_;
  int buffer_size_;
  uint8_t* internal_buffer_;
  
  /** Internal helper method */
  void resetBuffers();
};

} // namespace arcos::abstraction::drivers

#endif // ARCOS_ABSTRACTION_DRIVERS_DRIVER_EXAMPLE_HPP_
```

## Summary

Key points to remember:
- **2-space indentation**
- **Tight braces** (no space before `{`)
- **camelCase** for methods
- **snake_case** for variables
- **PascalCase** for classes
- Use const correctness
- Document public APIs
- Clear ownership semantics
