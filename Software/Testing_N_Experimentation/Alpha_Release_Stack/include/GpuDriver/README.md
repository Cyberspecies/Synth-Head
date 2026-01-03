# GPU Driver Architecture

## Overview

The GPU Driver system provides a command-based architecture for CPU-to-GPU communication, where the CPU sends high-level graphics commands and the GPU performs the rendering locally. This approach dramatically reduces bandwidth requirements compared to raw pixel streaming.

## Complete System Architecture

The GPU driver is now a comprehensive graphics engine featuring:

- **Formal ISA** - ~100 opcodes across 16 categories with full type system
- **Validation Pipeline** - Compile-time and runtime verification
- **Animation System** - Hierarchical animations with blending and composition
- **SDF Rendering** - Signed Distance Field primitives and operations
- **Antialiasing** - Per-pixel coverage-based AA with multi-sample support
- **Compositing** - Multi-layer Porter-Duff compositing with color space management

## File Structure

```
include/GpuDriver/
├── GpuBaseAPI.hpp              # Protocol definitions, commands, data structures
├── GpuDriver.hpp               # CPU-side driver API
├── GpuRenderer.hpp             # GPU-side command processor and renderer
├── GpuScript.hpp               # Bytecode scripting system
├── GpuISA.hpp                  # Formal Instruction Set Architecture
├── GpuValidator.hpp            # Compile-time and runtime validation
├── GpuAnimationSystem.hpp      # Hierarchical animation framework
├── GpuSDF.hpp                  # SDF primitives and rendering
├── GpuAntialiasing.hpp         # Coverage-based antialiasing
├── GpuCompositor.hpp           # Multi-layer compositing pipeline
├── GpuTestFramework.hpp        # Unit test framework
├── GpuValidationTests.hpp      # Exhaustive validation test suite
├── GpuHardwareTestRunner.hpp   # Hardware test execution framework
├── GpuStressTest.hpp           # Long-duration stress testing
├── GpuDiagnostics.hpp          # Performance counters and logging
├── GpuRegressionTracker.hpp    # Baseline comparison and regression detection
├── GpuContinuousValidation.hpp # Multi-config continuous validation loop
├── GpuHardwareHarness.hpp      # GPU-side test harness (ESP-IDF)
├── GpuTestCoordinator.hpp      # CPU-side test coordinator (Arduino)
├── examples/
│   ├── GPU_HardwareTest.cpp    # GPU-side test implementation example
│   └── CPU_TestCoordinator.cpp # CPU-side test orchestration example
└── README.md                   # This documentation
```

---

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            CPU Side (Arduino)                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────────┐    ┌─────────────────────────┐                │
│  │     Application         │    │    ScriptBuilder        │                │
│  │  ─────────────────────  │    │  ─────────────────────  │                │
│  │  • Your code            │    │  • Build bytecode       │                │
│  │  • Game logic           │    │  • Pre-built scripts    │                │
│  │  • UI management        │    │  • Custom animations    │                │
│  └──────────┬──────────────┘    └──────────┬──────────────┘                │
│             │                               │                               │
│             ▼                               ▼                               │
│  ┌──────────────────────────────────────────────────────────┐              │
│  │                     GpuDriver                            │              │
│  │  ────────────────────────────────────────────────────── │              │
│  │  • High-level API (drawRect, drawCircle, drawText...)   │              │
│  │  • Sprite management (loadSprite, drawSprite)           │              │
│  │  • Animation control (createAnimation, startAnimation)  │              │
│  │  • Script execution (uploadScript, executeScript)       │              │
│  │  • Effect controls (rainbow, plasma, fire, particles)   │              │
│  │  • File upload (uploadFile with chunked transfer)       │              │
│  └────────────────────────────────┬─────────────────────────┘              │
│                                   │                                         │
└───────────────────────────────────┼─────────────────────────────────────────┘
                                    │ UART @ 2Mbps
                                    │ Commands ▼  ▲ ACK/Status
┌───────────────────────────────────┼─────────────────────────────────────────┐
│                                   │                                         │
│  ┌────────────────────────────────▼─────────────────────────┐              │
│  │                    GpuRenderer                           │              │
│  │  ────────────────────────────────────────────────────── │              │
│  │  • Command parsing (processCommands)                    │              │
│  │  • Drawing primitives (Bresenham's line, circles)       │              │
│  │  • Sprite storage and rendering                         │              │
│  │  • Animation system (ONCE, LOOP, PING_PONG, REVERSE)   │              │
│  │  • Effect engine (rainbow, fade, plasma, fire)         │              │
│  │  • Script interpreter (bytecode execution)             │              │
│  │  • Double-buffered frame management                    │              │
│  └────────────────────────────────┬─────────────────────────┘              │
│                                   │                                         │
│             ┌─────────────────────┴─────────────────────┐                  │
│             ▼                                           ▼                  │
│  ┌─────────────────────────┐              ┌─────────────────────────┐      │
│  │     HUB75 Display       │              │     OLED Display        │      │
│  │  ─────────────────────  │              │  ─────────────────────  │      │
│  │  128x32 RGB             │              │  128x128 Monochrome     │      │
│  │  60fps target           │              │  15fps target           │      │
│  └─────────────────────────┘              └─────────────────────────┘      │
│                                                                             │
│                            GPU Side (ESP-IDF)                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

## File Structure

```
include/GpuDriver/
├── GpuBaseAPI.hpp    # Protocol definitions, commands, data structures
├── GpuDriver.hpp     # CPU-side driver API
├── GpuRenderer.hpp   # GPU-side command processor and renderer
└── GpuScript.hpp     # Bytecode scripting system
```

## Command Protocol

### Packet Format

```
┌──────────────────────────────────────────────────────────────┐
│                       Packet Header (12 bytes)               │
├────────────┬────────────┬────────────┬────────────┬─────────┤
│  sync[3]   │  version   │  category  │  command   │ display │
│  AA 55 CC  │    0x02    │   0x10     │   0x01     │  0x01   │
├────────────┴────────────┴────────────┴────────────┴─────────┤
│  sequence  │    flags   │        payload_length             │
│   0x00     │    0x00    │         0x0008                    │
├─────────────────────────────────────────────────────────────┤
│                      Payload (variable)                      │
│                    (command-specific data)                   │
├─────────────────────────────────────────────────────────────┤
│                       Footer (3 bytes)                       │
│                   crc16 (2) + terminator (1)                │
└─────────────────────────────────────────────────────────────┘
```

### Command Categories

| Category   | Code   | Description                        |
|------------|--------|------------------------------------|
| SYSTEM     | 0x00   | Ping, reset, brightness, version   |
| DRAW       | 0x10   | Pixel, line, rect, circle, etc.   |
| TEXT       | 0x20   | Draw text, set font, set color     |
| IMAGE      | 0x30   | Upload/draw images, sprites        |
| ANIMATION  | 0x40   | Create, start, stop animations     |
| SCRIPT     | 0x50   | Upload, execute, stop scripts      |
| FILE       | 0x60   | Upload, list, delete files         |
| BUFFER     | 0x70   | Clear, swap, lock buffers          |
| EFFECT     | 0x80   | Rainbow, fade, plasma, fire, etc.  |
| QUERY      | 0x90   | Query stats, capabilities          |

## Usage Examples

### Basic Drawing (CPU Side)

```cpp
#include "GpuDriver/GpuDriver.hpp"
using namespace gpu;

GpuDriver gpu;

void setup() {
  GpuDriver::Config config;
  config.tx_pin = 12;
  config.rx_pin = 11;
  gpu.init(config);
}

void loop() {
  // Clear and draw
  gpu.beginDraw(Display::HUB75, Colors::Black);
  
  gpu.drawRect(Display::HUB75, 10, 5, 50, 20, Colors::Red, 2);
  gpu.fillCircle(Display::HUB75, 100, 16, 10, Colors::Green);
  gpu.drawLine(Display::HUB75, 0, 0, 127, 31, Colors::Blue, 1);
  
  gpu.setTextColor(Display::HUB75, Colors::Yellow);
  gpu.drawText(Display::HUB75, 30, 12, "Hello!");
  
  gpu.endDraw(Display::HUB75);
  
  delay(16);  // ~60fps
}
```

### Using Effects

```cpp
// Start a rainbow effect (runs on GPU)
gpu.rainbow(Display::HUB75, 2000);  // 2 second cycle

// Start plasma effect
gpu.plasma(Display::HUB75);

// Particle system
gpu.particles(Display::HUB75, 64, 16, 20);  // 20 particles from center
```

### Using Scripts

```cpp
#include "GpuDriver/GpuScript.hpp"

// Build a custom animation script
ScriptBuilder script;
script.clear(Colors::Black);
script.loopStart(10);
  script.rainbow(50);  // Rainbow palette phase
  script.delay(50);
script.loopEnd();
script.end();

// Upload and run
gpu.uploadScript(0, script.getData(), script.getLength());
gpu.executeScript(0);

// Or use pre-built scripts
ScriptBuilder bootScript;
Scripts::buildBootAnimation(bootScript);
gpu.uploadScript(1, bootScript.getData(), bootScript.getLength());
gpu.executeScript(1);
```

### Sprite System

```cpp
// Load a sprite (8x8 pixels)
uint8_t sprite_data[8 * 8 * 3] = { /* RGB data */ };
gpu.loadSprite(0, 8, 8, sprite_data, sizeof(sprite_data));

// Draw sprite at position
gpu.drawSprite(Display::HUB75, 0, 50, 10);

// Create animation with multiple sprites
gpu.createAnimation(0, {0, 1, 2, 3}, 100, AnimMode::LOOP);  // 100ms per frame
gpu.startAnimation(0, 20, 5);  // Start at position (20, 5)
```

---

## Advanced Graphics Systems

### Instruction Set Architecture (GpuISA.hpp)

The formal ISA defines ~100 opcodes across 16 categories:

```cpp
using namespace gpu::isa;

// Opcode categories
// SYSTEM: NOP, HALT, RESET, YIELD, SYNC, DEBUG
// FLOW: JMP, JZ, JNZ, JLT, JGT, JLE, JGE, CALL, RET, LOOP, ENDLOOP
// MEMORY: LOAD, STORE, LOAD_CONST, PUSH, POP, ALLOC, FREE
// ARITH: ADD, SUB, MUL, DIV, MOD, SIN, COS, SQRT, LERP
// DRAW: SET_PIXEL, DRAW_LINE, DRAW_RECT, FILL_CIRCLE, etc.
// SDF: SDF_CIRCLE, SDF_BOX, SDF_UNION, SDF_SMOOTH, etc.

// Data types
DataType::U8, S8, U16, S16, U32, S32, F32;
DataType::Q8_8, Q16_16;  // Fixed-point
DataType::RGB, RGBA, VEC2, VEC3, VEC4, MAT4;

// Fixed-point math
Fixed8_8 a = Fixed8_8::fromFloat(1.5f);
Fixed16_16 b = Fixed16_16::fromFloat(3.14159f);

// Vector operations
Vec3 v1(1, 0, 0), v2(0, 1, 0);
Vec3 cross = v1.cross(v2);
float dot = v1.dot(v2);

// Color operations with blend modes
ColorF red(1.0f, 0.0f, 0.0f, 1.0f);
ColorF blue(0.0f, 0.0f, 1.0f, 0.5f);
ColorF blended = applyBlendMode(red, blue, BlendMode::ALPHA);

// Easing functions (23 types)
float t = evaluateEasing(EasingType::EASE_IN_OUT_CUBIC, 0.5f);
```

### Validation System (GpuValidator.hpp)

Three-stage validation pipeline:

```cpp
using namespace gpu::validator;

// Compile-time validation
CompileTimeValidator ctv;
ValidationResult result = ctv.validate(bytecode, size);

if (!result.isValid()) {
  // result.first_error = ValidationError::SYNTAX_INVALID_OPCODE
  // result.error_address = 42  // byte offset
}

// Runtime validation
RuntimeValidator rtv;
rtv.checkArrayBounds(index, array_size);
rtv.checkDivisionSafety(numerator, denominator);
rtv.checkStackPush(current_depth, max_depth);
rtv.checkIntegerOverflow(a, b, is_addition);

// Full validation pipeline
ValidationPipeline pipeline;
pipeline.validate(program);
```

### Animation System (GpuAnimationSystem.hpp)

Comprehensive hierarchical animation:

```cpp
using namespace gpu::animation;

AnimationSystem anim;

// Create animation definition
int anim_def = anim.createAnimation();
AnimationDef* def = anim.getAnimationDef(anim_def);
def->duration = 2.0f;
def->loop_mode = LoopMode::PING_PONG;

// Add keyframes to property track
PropertyTrack& track = def->tracks[0];
track.property = PropertyType::POSITION_X;
track.addKeyframe({0.0f, {0.0f}, EasingType::EASE_OUT_BOUNCE});
track.addKeyframe({1.0f, {100.0f}, EasingType::LINEAR});

// Create instance and play
int inst = anim.createInstance(anim_def);
anim.play(inst);

// Animation layers with blend modes
AnimationLayer& layer = anim.getLayer(0);
layer.blend_mode = LayerBlendMode::ADDITIVE;
layer.weight = 0.5f;

// Procedural modifiers
ProceduralModifier noise;
noise.type = ModifierType::NOISE;
noise.amplitude = 5.0f;
noise.frequency = 2.0f;
anim.applyModifier(inst, PropertyType::POSITION_Y, noise);

// Update each frame
anim.update(delta_time);
```

### SDF Rendering (GpuSDF.hpp)

Resolution-independent shape rendering:

```cpp
using namespace gpu::sdf;

// SDF primitives (return signed distance)
float d = SDFPrimitives::circle(px, py, radius);      // < 0 inside
d = SDFPrimitives::box(px, py, half_w, half_h);
d = SDFPrimitives::roundedBox(px, py, hw, hh, r);
d = SDFPrimitives::star(px, py, r_outer, r_inner, 5);

// Boolean operations
float d1 = SDFPrimitives::circle(px, py, 10);
float d2 = SDFPrimitives::box(px - 5, py, 5, 5);
float union_d = SDFOperations::opUnion(d1, d2);
float subtract_d = SDFOperations::opSubtract(d1, d2);
float smooth_d = SDFOperations::opSmoothUnion(d1, d2, 2.0f);

// Scene graph
SDFScene scene;
int circle = scene.addCircle(0, 0, 20);
int box = scene.addBox(30, 0, 10, 15);
int combined = scene.addSmoothUnion(circle, box, 5.0f);

// Render with antialiasing
SDFRenderer renderer;
renderer.setFillColor(ColorF(1, 0, 0, 1));
renderer.render(scene, framebuffer, width, height);
```

### Antialiasing (GpuAntialiasing.hpp)

Per-pixel coverage-based AA:

```cpp
using namespace gpu::aa;

// Analytical coverage for primitives
float cov = AnalyticalCoverage::circle(px, py, cx, cy, r, filled);
cov = AnalyticalCoverage::line(px, py, x0, y0, x1, y1, width);
cov = AnalyticalCoverage::triangle(px, py, x0, y0, x1, y1, x2, y2);

// Multi-sample coverage (for complex shapes)
MultiSampleCoverage<MySDF> msc(SamplePattern::MSAA_4X);
float coverage = msc.evaluate(px, py, sdf_evaluator);
CoverageMask mask = msc.evaluateMask(px, py, sdf_evaluator);

// SDF-based antialiasing
float aa_cov = SDFAntialiasing::coverage(sdf_distance, aa_width);
aa_cov = SDFAntialiasing::strokeCoverage(distance, stroke_width);

// Coverage blending
ColorF result = CoverageBlending::blend(dst, src, coverage);
result = CoverageBlending::blendFillStroke(dst, fill, stroke, fill_cov, stroke_cov);

// High-quality primitive renderer
AAPrimitiveRenderer renderer;
renderer.setConfig({.stroke_width = 2.0f, .fill_color = red});
renderer.renderFilledCircle(buffer, w, h, stride, cx, cy, radius);
```

### Compositing (GpuCompositor.hpp)

Multi-layer Porter-Duff compositing:

```cpp
using namespace gpu::compositor;

Compositor comp;

// Allocate framebuffers
comp.getFramebuffer(0)->allocate(128, 64);
comp.getFramebuffer(0)->setColorSpace(ColorSpace::SRGB);

// Create layers
int layer0 = comp.addLayer();
Layer* bg = comp.getLayer(layer0);
bg->buffer = background_data;
bg->blend_op = CompositeOp::SRC;

int layer1 = comp.addLayer();
Layer* fg = comp.getLayer(layer1);
fg->buffer = foreground_data;
fg->blend_op = CompositeOp::SRC_OVER;
fg->opacity = 0.8f;
fg->offset_x = 10;

// Porter-Duff operations
ColorF result = PorterDuff::composite(dst, src, CompositeOp::SRC_OVER);
// Also: SRC, DST, SRC_IN, DST_OUT, XOR, MULTIPLY, SCREEN, etc.

// Execute compositing
comp.compositeAll();

// Output with dithering
comp.outputToRGB565(rgb565_buffer, w, h, true);
```

---

## Testing

### Running Tests

```cpp
#include "GpuTestFramework.hpp"
#include "GpuValidationTests.hpp"

// Run all tests
int result = gpu::test::runAllTests();

// Run by category
gpu::test::runTestsForCategory("ISA");
gpu::test::runTestsForCategory("Animation");
gpu::test::runTestsForCategory("SDF");

// Run validation suite
gpu::validation_tests::runAllValidationTests();

// Check results
TestRunner& runner = TestRunner::instance();
printf("Passed: %d, Failed: %d\n", 
       runner.getPassedCount(), runner.getFailedCount());
```

### Test Categories

| Category      | Tests | Description                            |
|---------------|-------|----------------------------------------|
| ISA           | 9     | Fixed-point math, vectors, colors      |
| Validator     | 6     | Operand counts, context, bounds        |
| Animation     | 6     | Keyframes, easing, layers, state       |
| SDF           | 5     | Primitives, operations, scene          |
| Antialiasing  | 5     | Coverage, sampling, masks              |
| Compositor    | 5     | Color space, Porter-Duff, framebuffer  |
| System        | 6     | NOP, HALT, SYNC opcodes                |
| Flow          | 4     | JMP, CALL, LOOP opcodes                |
| Memory        | 3     | LOAD, STORE, PUSH/POP                  |
| Arithmetic    | 4     | ADD, MUL, SIN, LERP                    |
| EdgeCases     | 5     | Division by zero, overflow, NaN        |

---

## Building

```bash
# Build both CPU and GPU examples
pio run -e CPU_GpuDriver -e GPU_GpuDriver

# Upload
pio run -e GPU_GpuDriver -t upload   # Upload GPU first
pio run -e CPU_GpuDriver -t upload   # Then CPU

# Monitor
pio device monitor -e CPU_GpuDriver  # Watch CPU output
```

## Bandwidth Comparison

| Method                | Data per Frame | Bandwidth @60fps |
|-----------------------|----------------|------------------|
| Raw HUB75 pixels      | 12,288 bytes   | ~5.9 Mbps        |
| Raw HUB75+OLED pixels | 14,336 bytes   | ~6.9 Mbps        |
| Command: drawRect     | ~20 bytes      | ~9.6 Kbps        |
| Command: drawText     | ~30 bytes      | ~14.4 Kbps       |
| Script (runs on GPU)  | 0 bytes/frame  | 0 (one-time)     |

The command-based approach reduces bandwidth by **100-600x** for typical use cases!

## Integration with Display Drivers

The `GpuRenderer` provides abstract buffers. To connect to actual displays:

```cpp
// In your GPU main.cpp
#include "GpuDriver/GpuRenderer.hpp"
#include "your_hub75_driver.h"
#include "your_oled_driver.h"

GpuRenderer renderer;

void renderLoop() {
  renderer.update();
  renderer.render();
  
  // Get buffers and send to hardware
  const uint8_t* hub75 = renderer.getHUB75Buffer();
  const uint8_t* oled = renderer.getOLEDBuffer();
  
  hub75_display(hub75);  // Your HUB75 driver
  oled_display(oled);    // Your OLED driver
}
```

---

## Hardware Testing Framework

The driver includes a comprehensive hardware validation system for continuous testing on real devices.

### Components

1. **GpuHardwareTestRunner.hpp** - Test execution framework with callbacks
2. **GpuStressTest.hpp** - Long-duration stress tests (memory, precision, thermal)
3. **GpuDiagnostics.hpp** - Performance counters, logging, state capture
4. **GpuRegressionTracker.hpp** - Baseline comparison and regression detection
5. **GpuContinuousValidation.hpp** - Multi-configuration continuous validation
6. **GpuHardwareHarness.hpp** - GPU-side ESP-IDF test harness
7. **GpuTestCoordinator.hpp** - CPU-side Arduino test coordinator

### Test Protocol

CPU and GPU communicate over UART using a simple packet protocol:

```
┌───────┬─────────┬────────┬─────────────┬─────┐
│ Magic │ Command │ Length │   Payload   │ CRC │
│ 0xAA55│  1 byte │ 1 byte │  0-252 bytes│1byte│
└───────┴─────────┴────────┴─────────────┴─────┘
```

### Test Categories

| Category  | Tests | Purpose |
|-----------|-------|---------|
| ISA       | NOP, SET_PIXEL, FILL_RECT, etc. | Verify instruction execution |
| Rendering | Clear, Flip, Blend, Gradient | Visual output validation |
| Animation | Linear, Bezier, Loop, Chain | Animation system verification |
| SDF       | Circle, Box, Union, Blend | SDF rendering accuracy |
| Stress    | Memory, Commands, Precision, Thermal | Long-duration stability |

### Running Tests

**GPU Side (ESP-IDF):**
```cpp
#include "GpuDriver/GpuHardwareHarness.hpp"

GpuTestHarness harness;

void app_main() {
  harness.initialize();
  GPU_REGISTER_TEST(harness, ISA_NOP, 1000);
  GPU_REGISTER_TEST(harness, ISA_SET_PIXEL, 1000);
  // ... register more tests
  harness.run();  // Enter test loop
}
```

**CPU Side (Arduino):**
```cpp
#include "GpuDriver/GpuTestCoordinator.hpp"

TestCoordinator coordinator;

void setup() {
  coordinator.initialize(921600);  // UART baud
  suites::addAllTests(coordinator.tests());
}

void loop() {
  // Run all tests once
  coordinator.runAllTests(1);
  
  // Or run until stable
  coordinator.runUntilStable(5, 100);  // 5 passes, max 100 iterations
  
  // Or continuous validation
  coordinator.runContinuous();
}
```

### Stress Testing

```cpp
#include "GpuDriver/GpuStressTest.hpp"

StressTestExecutor executor;

// Memory stress
StressResult mem = executor.runMemoryStress(60000);  // 60 seconds

// Precision drift test
StressResult prec = executor.runPrecisionDriftTest(30000, 100000);

// Full stress suite
StressResult full = executor.runFullStressSuite(300000);  // 5 minutes
```

### Regression Tracking

```cpp
#include "GpuDriver/GpuRegressionTracker.hpp"

AdvancedRegressionTracker tracker;

// Create baseline
BaselineMetrics metrics = { /* measured values */ };
int id = tracker.createBaseline("ISA_NOP", BaselineType::FULL, config, metrics);

// Compare future runs
TestResult result = tracker.compareToBaseline("ISA_NOP", config, newMetrics);
if (result.status == RegressionStatus::REGRESSION) {
  // Alert! Performance degraded
}
```

### Diagnostics

```cpp
#include "GpuDriver/GpuDiagnostics.hpp"

DiagnosticsSystem diag;

// Performance counters
diag.counters().increment(PerfCounter::FRAMES_RENDERED);
diag.counters().increment(PerfCounter::PIXELS_DRAWN, 1024);

// Logging
diag.log().info("TEST", "Starting test run");
diag.log().error("TEST", "Test failed: %s", reason);

// Health check
auto health = diag.getHealthStatus();
if (!health.healthy) {
  // Handle unhealthy state
}
```

### Failure Categories

The system tracks failures by category for pattern detection:

| Category      | Detection Method | Example |
|---------------|------------------|---------|
| TIMING        | Frame time variance | Frames > 20ms |
| PRECISION     | Float comparison | Drift > 0.1% |
| RACE_CONDITION| Inconsistent results | Same seed, different output |
| MEMORY_CORRUPT| Pattern verification | Corrupted data |
| SYNC_ERROR    | Protocol timeout | No ACK received |
| WATCHDOG      | Timeout exceeded | Test hung |
| THERMAL       | Temperature check | > 80°C |

### Continuous Validation Modes

```cpp
// Single iteration
coordinator.runAllTests(1);

// Multiple iterations
coordinator.runAllTests(10);

// Run until all tests pass 5 times consecutively
coordinator.runUntilStable(5, 100);

// Continuous until stopped
coordinator.runContinuous();

// Duration-based
validator.runForDuration(3600000);  // 1 hour
```

### Configuration Matrix

Test across multiple configurations:

```cpp
ContinuousValidator validator;

// Add configurations
validator.addConfig(presets::normalConfig());    // 240MHz, 25°C
validator.addConfig(presets::highSpeedConfig()); // 240MHz, 2Mbps UART
validator.addConfig(presets::lowSpeedConfig());  // 160MHz, 115200 UART
validator.addConfig(presets::hotConfig());       // 240MHz, 50°C
validator.addConfig(presets::coldConfig());      // 240MHz, 10°C

// Run until stable across all configs
bool stable = validator.runUntilStable(100);
```
```

## Future Enhancements

1. **Font System**: Multiple fonts, sizes, Unicode support
2. **Image Compression**: RLE or LZ4 for sprite data
3. **3D Graphics**: Simple 3D wireframe rendering
4. **Audio Sync**: Sync animations to audio beats
5. **Touch/Input**: Handle input from GPU-connected sensors
