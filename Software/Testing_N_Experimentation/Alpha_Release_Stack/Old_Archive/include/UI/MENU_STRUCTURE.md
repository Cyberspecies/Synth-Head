# Menu System Structure

## Overview
Hierarchical menu system with 7 top-level modes. Holding MODE button (2 seconds) from ANY depth returns to mode selector.

## Button Controls
- **SET (A)**: Enter submenu / Confirm selection
- **UP (B)**: Navigate up / Previous item
- **DOWN (C)**: Navigate down / Next item
- **MODE (D)**: 
  - **Hold 2 seconds**: ⚡ **UNIVERSAL ESCAPE** - Opens mode selector from ANYWHERE, cancels any action
  - **Short press**: Cancel/Back (in menus only)

## Menu Hierarchy

### Top-Level Modes

#### 1. Screen Saver
- **Type**: Active display mode
- **Description**: Bouncing "SYNTH-HEAD" text
- **Submenu**: None
- **Controls**: None (animated only)

#### 2. Idle (GPS)
- **Type**: Active display mode
- **Description**: GPS time bouncing around screen with collision detection
- **Submenu**: None
- **Controls**: None (animated only)
- **Display**: `HH:MM:SS` format from GPS data

#### 3. Debug Mode
- **Type**: Active display with navigation
- **Description**: Sensor data pages (5 pages)
- **Submenu**: None (direct page navigation)
- **Controls**: 
  - UP/DOWN: Switch between debug pages
  - MODE hold: Return to mode selector
- **Pages**:
  1. IMU Data - Accelerometer, gyroscope, magnetometer
  2. Environmental - Temperature, humidity, pressure
  3. GPS Data - Position, navigation, satellite info
  4. Microphone - Audio level, peak detection, bar graph
  5. System Info - Frame rates, sensor status, button states

#### 4. Display Faces (HUB75)
- **Type**: Configuration mode with submenu
- **Description**: Select shape to display on **HUB75 LED matrix**
- **Submenu**: Yes (5 options)
- **Controls**:
  - Main screen (OLED): Shows current selection, press SET to enter submenu
  - Submenu (OLED): UP/DOWN navigate, SET confirms, MODE cancels
- **Options**:
  1. Circle
  2. Square
  3. Triangle
  4. Hexagon
  5. Star
- **Effect**: Selected shape will be rendered on **HUB75 128x32 display**
- **Note**: OLED shows menu/config, HUB75 shows the actual shape

#### 5. Display Effects (HUB75)
- **Type**: Configuration mode with submenu
- **Description**: Select animation effects for **HUB75 LED matrix**
- **Submenu**: Yes (5 options)
- **Controls**: Same as Display Faces
- **Options**:
  1. None - No effect overlay
  2. Particles - Particle system overlay
  3. Trails - Motion trails behind shapes
  4. Grid - Grid overlay pattern
  5. Wave - Wave distortion effect
- **Effect**: Applied on top of or behind display faces **on HUB75**
- **Note**: OLED shows menu/config, HUB75 shows the actual effect

#### 6. Display Shaders (HUB75)
- **Type**: Configuration mode with submenu
- **Description**: Select raster shader for **HUB75 LED matrix**
- **Submenu**: Yes (5 options)
- **Controls**: Same as Display Faces
- **Options**:
  1. RGB Split - Chromatic aberration effect
  2. Scanlines - CRT scanline effect
  3. Pixelate - Pixelation/mosaic effect
  4. Invert - Color inversion
  5. Dither - Dithering pattern
- **Effect**: Post-processing applied to **HUB75 framebuffer** before display
- **Note**: OLED shows menu/config, HUB75 shows the shader result

#### 7. LED Strip Config
- **Type**: Configuration mode with submenu
- **Description**: Configure LED strip behavior
- **Submenu**: Yes (6 options)
- **Controls**: Same as Display Faces
- **Options**:
  1. Dynamic Display - Uses HUB75 display as LED data source
  2. Rainbow - Rainbow cycle animation
  3. Breathing - Pulsing brightness effect
  4. Wave - Wave propagation effect
  5. Fire - Fire simulation
  6. Theater Chase - Theater chase pattern
- **Effect**: Controls LED strip animation mode

## Navigation Flow

```
ANY MODE
   |
   | (Hold MODE 2s)
   v
MODE SELECTOR (7 items visible)
   |
   | (UP/DOWN navigate, SET select)
   v
SELECTED MODE
   |
   ├─ Screen Saver ──> (animated, no controls)
   ├─ Idle GPS ──────> (animated, no controls)
   ├─ Debug Mode ────> (UP/DOWN switch pages)
   ├─ Display Faces ─> (press SET) ──> SUBMENU (5 items) ──> (SET confirm)
   ├─ Display Effects > (press SET) ──> SUBMENU (5 items) ──> (SET confirm)
   ├─ Display Shaders > (press SET) ──> SUBMENU (5 items) ──> (SET confirm)
   └─ LED Strip Cfg ─> (press SET) ──> SUBMENU (6 items) ──> (SET confirm)
```

## Mode Selector Display
- **Layout**: Compact vertical list (7 items, 14px each + 1px spacing)
- **Visual**: 
  - Flashing double border around selected item (250ms flash rate)
  - Arrow ">" indicates current selection
  - Numbered list (1-7) for quick reference
- **Dimensions**: Fits in 128x128 OLED display

## Submenu Display
- **Layout**: Vertical list (varies: 5-6 items)
- **Visual**:
  - Flashing single border around selected item
  - Arrow ">" indicates current selection
  - Instructions at bottom
- **Controls**: 
  - UP/DOWN: Navigate
  - SET: Confirm and return to mode
  - MODE: Cancel and return to mode

## Implementation Notes

### Display Roles
- **OLED (128x128)**: Menu system, configuration interface, sensor data
- **HUB75 (128x32)**: Visual output controlled by Display Faces/Effects/Shaders
- **LED Strips (49 LEDs)**: Controlled by LED Strip Config mode

### State Machine
- `MenuState::MODE_SELECTOR` - In top-level mode selection
- `MenuState::ACTIVE_MODE` - Running selected mode
- `MenuState::SUBMENU` - In mode's submenu (if applicable)

### Hold Detection (CRITICAL FEATURE)
- **MODE button hold threshold**: 2000ms (2 seconds)
- **Works from ANY state**: Mode selector, active mode, submenu, anywhere
- **Cancels current action**: Immediately interrupts whatever is happening
- **Always returns to mode selector**: Universal "escape to top" functionality
- **Implementation**: Checked FIRST in update loop, clears all button flags, returns immediately

### Default Mode
- System boots into: **Idle (GPS)** mode
- OLED displays GPS time bouncing around screen
- HUB75 shows configured face/effect/shader combination

### Mode Persistence
- Current selections persist until changed
- Display Face, Effect, Shader (HUB75 settings) retained
- LED mode settings retained
- Debug page position retained

### Configuration vs Display Modes
**Configuration Modes (OLED shows settings):**
- Display Faces - Configures HUB75 shape
- Display Effects - Configures HUB75 effect
- Display Shaders - Configures HUB75 shader
- LED Strip Config - Configures LED animations

**Display Modes (OLED shows content):**
- Screen Saver - OLED shows bouncing text
- Idle GPS - OLED shows bouncing time
- Debug Mode - OLED shows sensor data
