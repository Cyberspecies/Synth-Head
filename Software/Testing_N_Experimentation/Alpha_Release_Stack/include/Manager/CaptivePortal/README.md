# Captive Portal Manager - Modular Structure

This directory contains the implementation files for the Captive Portal Manager, split into logical components for better maintainability.

## File Structure

- **CaptivePortalManager.impl.hpp** - Core implementation (constructor, destructor, initialization, credentials, etc.)
- **WebPages_Setup.hpp** - Setup page HTML generation
- **WebPages_Dashboard.hpp** - Dashboard page HTML generation  
- **WebServer_Routes.hpp** - Web server route handlers and API endpoints

## Usage

The main header `include/Manager/CaptivePortalManager.hpp` automatically includes the implementation files.

Simply include the main header:
```cpp
#include "Manager/CaptivePortalManager.hpp"
```

## Architecture

The captive portal manager provides:
1. WiFi Access Point with customizable credentials
2. DNS server for captive portal redirection
3. Web interface for sensor monitoring and configuration
4. Real-time sensor data display via AJAX polling
5. Interactive button controls via web interface

## Implementation Pattern

Uses header-only implementation pattern with separate files for organization:
- No `.cpp` file needed
- All implementation in `.impl.hpp` files
- Modular separation of concerns
- Easy to maintain and extend
