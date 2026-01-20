# Animation System

A modular animation framework for the Synth-Head HUB75 LED matrix display (128x32).

## Folder Structure

```
AnimationSystem/
├── AnimationTypes.hpp          # Shared types, constants, GPU callbacks
├── AnimationSandbox.hpp        # Master controller with auto-cycling
│
├── Animations/                 # Individual animation modules
│   ├── AllAnimations.hpp       # Include all animations at once
│   ├── GyroEyesAnim.hpp        # Gyroscope-controlled eyes
│   ├── GlitchTVAnim.hpp        # Glitch TV demo effect
│   ├── SDFMorphAnim.hpp        # SDF shape morphing (□→△→○)
│   ├── ShaderTestAnim.hpp      # Rotating squares with shader
│   └── ComplexTransitionAnim.hpp
│
├── Transitions/                # Transition effects between animations
│   ├── AllTransitions.hpp      # Include all transitions at once
│   ├── GlitchTransition.hpp    # Glitch-based transition
│   └── ParticleTransition.hpp  # Particle dissolve/reform effect
│
├── Shaders/                    # Post-processing effects
│   ├── AllShaders.hpp          # Include all shaders at once
│   └── GlitchShader.hpp        # Row displacement, scanlines, color bands
│
└── Common/                     # Shared interfaces and utilities
    └── AnimationCommon.hpp     # IAnimation, ITransition, IShader interfaces
```

## Quick Start

### Include Specific Modules
```cpp
#include "AnimationSystem/Animations/GyroEyesAnim.hpp"
#include "AnimationSystem/Transitions/ParticleTransition.hpp"
#include "AnimationSystem/Shaders/GlitchShader.hpp"
```

### Include All of a Category
```cpp
#include "AnimationSystem/Animations/AllAnimations.hpp"
#include "AnimationSystem/Transitions/AllTransitions.hpp"
#include "AnimationSystem/Shaders/AllShaders.hpp"
```

### Use the Master Controller
```cpp
#include "AnimationSystem/AnimationSandbox.hpp"

// Get the singleton controller
auto& sandbox = AnimationSystem::Sandbox::getSandbox();

// Set GPU callbacks
sandbox.clear = [](uint8_t r, uint8_t g, uint8_t b) { /* ... */ };
sandbox.fillRect = [](int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) { /* ... */ };
sandbox.present = []() { /* ... */ };

// Enable and run
sandbox.setEnabled(true);
sandbox.update(deltaMs);
sandbox.render();
```

## Animations

| Animation | Description |
|-----------|-------------|
| **GyroEyesAnim** | Two circular eyes that respond to gyroscope input with smooth motion |
| **GlitchTVAnim** | Demo scene showing the GlitchShader with chromatic aberration |
| **SDFMorphAnim** | Smooth morphing between square, triangle, and circle using SDFs |
| **ShaderTestAnim** | Rotating/orbiting squares with optional glitch shader overlay |

## Transitions

| Transition | Description |
|------------|-------------|
| **GlitchTransition** | Glitch effect that peaks at 50% progress when animations swap |
| **ParticleTransition** | Current animation dissolves into falling particles, new animation reforms from particles falling into place |

### Particle Transition Details
- **MAX_PARTICLES**: 256 particles
- **GRID_STEP**: Samples every 2 pixels for performance
- **Physics**: Gravity-based with horizontal spreading and homing behavior
- **Duration**: 1.5 seconds total (0.75s out, 0.75s in)

## Shaders

| Shader | Description |
|--------|-------------|
| **GlitchShader** | Per-row displacement, chromatic aberration, scanlines, color tint bands, edge flashes |

### GlitchShader Features
- `getRowOffset(y)` - Get horizontal displacement for a row
- `getChromaOffset()` - Get chromatic aberration offset
- `getRowTint(y, r, g, b)` - Check for color tint on a row
- `applyOverlay(fillRect)` - Apply scanlines, flashes, color bands
- `setIntensity(0.0 - 2.0)` - Control effect strength (>1.0 for overdrive)

## Display Constants

```cpp
constexpr int DISPLAY_W = 128;    // Full display width
constexpr int DISPLAY_H = 32;     // Full display height
constexpr int EYE_W = 64;         // Single eye panel width
constexpr int EYE_H = 32;         // Single eye panel height
constexpr int LEFT_EYE_X = 0;     // Left panel X offset
constexpr int RIGHT_EYE_X = 64;   // Right panel X offset
```

## GPU Callback Types

```cpp
using ClearFunc = std::function<void(uint8_t r, uint8_t g, uint8_t b)>;
using FillRectFunc = std::function<void(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b)>;
using DrawPixelFunc = std::function<void(int x, int y, uint8_t r, uint8_t g, uint8_t b)>;
using DrawCircleFFunc = std::function<void(float x, float y, float radius, uint8_t r, uint8_t g, uint8_t b)>;
using PresentFunc = std::function<void()>;
```

## Adding New Components

### New Animation
1. Create `Animations/MyNewAnim.hpp`
2. Implement `update(uint32_t deltaMs)` and `render(...)` methods
3. Add to `Animations/AllAnimations.hpp`
4. Register in `SandboxController` if using auto-cycling

### New Transition
1. Create `Transitions/MyNewTransition.hpp`
2. Implement `init()`, `update()`, `draw()`, `reset()` methods
3. Add to `Transitions/AllTransitions.hpp`
4. Add to `TransitionType` enum in `SandboxController`

### New Shader
1. Create `Shaders/MyNewShader.hpp`
2. Implement `update()`, `setIntensity()`, `applyOverlay()` methods
3. Add to `Shaders/AllShaders.hpp`

## License

Part of the ARCOS project.
