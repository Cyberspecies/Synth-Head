/*****************************************************************
 * File:      GpuISA.hpp
 * Category:  GPU Driver / Instruction Set Architecture
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Formal definition of the GPU Instruction Set Architecture.
 *    Defines all opcodes, operand types, encoding rules, and
 *    execution semantics with complete type safety.
 * 
 * ISA Version: 2.0
 * 
 * Instruction Format:
 *    [OPCODE:8][FLAGS:8][OPERANDS:variable]
 * 
 * Encoding:
 *    - All integers are little-endian
 *    - Coordinates are signed 16-bit (supports negative for clipping)
 *    - Colors are 8-bit per channel (RGB/RGBA)
 *    - Fixed-point uses Q8.8 or Q16.16 format
 *****************************************************************/

#ifndef GPU_ISA_HPP_
#define GPU_ISA_HPP_

#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace gpu {
namespace isa {

// ============================================================
// ISA Version and Limits
// ============================================================

constexpr uint8_t  ISA_VERSION_MAJOR = 2;
constexpr uint8_t  ISA_VERSION_MINOR = 0;
constexpr uint16_t ISA_VERSION = (ISA_VERSION_MAJOR << 8) | ISA_VERSION_MINOR;

constexpr size_t   MAX_INSTRUCTION_SIZE   = 64;
constexpr size_t   MAX_PROGRAM_SIZE       = 8192;
constexpr size_t   MAX_STACK_DEPTH        = 32;
constexpr size_t   MAX_VARIABLES          = 64;
constexpr size_t   MAX_LABELS             = 128;
constexpr size_t   MAX_STRING_LENGTH      = 64;
constexpr size_t   MAX_CALL_DEPTH         = 16;

// ============================================================
// Data Types
// ============================================================

enum class DataType : uint8_t {
  VOID      = 0x00,
  BOOL      = 0x01,
  UINT8     = 0x02,
  INT8      = 0x03,
  UINT16    = 0x04,
  INT16     = 0x05,
  UINT32    = 0x06,
  INT32     = 0x07,
  FLOAT16   = 0x08,  // Half precision (for color/coord)
  FLOAT32   = 0x09,  // Single precision
  FIXED8_8  = 0x0A,  // Q8.8 fixed point
  FIXED16_16= 0x0B,  // Q16.16 fixed point
  COLOR_RGB = 0x10,  // 3 bytes
  COLOR_RGBA= 0x11,  // 4 bytes
  VEC2      = 0x20,  // 2x float/fixed
  VEC3      = 0x21,  // 3x float/fixed
  VEC4      = 0x22,  // 4x float/fixed
  MAT2      = 0x30,  // 2x2 matrix
  MAT3      = 0x31,  // 3x3 matrix
  MAT4      = 0x32,  // 4x4 matrix
  STRING    = 0x40,  // Length-prefixed string
  ARRAY     = 0x50,  // Dynamic array
  BUFFER    = 0x60,  // Buffer reference
};

// Get size in bytes for data type
// Note: Uses inline for C++11 compatibility; constexpr works in C++14+
inline size_t getDataTypeSize(DataType type) {
  switch (type) {
    case DataType::VOID:       return 0;
    case DataType::BOOL:       return 1;
    case DataType::UINT8:      return 1;
    case DataType::INT8:       return 1;
    case DataType::UINT16:     return 2;
    case DataType::INT16:      return 2;
    case DataType::UINT32:     return 4;
    case DataType::INT32:      return 4;
    case DataType::FLOAT16:    return 2;
    case DataType::FLOAT32:    return 4;
    case DataType::FIXED8_8:   return 2;
    case DataType::FIXED16_16: return 4;
    case DataType::COLOR_RGB:  return 3;
    case DataType::COLOR_RGBA: return 4;
    case DataType::VEC2:       return 8;
    case DataType::VEC3:       return 12;
    case DataType::VEC4:       return 16;
    case DataType::MAT2:       return 16;
    case DataType::MAT3:       return 36;
    case DataType::MAT4:       return 64;
    default:                   return 0;  // Variable/unknown types
  }
}

// ============================================================
// Precision Modes
// ============================================================

enum class PrecisionMode : uint8_t {
  LOW       = 0x00,  // 8-bit fixed, fast
  MEDIUM    = 0x01,  // 16-bit fixed, balanced
  HIGH      = 0x02,  // 32-bit float, accurate
  ADAPTIVE  = 0x03,  // Choose based on operation
};

// ============================================================
// Execution Context
// ============================================================

enum class ExecutionContext : uint8_t {
  IMMEDIATE = 0x00,  // Execute now
  DEFERRED  = 0x01,  // Queue for batch execution
  VERTEX    = 0x02,  // Vertex shader context
  FRAGMENT  = 0x03,  // Fragment/pixel shader context
  COMPUTE   = 0x04,  // Compute shader context
  ANIMATION = 0x05,  // Animation interpolator context
};

// ============================================================
// Instruction Flags
// ============================================================

namespace InstructionFlags {
  constexpr uint8_t NONE          = 0x00;
  constexpr uint8_t CONDITIONAL   = 0x01;  // Depends on condition
  constexpr uint8_t SATURATE      = 0x02;  // Clamp result
  constexpr uint8_t BLEND         = 0x04;  // Use alpha blending
  constexpr uint8_t ANTIALIASED   = 0x08;  // Use antialiasing
  constexpr uint8_t IMMEDIATE     = 0x10;  // Immediate operand follows
  constexpr uint8_t INDIRECT      = 0x20;  // Operand is address
  constexpr uint8_t BROADCAST     = 0x40;  // Broadcast to all targets
  constexpr uint8_t ATOMIC        = 0x80;  // Atomic operation
}

// ============================================================
// Opcode Categories (high nibble)
// ============================================================

enum class OpcodeCategory : uint8_t {
  SYSTEM      = 0x00,  // 0x00-0x0F: System operations
  FLOW        = 0x10,  // 0x10-0x1F: Flow control
  MEMORY      = 0x20,  // 0x20-0x2F: Memory operations
  ARITH       = 0x30,  // 0x30-0x3F: Arithmetic
  LOGIC       = 0x40,  // 0x40-0x4F: Logic operations
  COMPARE     = 0x50,  // 0x50-0x5F: Comparison
  CONVERT     = 0x60,  // 0x60-0x6F: Type conversion
  DRAW        = 0x70,  // 0x70-0x7F: Drawing primitives
  TEXT        = 0x80,  // 0x80-0x8F: Text operations
  SPRITE      = 0x90,  // 0x90-0x9F: Sprite operations
  ANIM        = 0xA0,  // 0xA0-0xAF: Animation
  EFFECT      = 0xB0,  // 0xB0-0xBF: Effects
  BUFFER      = 0xC0,  // 0xC0-0xCF: Buffer operations
  SDF         = 0xD0,  // 0xD0-0xDF: SDF operations
  SHADER      = 0xE0,  // 0xE0-0xEF: Shader intrinsics
  EXTENDED    = 0xF0,  // 0xF0-0xFF: Extended opcodes
};

// ============================================================
// Full Opcode Definitions
// ============================================================

enum class Opcode : uint8_t {
  // ===== SYSTEM (0x00-0x0F) =====
  NOP           = 0x00,  // No operation
  HALT          = 0x01,  // Stop execution
  YIELD         = 0x02,  // Yield to scheduler
  SYNC          = 0x03,  // Synchronization barrier
  DEBUG         = 0x04,  // Debug breakpoint
  ASSERT        = 0x05,  // Runtime assertion
  TRACE         = 0x06,  // Emit trace event
  PROFILE_START = 0x07,  // Start profiling
  PROFILE_END   = 0x08,  // End profiling
  VERSION       = 0x09,  // Check ISA version
  CAPABILITY    = 0x0A,  // Query capability
  
  // ===== FLOW CONTROL (0x10-0x1F) =====
  JUMP          = 0x10,  // Unconditional jump
  JUMP_IF       = 0x11,  // Conditional jump (true)
  JUMP_UNLESS   = 0x12,  // Conditional jump (false)
  CALL          = 0x13,  // Call subroutine
  RETURN        = 0x14,  // Return from subroutine
  LOOP_START    = 0x15,  // Begin loop
  LOOP_END      = 0x16,  // End loop (continue/break)
  SWITCH        = 0x17,  // Switch statement
  CASE          = 0x18,  // Case label
  DEFAULT       = 0x19,  // Default case
  FOR_EACH      = 0x1A,  // Iterate over array
  WAIT          = 0x1B,  // Wait for condition
  DELAY         = 0x1C,  // Delay in milliseconds
  TRIGGER       = 0x1D,  // Fire trigger
  ON_EVENT      = 0x1E,  // Event handler
  
  // ===== MEMORY (0x20-0x2F) =====
  LOAD          = 0x20,  // Load from memory
  STORE         = 0x21,  // Store to memory
  PUSH          = 0x22,  // Push to stack
  POP           = 0x23,  // Pop from stack
  DUP           = 0x24,  // Duplicate top of stack
  SWAP_STACK    = 0x25,  // Swap top two stack items
  ALLOC         = 0x26,  // Allocate memory
  FREE          = 0x27,  // Free memory
  COPY          = 0x28,  // Memory copy
  FILL          = 0x29,  // Memory fill
  LOAD_CONST    = 0x2A,  // Load constant
  LOAD_UNIFORM  = 0x2B,  // Load uniform variable
  STORE_UNIFORM = 0x2C,  // Store uniform variable
  
  // ===== ARITHMETIC (0x30-0x3F) =====
  ADD           = 0x30,  // Addition
  SUB           = 0x31,  // Subtraction
  MUL           = 0x32,  // Multiplication
  DIV           = 0x33,  // Division
  MOD           = 0x34,  // Modulo
  NEG           = 0x35,  // Negate
  ABS           = 0x36,  // Absolute value
  MIN           = 0x37,  // Minimum
  MAX           = 0x38,  // Maximum
  CLAMP         = 0x39,  // Clamp to range
  LERP          = 0x3A,  // Linear interpolation
  STEP          = 0x3B,  // Step function
  SMOOTH_STEP   = 0x3C,  // Smooth step
  FMA           = 0x3D,  // Fused multiply-add
  SQRT          = 0x3E,  // Square root
  RSQRT         = 0x3F,  // Reciprocal square root
  
  // ===== LOGIC (0x40-0x4F) =====
  AND           = 0x40,  // Bitwise AND
  OR            = 0x41,  // Bitwise OR
  XOR           = 0x42,  // Bitwise XOR
  NOT           = 0x43,  // Bitwise NOT
  SHL           = 0x44,  // Shift left
  SHR           = 0x45,  // Shift right (logical)
  SAR           = 0x46,  // Shift right (arithmetic)
  ROL           = 0x47,  // Rotate left
  ROR           = 0x48,  // Rotate right
  LAND          = 0x49,  // Logical AND
  LOR           = 0x4A,  // Logical OR
  LNOT          = 0x4B,  // Logical NOT
  
  // ===== COMPARISON (0x50-0x5F) =====
  CMP_EQ        = 0x50,  // Equal
  CMP_NE        = 0x51,  // Not equal
  CMP_LT        = 0x52,  // Less than
  CMP_LE        = 0x53,  // Less than or equal
  CMP_GT        = 0x54,  // Greater than
  CMP_GE        = 0x55,  // Greater than or equal
  CMP_ZERO      = 0x56,  // Compare to zero
  CMP_SIGN      = 0x57,  // Get sign
  SELECT        = 0x58,  // Conditional select
  
  // ===== TYPE CONVERSION (0x60-0x6F) =====
  CAST_INT      = 0x60,  // Cast to integer
  CAST_FLOAT    = 0x61,  // Cast to float
  CAST_FIXED    = 0x62,  // Cast to fixed-point
  CAST_BOOL     = 0x63,  // Cast to boolean
  PACK_COLOR    = 0x64,  // Pack RGB(A) to int
  UNPACK_COLOR  = 0x65,  // Unpack int to RGB(A)
  PACK_VEC      = 0x66,  // Pack vector
  UNPACK_VEC    = 0x67,  // Unpack vector
  TRUNC         = 0x68,  // Truncate to integer
  FLOOR         = 0x69,  // Floor
  CEIL          = 0x6A,  // Ceiling
  ROUND         = 0x6B,  // Round to nearest
  FRACT         = 0x6C,  // Fractional part
  
  // ===== DRAWING (0x70-0x7F) =====
  DRAW_PIXEL    = 0x70,  // Draw single pixel
  DRAW_LINE     = 0x71,  // Draw line
  DRAW_RECT     = 0x72,  // Draw rectangle outline
  DRAW_FILL_RECT= 0x73,  // Draw filled rectangle
  DRAW_CIRCLE   = 0x74,  // Draw circle outline
  DRAW_FILL_CIRC= 0x75,  // Draw filled circle
  DRAW_ELLIPSE  = 0x76,  // Draw ellipse
  DRAW_ARC      = 0x77,  // Draw arc
  DRAW_TRIANGLE = 0x78,  // Draw triangle
  DRAW_POLYGON  = 0x79,  // Draw polygon
  DRAW_BEZIER   = 0x7A,  // Draw bezier curve
  DRAW_PATH     = 0x7B,  // Draw path
  DRAW_GRADIENT = 0x7C,  // Draw gradient
  DRAW_BITMAP   = 0x7D,  // Draw bitmap
  
  // ===== TEXT (0x80-0x8F) =====
  TEXT_DRAW     = 0x80,  // Draw text string
  TEXT_CHAR     = 0x81,  // Draw single character
  TEXT_SET_FONT = 0x82,  // Set font
  TEXT_SET_SIZE = 0x83,  // Set text size
  TEXT_SET_COLOR= 0x84,  // Set text color
  TEXT_MEASURE  = 0x85,  // Measure text bounds
  TEXT_WRAP     = 0x86,  // Draw wrapped text
  TEXT_ALIGN    = 0x87,  // Set text alignment
  
  // ===== SPRITE (0x90-0x9F) =====
  SPRITE_LOAD   = 0x90,  // Load sprite data
  SPRITE_UNLOAD = 0x91,  // Unload sprite
  SPRITE_DRAW   = 0x92,  // Draw sprite
  SPRITE_FRAME  = 0x93,  // Set sprite frame
  SPRITE_TRANSFORM = 0x94,  // Set sprite transform
  SPRITE_TINT   = 0x95,  // Set sprite tint
  SPRITE_CLIP   = 0x96,  // Set sprite clip rect
  
  // ===== ANIMATION (0xA0-0xAF) =====
  ANIM_CREATE   = 0xA0,  // Create animation
  ANIM_DESTROY  = 0xA1,  // Destroy animation
  ANIM_START    = 0xA2,  // Start animation
  ANIM_STOP     = 0xA3,  // Stop animation
  ANIM_PAUSE    = 0xA4,  // Pause animation
  ANIM_RESUME   = 0xA5,  // Resume animation
  ANIM_SEEK     = 0xA6,  // Seek to time
  ANIM_SPEED    = 0xA7,  // Set playback speed
  ANIM_LOOP     = 0xA8,  // Set loop mode
  ANIM_BLEND    = 0xA9,  // Blend animations
  ANIM_CHAIN    = 0xAA,  // Chain animations
  ANIM_KEYFRAME = 0xAB,  // Define keyframe
  ANIM_CURVE    = 0xAC,  // Set interpolation curve
  ANIM_QUERY    = 0xAD,  // Query animation state
  
  // ===== EFFECTS (0xB0-0xBF) =====
  EFFECT_START  = 0xB0,  // Start effect
  EFFECT_STOP   = 0xB1,  // Stop effect
  EFFECT_PARAM  = 0xB2,  // Set effect parameter
  EFFECT_RAINBOW= 0xB3,  // Rainbow effect
  EFFECT_PLASMA = 0xB4,  // Plasma effect
  EFFECT_FIRE   = 0xB5,  // Fire effect
  EFFECT_MATRIX = 0xB6,  // Matrix rain effect
  EFFECT_PARTICLE=0xB7,  // Particle system
  EFFECT_BLUR   = 0xB8,  // Blur
  EFFECT_GLOW   = 0xB9,  // Glow
  EFFECT_SHADOW = 0xBA,  // Drop shadow
  EFFECT_CUSTOM = 0xBB,  // Custom effect
  
  // ===== BUFFER (0xC0-0xCF) =====
  BUF_CLEAR     = 0xC0,  // Clear buffer
  BUF_SWAP      = 0xC1,  // Swap buffers
  BUF_COPY      = 0xC2,  // Copy buffer region
  BUF_BLEND     = 0xC3,  // Blend buffers
  BUF_LOCK      = 0xC4,  // Lock buffer
  BUF_UNLOCK    = 0xC5,  // Unlock buffer
  BUF_SET_TARGET= 0xC6,  // Set render target
  BUF_GET_PIXEL = 0xC7,  // Read pixel from buffer
  BUF_SET_CLIP  = 0xC8,  // Set clipping region
  BUF_RESET_CLIP= 0xC9,  // Reset clipping
  BUF_COMPOSITE = 0xCA,  // Composite layers
  
  // ===== SDF (0xD0-0xDF) =====
  SDF_CIRCLE    = 0xD0,  // Circle SDF
  SDF_BOX       = 0xD1,  // Box SDF
  SDF_ROUND_BOX = 0xD2,  // Rounded box SDF
  SDF_SEGMENT   = 0xD3,  // Line segment SDF
  SDF_TRIANGLE  = 0xD4,  // Triangle SDF
  SDF_POLYGON   = 0xD5,  // Polygon SDF
  SDF_UNION     = 0xD6,  // Union of SDFs
  SDF_SUBTRACT  = 0xD7,  // Subtraction of SDFs
  SDF_INTERSECT = 0xD8,  // Intersection of SDFs
  SDF_SMOOTH_U  = 0xD9,  // Smooth union
  SDF_SMOOTH_S  = 0xDA,  // Smooth subtraction
  SDF_BLEND     = 0xDB,  // Blend SDFs
  SDF_RENDER    = 0xDC,  // Render SDF to buffer
  SDF_GRADIENT  = 0xDD,  // Compute SDF gradient
  SDF_AA        = 0xDE,  // SDF with antialiasing
  
  // ===== SHADER INTRINSICS (0xE0-0xEF) =====
  SHADER_SIN    = 0xE0,  // Sine
  SHADER_COS    = 0xE1,  // Cosine
  SHADER_TAN    = 0xE2,  // Tangent
  SHADER_ASIN   = 0xE3,  // Arc sine
  SHADER_ACOS   = 0xE4,  // Arc cosine
  SHADER_ATAN   = 0xE5,  // Arc tangent
  SHADER_ATAN2  = 0xE6,  // Two-argument arc tangent
  SHADER_POW    = 0xE7,  // Power
  SHADER_EXP    = 0xE8,  // Exponential
  SHADER_LOG    = 0xE9,  // Natural logarithm
  SHADER_DOT    = 0xEA,  // Dot product
  SHADER_CROSS  = 0xEB,  // Cross product
  SHADER_LENGTH = 0xEC,  // Vector length
  SHADER_NORM   = 0xED,  // Normalize
  SHADER_REFLECT= 0xEE,  // Reflection
  SHADER_NOISE  = 0xEF,  // Noise function
  
  // ===== EXTENDED (0xF0-0xFF) =====
  EXT_PREFIX    = 0xF0,  // Extended opcode prefix
  RESERVED_F1   = 0xF1,
  RESERVED_F2   = 0xF2,
  RESERVED_F3   = 0xF3,
  RESERVED_F4   = 0xF4,
  RESERVED_F5   = 0xF5,
  RESERVED_F6   = 0xF6,
  RESERVED_F7   = 0xF7,
  RESERVED_F8   = 0xF8,
  RESERVED_F9   = 0xF9,
  RESERVED_FA   = 0xFA,
  RESERVED_FB   = 0xFB,
  RESERVED_FC   = 0xFC,
  RESERVED_FD   = 0xFD,
  RESERVED_FE   = 0xFE,
  ILLEGAL       = 0xFF,  // Illegal instruction (trap)
};

// ============================================================
// Operand Descriptors
// ============================================================

struct OperandDesc {
  DataType type;
  bool     is_immediate;
  bool     is_optional;
  int8_t   min_value;   // For range validation (-128 = no min)
  int8_t   max_value;   // For range validation (127 = no max)
};

// ============================================================
// Instruction Descriptor
// ============================================================

struct InstructionDesc {
  Opcode       opcode;
  const char*  mnemonic;
  uint8_t      num_operands;
  OperandDesc  operands[4];
  uint8_t      cycles;           // Estimated execution cycles
  uint8_t      valid_contexts;   // Bitmask of valid ExecutionContext
  bool         modifies_state;   // True if modifies render state
  bool         has_side_effects; // True if has side effects
};

// ============================================================
// Instruction Encoding/Decoding
// ============================================================

struct EncodedInstruction {
  Opcode   opcode;
  uint8_t  flags;
  uint8_t  operand_count;
  uint8_t  operand_data[MAX_INSTRUCTION_SIZE - 3];
  size_t   total_size;
};

// Decode instruction from bytecode
inline bool decodeInstruction(const uint8_t* data, size_t len, EncodedInstruction& out) {
  if (len < 2) return false;
  
  out.opcode = static_cast<Opcode>(data[0]);
  out.flags = data[1];
  out.total_size = 2;
  out.operand_count = 0;
  
  // Decode based on opcode...
  // (Actual decoding would examine opcode and extract operands)
  
  return true;
}

// ============================================================
// Fixed-Point Math Helpers
// ============================================================

// Q8.8 fixed point (16-bit)
using Fixed8_8 = int16_t;

inline Fixed8_8 floatToFixed8_8(float f) {
  return static_cast<Fixed8_8>(f * 256.0f);
}

inline float fixed8_8ToFloat(Fixed8_8 f) {
  return f / 256.0f;
}

// Q16.16 fixed point (32-bit)
using Fixed16_16 = int32_t;

inline Fixed16_16 floatToFixed16_16(float f) {
  return static_cast<Fixed16_16>(f * 65536.0f);
}

inline float fixed16_16ToFloat(Fixed16_16 f) {
  return f / 65536.0f;
}

// Fixed-point multiplication (Q8.8)
inline Fixed8_8 mulFixed8_8(Fixed8_8 a, Fixed8_8 b) {
  return static_cast<Fixed8_8>((static_cast<int32_t>(a) * b) >> 8);
}

// Fixed-point multiplication (Q16.16)
inline Fixed16_16 mulFixed16_16(Fixed16_16 a, Fixed16_16 b) {
  return static_cast<Fixed16_16>((static_cast<int64_t>(a) * b) >> 16);
}

// ============================================================
// Vector Types
// ============================================================

struct Vec2 {
  float x, y;
  
  Vec2() : x(0), y(0) {}
  Vec2(float x_, float y_) : x(x_), y(y_) {}
  
  Vec2 operator+(const Vec2& v) const { return Vec2(x + v.x, y + v.y); }
  Vec2 operator-(const Vec2& v) const { return Vec2(x - v.x, y - v.y); }
  Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
  Vec2 operator/(float s) const { return Vec2(x / s, y / s); }
  
  float dot(const Vec2& v) const { return x * v.x + y * v.y; }
  float length() const { return sqrtf(x * x + y * y); }
  Vec2 normalized() const { float l = length(); return l > 0 ? *this / l : Vec2(); }
};

struct Vec3 {
  float x, y, z;
  
  Vec3() : x(0), y(0), z(0) {}
  Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
  
  Vec3 operator+(const Vec3& v) const { return Vec3(x + v.x, y + v.y, z + v.z); }
  Vec3 operator-(const Vec3& v) const { return Vec3(x - v.x, y - v.y, z - v.z); }
  Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
  
  float dot(const Vec3& v) const { return x * v.x + y * v.y + z * v.z; }
  Vec3 cross(const Vec3& v) const {
    return Vec3(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
  }
  float length() const { return sqrtf(x * x + y * y + z * z); }
  Vec3 normalized() const { float l = length(); return l > 0 ? *this / l : Vec3(); }
};

struct Vec4 {
  float x, y, z, w;
  
  Vec4() : x(0), y(0), z(0), w(0) {}
  Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
  Vec4(const Vec3& v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
};

// ============================================================
// Color with Float Precision
// ============================================================

struct ColorF {
  float r, g, b, a;
  
  ColorF() : r(0), g(0), b(0), a(1) {}
  ColorF(float r_, float g_, float b_, float a_ = 1.0f) : r(r_), g(g_), b(b_), a(a_) {}
  
  // From 8-bit
  static ColorF fromRGB(uint8_t r, uint8_t g, uint8_t b) {
    return ColorF(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
  }
  
  static ColorF fromRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ColorF(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
  }
  
  // To 8-bit
  uint8_t r8() const { return static_cast<uint8_t>(clamp01(r) * 255.0f); }
  uint8_t g8() const { return static_cast<uint8_t>(clamp01(g) * 255.0f); }
  uint8_t b8() const { return static_cast<uint8_t>(clamp01(b) * 255.0f); }
  uint8_t a8() const { return static_cast<uint8_t>(clamp01(a) * 255.0f); }
  
  // Operations
  ColorF operator+(const ColorF& c) const { return ColorF(r + c.r, g + c.g, b + c.b, a + c.a); }
  ColorF operator*(float s) const { return ColorF(r * s, g * s, b * s, a * s); }
  ColorF operator*(const ColorF& c) const { return ColorF(r * c.r, g * c.g, b * c.b, a * c.a); }
  
  // Blend (alpha compositing)
  ColorF blend(const ColorF& over) const {
    float out_a = over.a + a * (1.0f - over.a);
    if (out_a < 0.0001f) return ColorF(0, 0, 0, 0);
    float inv_out_a = 1.0f / out_a;
    return ColorF(
      (over.r * over.a + r * a * (1.0f - over.a)) * inv_out_a,
      (over.g * over.a + g * a * (1.0f - over.a)) * inv_out_a,
      (over.b * over.a + b * a * (1.0f - over.a)) * inv_out_a,
      out_a
    );
  }
  
  // Linear interpolation
  ColorF lerp(const ColorF& to, float t) const {
    return ColorF(
      r + (to.r - r) * t,
      g + (to.g - g) * t,
      b + (to.b - b) * t,
      a + (to.a - a) * t
    );
  }
  
  // Premultiplied alpha
  ColorF premultiply() const { return ColorF(r * a, g * a, b * a, a); }
  
private:
  static float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
};

// ============================================================
// Interpolation Curves
// ============================================================

enum class EasingType : uint8_t {
  LINEAR        = 0x00,
  EASE_IN       = 0x01,  // Quadratic
  EASE_OUT      = 0x02,
  EASE_IN_OUT   = 0x03,
  EASE_IN_CUBIC = 0x04,
  EASE_OUT_CUBIC= 0x05,
  EASE_IN_OUT_CUBIC = 0x06,
  EASE_IN_QUART = 0x07,
  EASE_OUT_QUART= 0x08,
  EASE_IN_OUT_QUART = 0x09,
  EASE_IN_EXPO  = 0x0A,
  EASE_OUT_EXPO = 0x0B,
  EASE_IN_OUT_EXPO = 0x0C,
  EASE_IN_BACK  = 0x0D,
  EASE_OUT_BACK = 0x0E,
  EASE_IN_OUT_BACK = 0x0F,
  EASE_IN_ELASTIC = 0x10,
  EASE_OUT_ELASTIC = 0x11,
  EASE_IN_BOUNCE = 0x12,
  EASE_OUT_BOUNCE = 0x13,
  STEP          = 0x14,
  SMOOTH_STEP   = 0x15,
  SMOOTHER_STEP = 0x16,
  BEZIER        = 0x17,  // Custom bezier curve
};

// Evaluate easing function
inline float evaluateEasing(EasingType type, float t) {
  // Clamp t to [0, 1]
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  
  switch (type) {
    case EasingType::LINEAR:
      return t;
      
    case EasingType::EASE_IN:
      return t * t;
      
    case EasingType::EASE_OUT:
      return t * (2 - t);
      
    case EasingType::EASE_IN_OUT:
      return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t;
      
    case EasingType::EASE_IN_CUBIC:
      return t * t * t;
      
    case EasingType::EASE_OUT_CUBIC: {
      float t1 = t - 1;
      return t1 * t1 * t1 + 1;
    }
      
    case EasingType::EASE_IN_OUT_CUBIC:
      return t < 0.5f ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;
      
    case EasingType::EASE_IN_QUART:
      return t * t * t * t;
      
    case EasingType::EASE_OUT_QUART: {
      float t1 = t - 1;
      return 1 - t1 * t1 * t1 * t1;
    }
      
    case EasingType::EASE_IN_OUT_QUART: {
      if (t < 0.5f) return 8 * t * t * t * t;
      float t1 = t - 1;
      return 1 - 8 * t1 * t1 * t1 * t1;
    }
      
    case EasingType::EASE_IN_EXPO:
      return t == 0 ? 0 : powf(2, 10 * (t - 1));
      
    case EasingType::EASE_OUT_EXPO:
      return t == 1 ? 1 : 1 - powf(2, -10 * t);
      
    case EasingType::EASE_IN_OUT_EXPO:
      if (t == 0) return 0;
      if (t == 1) return 1;
      if (t < 0.5f) return powf(2, 20 * t - 10) / 2;
      return (2 - powf(2, -20 * t + 10)) / 2;
      
    case EasingType::EASE_IN_BACK: {
      float c1 = 1.70158f;
      float c3 = c1 + 1;
      return c3 * t * t * t - c1 * t * t;
    }
      
    case EasingType::EASE_OUT_BACK: {
      float c1 = 1.70158f;
      float c3 = c1 + 1;
      float t1 = t - 1;
      return 1 + c3 * t1 * t1 * t1 + c1 * t1 * t1;
    }
      
    case EasingType::EASE_OUT_BOUNCE: {
      float n1 = 7.5625f;
      float d1 = 2.75f;
      if (t < 1 / d1) return n1 * t * t;
      if (t < 2 / d1) { t -= 1.5f / d1; return n1 * t * t + 0.75f; }
      if (t < 2.5f / d1) { t -= 2.25f / d1; return n1 * t * t + 0.9375f; }
      t -= 2.625f / d1;
      return n1 * t * t + 0.984375f;
    }
      
    case EasingType::EASE_IN_BOUNCE:
      return 1 - evaluateEasing(EasingType::EASE_OUT_BOUNCE, 1 - t);
      
    case EasingType::STEP:
      return t < 0.5f ? 0 : 1;
      
    case EasingType::SMOOTH_STEP:
      return t * t * (3 - 2 * t);
      
    case EasingType::SMOOTHER_STEP:
      return t * t * t * (t * (t * 6 - 15) + 10);
      
    default:
      return t;
  }
}

// ============================================================
// Blend Modes
// ============================================================

enum class BlendMode : uint8_t {
  NORMAL      = 0x00,  // Standard alpha blending
  ADD         = 0x01,  // Additive blending
  MULTIPLY    = 0x02,  // Multiply colors
  SCREEN      = 0x03,  // Screen blend
  OVERLAY     = 0x04,  // Overlay
  DARKEN      = 0x05,  // Take darker
  LIGHTEN     = 0x06,  // Take lighter
  COLOR_DODGE = 0x07,
  COLOR_BURN  = 0x08,
  HARD_LIGHT  = 0x09,
  SOFT_LIGHT  = 0x0A,
  DIFFERENCE  = 0x0B,
  EXCLUSION   = 0x0C,
  HUE         = 0x0D,
  SATURATION  = 0x0E,
  COLOR       = 0x0F,
  LUMINOSITY  = 0x10,
  REPLACE     = 0x11,  // No blending, replace
};

// Apply blend mode
inline ColorF applyBlendMode(BlendMode mode, const ColorF& base, const ColorF& top) {
  switch (mode) {
    case BlendMode::NORMAL:
      return base.blend(top);
      
    case BlendMode::ADD:
      return ColorF(
        fminf(1.0f, base.r + top.r * top.a),
        fminf(1.0f, base.g + top.g * top.a),
        fminf(1.0f, base.b + top.b * top.a),
        fminf(1.0f, base.a + top.a)
      );
      
    case BlendMode::MULTIPLY:
      return ColorF(
        base.r * top.r,
        base.g * top.g,
        base.b * top.b,
        base.a * top.a
      );
      
    case BlendMode::SCREEN:
      return ColorF(
        1 - (1 - base.r) * (1 - top.r),
        1 - (1 - base.g) * (1 - top.g),
        1 - (1 - base.b) * (1 - top.b),
        1 - (1 - base.a) * (1 - top.a)
      );
      
    case BlendMode::DARKEN:
      return ColorF(
        fminf(base.r, top.r),
        fminf(base.g, top.g),
        fminf(base.b, top.b),
        fmaxf(base.a, top.a)
      );
      
    case BlendMode::LIGHTEN:
      return ColorF(
        fmaxf(base.r, top.r),
        fmaxf(base.g, top.g),
        fmaxf(base.b, top.b),
        fmaxf(base.a, top.a)
      );
      
    case BlendMode::DIFFERENCE:
      return ColorF(
        fabsf(base.r - top.r),
        fabsf(base.g - top.g),
        fabsf(base.b - top.b),
        base.a + top.a * (1 - base.a)
      );
      
    case BlendMode::REPLACE:
      return top;
      
    default:
      return base.blend(top);
  }
}

// ============================================================
// Runtime Value Container
// ============================================================

union RuntimeValue {
  bool     b;
  uint8_t  u8;
  int8_t   i8;
  uint16_t u16;
  int16_t  i16;
  uint32_t u32;
  int32_t  i32;
  float    f32;
  uint8_t  raw[8];
};

// ============================================================
// Instruction Builder Helpers
// ============================================================

class BytecodeWriter {
public:
  BytecodeWriter(uint8_t* buffer, size_t max_size)
    : buffer_(buffer), max_size_(max_size), pos_(0), error_(false) {}
  
  bool hasError() const { return error_; }
  size_t position() const { return pos_; }
  
  void write8(uint8_t v) {
    if (pos_ < max_size_) {
      buffer_[pos_++] = v;
    } else {
      error_ = true;
    }
  }
  
  void write16(uint16_t v) {
    write8(v & 0xFF);
    write8((v >> 8) & 0xFF);
  }
  
  void write32(uint32_t v) {
    write16(v & 0xFFFF);
    write16((v >> 16) & 0xFFFF);
  }
  
  void writeFloat(float v) {
    uint32_t u;
    memcpy(&u, &v, sizeof(u));
    write32(u);
  }
  
  void writeOpcode(Opcode op, uint8_t flags = 0) {
    write8(static_cast<uint8_t>(op));
    write8(flags);
  }
  
  void writeString(const char* str, size_t max_len = MAX_STRING_LENGTH) {
    size_t len = strlen(str);
    if (len > max_len) len = max_len;
    write8(static_cast<uint8_t>(len));
    for (size_t i = 0; i < len; i++) {
      write8(static_cast<uint8_t>(str[i]));
    }
  }

private:
  uint8_t* buffer_;
  size_t   max_size_;
  size_t   pos_;
  bool     error_;
};

// ============================================================
// Bytecode Reader
// ============================================================

class BytecodeReader {
public:
  BytecodeReader(const uint8_t* data, size_t len)
    : data_(data), len_(len), pos_(0), error_(false) {}
  
  bool hasError() const { return error_; }
  bool atEnd() const { return pos_ >= len_; }
  size_t position() const { return pos_; }
  size_t remaining() const { return pos_ < len_ ? len_ - pos_ : 0; }
  
  void seek(size_t pos) {
    if (pos <= len_) {
      pos_ = pos;
      error_ = false;
    } else {
      error_ = true;
    }
  }
  
  uint8_t read8() {
    if (pos_ < len_) {
      return data_[pos_++];
    }
    error_ = true;
    return 0;
  }
  
  int8_t readS8() {
    return static_cast<int8_t>(read8());
  }
  
  uint16_t read16() {
    uint8_t lo = read8();
    uint8_t hi = read8();
    return lo | (hi << 8);
  }
  
  int16_t readS16() {
    return static_cast<int16_t>(read16());
  }
  
  uint32_t read32() {
    uint16_t lo = read16();
    uint16_t hi = read16();
    return lo | (hi << 16);
  }
  
  int32_t readS32() {
    return static_cast<int32_t>(read32());
  }
  
  float readFloat() {
    uint32_t u = read32();
    float f;
    memcpy(&f, &u, sizeof(f));
    return f;
  }
  
  Opcode readOpcode() {
    return static_cast<Opcode>(read8());
  }
  
  void readString(char* out, size_t out_size) {
    uint8_t len = read8();
    size_t copy_len = len < out_size - 1 ? len : out_size - 1;
    for (size_t i = 0; i < len; i++) {
      char c = static_cast<char>(read8());
      if (i < copy_len) out[i] = c;
    }
    out[copy_len] = '\0';
  }

private:
  const uint8_t* data_;
  size_t len_;
  size_t pos_;
  bool error_;
};

} // namespace isa
} // namespace gpu

#endif // GPU_ISA_HPP_
