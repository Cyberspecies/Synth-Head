/*****************************************************************
 * File:      GpuValidator.hpp
 * Category:  GPU Driver / Validation System
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Comprehensive compile-time and runtime validation for the
 *    GPU programming language. Validates syntax, types, memory
 *    safety, synchronization, and execution correctness.
 * 
 * Validation Levels:
 *    1. Compile-time: Syntax, typing, static constraints
 *    2. Link-time: Symbol resolution, memory layout
 *    3. Runtime: Safety checks, bounds, synchronization
 *****************************************************************/

#ifndef GPU_VALIDATOR_HPP_
#define GPU_VALIDATOR_HPP_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "GpuISA.hpp"

namespace gpu {
namespace validator {

using namespace isa;

// ============================================================
// Validation Error Codes
// ============================================================

enum class ValidationError : uint16_t {
  NONE                    = 0x0000,
  
  // Syntax Errors (0x01xx)
  SYNTAX_INVALID_OPCODE   = 0x0100,
  SYNTAX_UNEXPECTED_END   = 0x0101,
  SYNTAX_INVALID_OPERAND  = 0x0102,
  SYNTAX_MISSING_OPERAND  = 0x0103,
  SYNTAX_EXTRA_OPERAND    = 0x0104,
  SYNTAX_INVALID_STRING   = 0x0105,
  SYNTAX_INVALID_LABEL    = 0x0106,
  SYNTAX_DUPLICATE_LABEL  = 0x0107,
  
  // Type Errors (0x02xx)
  TYPE_MISMATCH           = 0x0200,
  TYPE_INVALID_CAST       = 0x0201,
  TYPE_OVERFLOW           = 0x0202,
  TYPE_UNDERFLOW          = 0x0203,
  TYPE_PRECISION_LOSS     = 0x0204,
  TYPE_INVALID_OPERATION  = 0x0205,
  TYPE_NULL_REFERENCE     = 0x0206,
  
  // Memory Errors (0x03xx)
  MEMORY_OUT_OF_BOUNDS    = 0x0300,
  MEMORY_STACK_OVERFLOW   = 0x0301,
  MEMORY_STACK_UNDERFLOW  = 0x0302,
  MEMORY_BUFFER_OVERFLOW  = 0x0303,
  MEMORY_INVALID_ADDRESS  = 0x0304,
  MEMORY_ALIGNMENT        = 0x0305,
  MEMORY_DOUBLE_FREE      = 0x0306,
  MEMORY_LEAK             = 0x0307,
  
  // Flow Control Errors (0x04xx)
  FLOW_INVALID_JUMP       = 0x0400,
  FLOW_UNDEFINED_LABEL    = 0x0401,
  FLOW_LOOP_MISMATCH      = 0x0402,
  FLOW_CALL_DEPTH         = 0x0403,
  FLOW_INFINITE_LOOP      = 0x0404,
  FLOW_UNREACHABLE_CODE   = 0x0405,
  FLOW_MISSING_RETURN     = 0x0406,
  
  // Resource Errors (0x05xx)
  RESOURCE_NOT_FOUND      = 0x0500,
  RESOURCE_ALREADY_EXISTS = 0x0501,
  RESOURCE_LIMIT_EXCEEDED = 0x0502,
  RESOURCE_LOCKED         = 0x0503,
  RESOURCE_INVALID_STATE  = 0x0504,
  
  // Synchronization Errors (0x06xx)
  SYNC_DEADLOCK           = 0x0600,
  SYNC_RACE_CONDITION     = 0x0601,
  SYNC_INVALID_BARRIER    = 0x0602,
  SYNC_TIMEOUT            = 0x0603,
  
  // Render State Errors (0x07xx)
  RENDER_INVALID_TARGET   = 0x0700,
  RENDER_INVALID_CLIP     = 0x0701,
  RENDER_BUFFER_LOCKED    = 0x0702,
  RENDER_INVALID_BLEND    = 0x0703,
  
  // Animation Errors (0x08xx)
  ANIM_INVALID_ID         = 0x0800,
  ANIM_INVALID_KEYFRAME   = 0x0801,
  ANIM_INVALID_TRANSITION = 0x0802,
  ANIM_CYCLE_DETECTED     = 0x0803,
  
  // Internal Errors (0xFFxx)
  INTERNAL_ERROR          = 0xFF00,
  INTERNAL_NOT_IMPLEMENTED= 0xFF01,
};

// ============================================================
// Validation Result
// ============================================================

struct ValidationResult {
  ValidationError error;
  uint32_t        offset;       // Byte offset in program
  uint32_t        line;         // Source line (if available)
  uint8_t         opcode;       // Offending opcode
  char            message[128]; // Human-readable message
  
  bool isValid() const { return error == ValidationError::NONE; }
  
  void clear() {
    error = ValidationError::NONE;
    offset = 0;
    line = 0;
    opcode = 0;
    message[0] = '\0';
  }
  
  void setError(ValidationError err, uint32_t off, const char* msg) {
    error = err;
    offset = off;
    strncpy(message, msg, sizeof(message) - 1);
    message[sizeof(message) - 1] = '\0';
  }
};

// ============================================================
// Validation Context
// ============================================================

struct ValidationContext {
  // Program state
  const uint8_t* program;
  size_t         program_size;
  size_t         pc;  // Program counter
  
  // Stack tracking
  int32_t        stack_depth;
  DataType       stack_types[MAX_STACK_DEPTH];
  
  // Variable state
  bool           var_defined[MAX_VARIABLES];
  DataType       var_types[MAX_VARIABLES];
  
  // Label tracking
  struct Label {
    uint32_t offset;
    bool     resolved;
    char     name[32];
  };
  Label          labels[MAX_LABELS];
  size_t         label_count;
  
  // Loop tracking
  struct LoopInfo {
    uint32_t start_offset;
    uint32_t depth;
    int32_t  max_iterations;  // -1 = unknown
  };
  LoopInfo       loop_stack[MAX_STACK_DEPTH];
  size_t         loop_depth;
  
  // Call stack tracking
  uint32_t       call_stack[MAX_CALL_DEPTH];
  size_t         call_depth;
  
  // Resource tracking
  bool           buffer_locked;
  uint8_t        active_display;
  
  // Statistics
  uint32_t       instruction_count;
  uint32_t       branch_count;
  uint32_t       memory_ops;
  
  void reset() {
    pc = 0;
    stack_depth = 0;
    label_count = 0;
    loop_depth = 0;
    call_depth = 0;
    buffer_locked = false;
    active_display = 0;
    instruction_count = 0;
    branch_count = 0;
    memory_ops = 0;
    memset(var_defined, 0, sizeof(var_defined));
    memset(labels, 0, sizeof(labels));
    memset(loop_stack, 0, sizeof(loop_stack));
    memset(call_stack, 0, sizeof(call_stack));
  }
};

// ============================================================
// Instruction Validator
// ============================================================

class InstructionValidator {
public:
  // Get expected operand count for opcode
  static int getOperandCount(Opcode op) {
    switch (op) {
      // No operands
      case Opcode::NOP:
      case Opcode::HALT:
      case Opcode::YIELD:
      case Opcode::SYNC:
      case Opcode::RETURN:
        return 0;
        
      // 1 operand
      case Opcode::JUMP:
      case Opcode::PUSH:
      case Opcode::POP:
      case Opcode::NEG:
      case Opcode::NOT:
      case Opcode::LNOT:
      case Opcode::SQRT:
      case Opcode::BUF_CLEAR:
      case Opcode::BUF_SWAP:
        return 1;
        
      // 2 operands
      case Opcode::JUMP_IF:
      case Opcode::JUMP_UNLESS:
      case Opcode::LOAD:
      case Opcode::STORE:
      case Opcode::ADD:
      case Opcode::SUB:
      case Opcode::MUL:
      case Opcode::DIV:
      case Opcode::MOD:
      case Opcode::AND:
      case Opcode::OR:
      case Opcode::XOR:
      case Opcode::CMP_EQ:
      case Opcode::CMP_NE:
      case Opcode::CMP_LT:
      case Opcode::CMP_LE:
      case Opcode::CMP_GT:
      case Opcode::CMP_GE:
        return 2;
        
      // 3 operands
      case Opcode::LERP:
      case Opcode::CLAMP:
      case Opcode::SELECT:
      case Opcode::FMA:
        return 3;
        
      // Drawing (variable)
      case Opcode::DRAW_PIXEL:  // x, y, color
        return 3;
      case Opcode::DRAW_LINE:   // x0, y0, x1, y1, color
        return 5;
      case Opcode::DRAW_RECT:   // x, y, w, h, color
      case Opcode::DRAW_FILL_RECT:
        return 5;
      case Opcode::DRAW_CIRCLE: // cx, cy, r, color
      case Opcode::DRAW_FILL_CIRC:
        return 4;
        
      // Text
      case Opcode::TEXT_DRAW:   // x, y, string
        return 3;
      case Opcode::TEXT_SET_COLOR:
        return 1;
      case Opcode::TEXT_SET_SIZE:
        return 1;
        
      // Animation
      case Opcode::ANIM_CREATE:
        return 4;
      case Opcode::ANIM_START:
      case Opcode::ANIM_STOP:
      case Opcode::ANIM_PAUSE:
      case Opcode::ANIM_RESUME:
        return 1;
        
      // SDF
      case Opcode::SDF_CIRCLE:  // cx, cy, r
        return 3;
      case Opcode::SDF_BOX:     // x, y, w, h
        return 4;
      case Opcode::SDF_UNION:
      case Opcode::SDF_SUBTRACT:
      case Opcode::SDF_INTERSECT:
        return 2;
        
      default:
        return -1;  // Unknown
    }
  }
  
  // Get expected operand types
  static bool getOperandTypes(Opcode op, DataType* types, int count) {
    if (count <= 0) return true;
    
    switch (op) {
      case Opcode::DRAW_PIXEL:
        if (count >= 3) {
          types[0] = DataType::INT16;  // x
          types[1] = DataType::INT16;  // y
          types[2] = DataType::COLOR_RGB;
        }
        return true;
        
      case Opcode::DRAW_LINE:
        if (count >= 5) {
          types[0] = DataType::INT16;  // x0
          types[1] = DataType::INT16;  // y0
          types[2] = DataType::INT16;  // x1
          types[3] = DataType::INT16;  // y1
          types[4] = DataType::COLOR_RGB;
        }
        return true;
        
      case Opcode::DRAW_RECT:
      case Opcode::DRAW_FILL_RECT:
        if (count >= 5) {
          types[0] = DataType::INT16;   // x
          types[1] = DataType::INT16;   // y
          types[2] = DataType::UINT16;  // width
          types[3] = DataType::UINT16;  // height
          types[4] = DataType::COLOR_RGB;
        }
        return true;
        
      case Opcode::LERP:
        if (count >= 3) {
          types[0] = DataType::FLOAT32; // a
          types[1] = DataType::FLOAT32; // b
          types[2] = DataType::FLOAT32; // t
        }
        return true;
        
      default:
        // Default to int32 for arithmetic ops
        for (int i = 0; i < count; i++) {
          types[i] = DataType::INT32;
        }
        return true;
    }
  }
  
  // Check if opcode is valid for execution context
  static bool isValidForContext(Opcode op, ExecutionContext ctx) {
    switch (ctx) {
      case ExecutionContext::IMMEDIATE:
        return true;  // All opcodes valid
        
      case ExecutionContext::VERTEX:
        // Drawing ops not valid in vertex shader
        return (static_cast<uint8_t>(op) & 0xF0) != 0x70;
        
      case ExecutionContext::FRAGMENT:
        // Most ops valid in fragment shader
        return true;
        
      case ExecutionContext::COMPUTE:
        // No drawing or buffer ops in compute
        return (static_cast<uint8_t>(op) & 0xF0) != 0x70 &&
               (static_cast<uint8_t>(op) & 0xF0) != 0xC0;
        
      case ExecutionContext::ANIMATION:
        // Only flow, arithmetic, animation ops
        return (static_cast<uint8_t>(op) & 0xF0) == 0x10 ||  // Flow
               (static_cast<uint8_t>(op) & 0xF0) == 0x30 ||  // Arith
               (static_cast<uint8_t>(op) & 0xF0) == 0xA0;    // Anim
        
      default:
        return false;
    }
  }
  
  // Check if operation modifies render state
  static bool modifiesRenderState(Opcode op) {
    uint8_t cat = static_cast<uint8_t>(op) & 0xF0;
    return cat == 0x70 || cat == 0x80 || cat == 0xC0;  // Draw, Text, Buffer
  }
  
  // Check if operation has side effects
  static bool hasSideEffects(Opcode op) {
    switch (op) {
      case Opcode::STORE:
      case Opcode::STORE_UNIFORM:
      case Opcode::TRIGGER:
        return true;
      default:
        return modifiesRenderState(op);
    }
  }
};

// ============================================================
// Compile-Time Validator
// ============================================================

class CompileTimeValidator {
public:
  ValidationResult validate(const uint8_t* program, size_t size) {
    result_.clear();
    ctx_.program = program;
    ctx_.program_size = size;
    ctx_.reset();
    
    if (size == 0) {
      result_.setError(ValidationError::SYNTAX_UNEXPECTED_END, 0, "Empty program");
      return result_;
    }
    
    // First pass: collect labels and validate syntax
    if (!firstPass()) {
      return result_;
    }
    
    // Second pass: validate control flow and types
    ctx_.pc = 0;
    if (!secondPass()) {
      return result_;
    }
    
    // Third pass: check for unreachable code
    if (!checkReachability()) {
      return result_;
    }
    
    return result_;
  }
  
  const ValidationContext& getContext() const { return ctx_; }

private:
  ValidationResult   result_;
  ValidationContext  ctx_;
  
  bool firstPass() {
    BytecodeReader reader(ctx_.program, ctx_.program_size);
    
    while (!reader.atEnd()) {
      uint32_t offset = reader.position();
      Opcode op = reader.readOpcode();
      uint8_t flags = reader.read8();
      
      if (reader.hasError()) {
        result_.setError(ValidationError::SYNTAX_UNEXPECTED_END, offset,
                         "Unexpected end of program");
        return false;
      }
      
      // Validate opcode
      if (op == Opcode::ILLEGAL) {
        result_.setError(ValidationError::SYNTAX_INVALID_OPCODE, offset,
                         "Illegal opcode 0xFF");
        result_.opcode = static_cast<uint8_t>(op);
        return false;
      }
      
      // Skip operands based on opcode
      int operand_count = InstructionValidator::getOperandCount(op);
      if (operand_count < 0) {
        // Unknown opcode
        result_.setError(ValidationError::SYNTAX_INVALID_OPCODE, offset,
                         "Unknown opcode");
        result_.opcode = static_cast<uint8_t>(op);
        return false;
      }
      
      // Skip operand bytes (simplified - real impl would parse properly)
      if (!skipOperands(reader, op, operand_count)) {
        result_.setError(ValidationError::SYNTAX_MISSING_OPERAND, offset,
                         "Missing operand data");
        return false;
      }
      
      ctx_.instruction_count++;
    }
    
    return true;
  }
  
  bool secondPass() {
    BytecodeReader reader(ctx_.program, ctx_.program_size);
    
    while (!reader.atEnd()) {
      uint32_t offset = reader.position();
      Opcode op = reader.readOpcode();
      uint8_t flags = reader.read8();
      
      // Validate based on opcode category
      switch (static_cast<uint8_t>(op) & 0xF0) {
        case 0x10:  // Flow control
          if (!validateFlowControl(reader, op, flags, offset)) {
            return false;
          }
          break;
          
        case 0x20:  // Memory
          if (!validateMemoryOp(reader, op, flags, offset)) {
            return false;
          }
          break;
          
        case 0x30:  // Arithmetic
          if (!validateArithmetic(reader, op, flags, offset)) {
            return false;
          }
          break;
          
        case 0x70:  // Drawing
          if (!validateDrawing(reader, op, flags, offset)) {
            return false;
          }
          break;
          
        case 0xA0:  // Animation
          if (!validateAnimation(reader, op, flags, offset)) {
            return false;
          }
          break;
          
        default:
          // Skip other opcodes with basic operand parsing
          int operand_count = InstructionValidator::getOperandCount(op);
          skipOperands(reader, op, operand_count);
          break;
      }
    }
    
    // Check for unclosed loops
    if (ctx_.loop_depth > 0) {
      result_.setError(ValidationError::FLOW_LOOP_MISMATCH, 0,
                       "Unclosed loop(s) at end of program");
      return false;
    }
    
    return true;
  }
  
  bool validateFlowControl(BytecodeReader& reader, Opcode op, uint8_t flags, uint32_t offset) {
    switch (op) {
      case Opcode::JUMP:
      case Opcode::JUMP_IF:
      case Opcode::JUMP_UNLESS: {
        uint16_t target = reader.read16();
        if (target >= ctx_.program_size) {
          result_.setError(ValidationError::FLOW_INVALID_JUMP, offset,
                           "Jump target out of bounds");
          return false;
        }
        ctx_.branch_count++;
        break;
      }
      
      case Opcode::LOOP_START: {
        if (ctx_.loop_depth >= MAX_STACK_DEPTH) {
          result_.setError(ValidationError::MEMORY_STACK_OVERFLOW, offset,
                           "Loop nesting too deep");
          return false;
        }
        ctx_.loop_stack[ctx_.loop_depth].start_offset = offset;
        ctx_.loop_stack[ctx_.loop_depth].depth = ctx_.loop_depth;
        ctx_.loop_depth++;
        break;
      }
      
      case Opcode::LOOP_END: {
        if (ctx_.loop_depth == 0) {
          result_.setError(ValidationError::FLOW_LOOP_MISMATCH, offset,
                           "LOOP_END without matching LOOP_START");
          return false;
        }
        ctx_.loop_depth--;
        break;
      }
      
      case Opcode::CALL: {
        uint8_t target_id = reader.read8();
        if (ctx_.call_depth >= MAX_CALL_DEPTH) {
          result_.setError(ValidationError::FLOW_CALL_DEPTH, offset,
                           "Call stack depth exceeded");
          return false;
        }
        ctx_.call_stack[ctx_.call_depth++] = offset;
        break;
      }
      
      case Opcode::RETURN: {
        // Note: We don't decrement call_depth here since this is static analysis
        // Runtime will handle actual call/return matching
        break;
      }
      
      case Opcode::DELAY: {
        uint16_t ms = reader.read16();
        // Valid range check
        if (ms > 60000) {
          result_.setError(ValidationError::TYPE_OVERFLOW, offset,
                           "Delay too long (max 60000ms)");
          return false;
        }
        break;
      }
      
      default:
        break;
    }
    
    return true;
  }
  
  bool validateMemoryOp(BytecodeReader& reader, Opcode op, uint8_t flags, uint32_t offset) {
    switch (op) {
      case Opcode::LOAD:
      case Opcode::STORE: {
        uint8_t var_id = reader.read8();
        if (var_id >= MAX_VARIABLES) {
          result_.setError(ValidationError::MEMORY_OUT_OF_BOUNDS, offset,
                           "Variable index out of range");
          return false;
        }
        if (op == Opcode::STORE) {
          ctx_.var_defined[var_id] = true;
        } else if (!ctx_.var_defined[var_id]) {
          // Warning: reading undefined variable
          // (Not an error, but could be flagged)
        }
        ctx_.memory_ops++;
        break;
      }
      
      case Opcode::PUSH: {
        if (ctx_.stack_depth >= MAX_STACK_DEPTH) {
          result_.setError(ValidationError::MEMORY_STACK_OVERFLOW, offset,
                           "Stack overflow");
          return false;
        }
        ctx_.stack_depth++;
        break;
      }
      
      case Opcode::POP: {
        if (ctx_.stack_depth <= 0) {
          result_.setError(ValidationError::MEMORY_STACK_UNDERFLOW, offset,
                           "Stack underflow");
          return false;
        }
        ctx_.stack_depth--;
        break;
      }
      
      default:
        break;
    }
    
    return true;
  }
  
  bool validateArithmetic(BytecodeReader& reader, Opcode op, uint8_t flags, uint32_t offset) {
    switch (op) {
      case Opcode::DIV:
      case Opcode::MOD: {
        // Note: Division by zero is a runtime check
        // Here we just ensure operands are present
        if (flags & InstructionFlags::IMMEDIATE) {
          int32_t divisor = reader.readS32();
          if (divisor == 0) {
            result_.setError(ValidationError::TYPE_INVALID_OPERATION, offset,
                             "Division by constant zero");
            return false;
          }
        }
        break;
      }
      
      case Opcode::SQRT:
      case Opcode::RSQRT: {
        // Note: Sqrt of negative is runtime check
        break;
      }
      
      default:
        break;
    }
    
    return true;
  }
  
  bool validateDrawing(BytecodeReader& reader, Opcode op, uint8_t flags, uint32_t offset) {
    switch (op) {
      case Opcode::DRAW_PIXEL: {
        int16_t x = reader.readS16();
        int16_t y = reader.readS16();
        reader.read8();  // r
        reader.read8();  // g
        reader.read8();  // b
        // Bounds checking is runtime (for clipping support)
        break;
      }
      
      case Opcode::DRAW_LINE: {
        reader.readS16();  // x0
        reader.readS16();  // y0
        reader.readS16();  // x1
        reader.readS16();  // y1
        reader.read8();    // r
        reader.read8();    // g
        reader.read8();    // b
        break;
      }
      
      case Opcode::DRAW_RECT:
      case Opcode::DRAW_FILL_RECT: {
        reader.readS16();  // x
        reader.readS16();  // y
        uint16_t w = reader.read16();  // width
        uint16_t h = reader.read16();  // height
        reader.read8();    // r
        reader.read8();    // g
        reader.read8();    // b
        
        if (w == 0 || h == 0) {
          // Warning: zero-size rectangle
        }
        break;
      }
      
      case Opcode::DRAW_CIRCLE:
      case Opcode::DRAW_FILL_CIRC: {
        reader.readS16();  // cx
        reader.readS16();  // cy
        uint16_t r = reader.read16();  // radius
        reader.read8();    // r
        reader.read8();    // g
        reader.read8();    // b
        
        if (r == 0) {
          // Warning: zero-radius circle
        }
        break;
      }
      
      default:
        break;
    }
    
    if (ctx_.buffer_locked) {
      result_.setError(ValidationError::RENDER_BUFFER_LOCKED, offset,
                       "Drawing while buffer is locked");
      return false;
    }
    
    return true;
  }
  
  bool validateAnimation(BytecodeReader& reader, Opcode op, uint8_t flags, uint32_t offset) {
    switch (op) {
      case Opcode::ANIM_CREATE: {
        uint8_t anim_id = reader.read8();
        if (anim_id >= 32) {  // MAX_ANIMATIONS
          result_.setError(ValidationError::ANIM_INVALID_ID, offset,
                           "Animation ID out of range");
          return false;
        }
        reader.read8();   // type
        reader.read16();  // duration
        reader.read8();   // flags
        break;
      }
      
      case Opcode::ANIM_START:
      case Opcode::ANIM_STOP:
      case Opcode::ANIM_PAUSE:
      case Opcode::ANIM_RESUME: {
        uint8_t anim_id = reader.read8();
        if (anim_id >= 32) {
          result_.setError(ValidationError::ANIM_INVALID_ID, offset,
                           "Animation ID out of range");
          return false;
        }
        break;
      }
      
      case Opcode::ANIM_KEYFRAME: {
        uint8_t anim_id = reader.read8();
        uint16_t time = reader.read16();
        // Read keyframe data based on animation type
        break;
      }
      
      default:
        break;
    }
    
    return true;
  }
  
  bool checkReachability() {
    // Simplified reachability analysis
    // A full implementation would build a control flow graph
    
    // Check for code after HALT
    BytecodeReader reader(ctx_.program, ctx_.program_size);
    bool halt_seen = false;
    uint32_t halt_offset = 0;
    
    while (!reader.atEnd()) {
      uint32_t offset = reader.position();
      Opcode op = reader.readOpcode();
      reader.read8();  // flags
      
      if (halt_seen) {
        // Code after HALT - potentially unreachable
        // (Unless there's a jump target here)
        // For now, just note it
      }
      
      if (op == Opcode::HALT) {
        halt_seen = true;
        halt_offset = offset;
      } else if (op == Opcode::JUMP || op == Opcode::JUMP_IF || op == Opcode::JUMP_UNLESS) {
        // Reset halt_seen if we see a jump (target could be after halt)
        halt_seen = false;
      }
      
      // Skip operands
      int operand_count = InstructionValidator::getOperandCount(op);
      skipOperands(reader, op, operand_count);
    }
    
    return true;
  }
  
  bool skipOperands(BytecodeReader& reader, Opcode op, int count) {
    // Calculate bytes to skip based on opcode
    size_t bytes = 0;
    
    switch (op) {
      case Opcode::DRAW_PIXEL:    bytes = 7; break;   // x(2) + y(2) + rgb(3)
      case Opcode::DRAW_LINE:     bytes = 11; break;  // x0,y0,x1,y1(8) + rgb(3)
      case Opcode::DRAW_RECT:
      case Opcode::DRAW_FILL_RECT: bytes = 11; break; // x,y,w,h(8) + rgb(3)
      case Opcode::DRAW_CIRCLE:
      case Opcode::DRAW_FILL_CIRC: bytes = 9; break;  // cx,cy,r(6) + rgb(3)
      case Opcode::JUMP:
      case Opcode::JUMP_IF:
      case Opcode::JUMP_UNLESS:   bytes = 2; break;   // offset(2)
      case Opcode::DELAY:         bytes = 2; break;   // ms(2)
      case Opcode::CALL:          bytes = 1; break;   // target(1)
      case Opcode::LOAD:
      case Opcode::STORE:         bytes = 1; break;   // var(1)
      case Opcode::ANIM_CREATE:   bytes = 5; break;   // id,type,dur,flags
      case Opcode::ANIM_START:
      case Opcode::ANIM_STOP:     bytes = 1; break;   // id
      default:
        bytes = count * 4;  // Assume 4 bytes per operand as fallback
        break;
    }
    
    for (size_t i = 0; i < bytes && !reader.hasError(); i++) {
      reader.read8();
    }
    
    return !reader.hasError();
  }
};

// ============================================================
// Runtime Validator
// ============================================================

class RuntimeValidator {
public:
  RuntimeValidator() : enabled_(true) {}
  
  void setEnabled(bool enabled) { enabled_ = enabled; }
  bool isEnabled() const { return enabled_; }
  
  // Memory bounds checking
  ValidationResult checkMemoryAccess(void* base, size_t size, size_t offset, size_t len) {
    ValidationResult result;
    result.clear();
    
    if (!enabled_) return result;
    
    if (offset + len > size) {
      result.setError(ValidationError::MEMORY_OUT_OF_BOUNDS, offset,
                      "Memory access out of bounds");
    }
    
    return result;
  }
  
  // Buffer bounds checking
  ValidationResult checkBufferBounds(int16_t x, int16_t y, uint16_t width, uint16_t height,
                                      int16_t buf_width, int16_t buf_height) {
    ValidationResult result;
    result.clear();
    
    if (!enabled_) return result;
    
    // Check if any part of the rectangle is within bounds
    if (x >= buf_width || y >= buf_height || 
        x + (int16_t)width < 0 || y + (int16_t)height < 0) {
      result.setError(ValidationError::RENDER_INVALID_CLIP, 0,
                      "Drawing completely outside buffer");
    }
    
    return result;
  }
  
  // Division by zero
  ValidationResult checkDivision(int32_t divisor, uint32_t offset) {
    ValidationResult result;
    result.clear();
    
    if (!enabled_) return result;
    
    if (divisor == 0) {
      result.setError(ValidationError::TYPE_INVALID_OPERATION, offset,
                      "Division by zero");
    }
    
    return result;
  }
  
  // Stack bounds
  ValidationResult checkStackPush(int32_t current_depth) {
    ValidationResult result;
    result.clear();
    
    if (!enabled_) return result;
    
    if (current_depth >= (int32_t)MAX_STACK_DEPTH) {
      result.setError(ValidationError::MEMORY_STACK_OVERFLOW, 0,
                      "Stack overflow");
    }
    
    return result;
  }
  
  ValidationResult checkStackPop(int32_t current_depth) {
    ValidationResult result;
    result.clear();
    
    if (!enabled_) return result;
    
    if (current_depth <= 0) {
      result.setError(ValidationError::MEMORY_STACK_UNDERFLOW, 0,
                      "Stack underflow");
    }
    
    return result;
  }
  
  // Animation ID validation
  ValidationResult checkAnimationId(uint8_t id, uint8_t max_id) {
    ValidationResult result;
    result.clear();
    
    if (!enabled_) return result;
    
    if (id >= max_id) {
      result.setError(ValidationError::ANIM_INVALID_ID, 0,
                      "Animation ID out of range");
    }
    
    return result;
  }
  
  // Numeric overflow/underflow detection
  template<typename T>
  ValidationResult checkOverflow(T a, T b, T result_val, bool is_add) {
    ValidationResult result;
    result.clear();
    
    if (!enabled_) return result;
    
    // Check for signed overflow
    if (is_add) {
      if ((b > 0 && a > (T)(~(T)0 >> 1) - b) ||
          (b < 0 && a < (T)(1 << (sizeof(T) * 8 - 1)) - b)) {
        result.setError(ValidationError::TYPE_OVERFLOW, 0,
                        "Integer overflow in addition");
      }
    }
    
    return result;
  }
  
  // NaN/Inf checking for floats
  ValidationResult checkFloatValidity(float value, uint32_t offset) {
    ValidationResult result;
    result.clear();
    
    if (!enabled_) return result;
    
    // Check for NaN
    if (value != value) {
      result.setError(ValidationError::TYPE_INVALID_OPERATION, offset,
                      "NaN result");
    }
    // Check for infinity
    else if (value > 3.4e38f || value < -3.4e38f) {
      result.setError(ValidationError::TYPE_OVERFLOW, offset,
                      "Float overflow (infinity)");
    }
    
    return result;
  }

private:
  bool enabled_;
};

// ============================================================
// Validation Statistics
// ============================================================

struct ValidationStats {
  uint32_t programs_validated;
  uint32_t programs_passed;
  uint32_t programs_failed;
  uint32_t total_instructions;
  uint32_t syntax_errors;
  uint32_t type_errors;
  uint32_t memory_errors;
  uint32_t flow_errors;
  uint32_t runtime_checks;
  uint32_t runtime_failures;
  
  void reset() {
    memset(this, 0, sizeof(*this));
  }
};

// ============================================================
// Full Validation Pipeline
// ============================================================

class ValidationPipeline {
public:
  ValidationPipeline() {
    stats_.reset();
  }
  
  // Full validation of a program
  ValidationResult validate(const uint8_t* program, size_t size,
                            ExecutionContext context = ExecutionContext::IMMEDIATE) {
    stats_.programs_validated++;
    
    // Compile-time validation
    ValidationResult result = compile_validator_.validate(program, size);
    
    if (!result.isValid()) {
      stats_.programs_failed++;
      categorizeError(result.error);
      return result;
    }
    
    // Context validation
    result = validateContext(program, size, context);
    if (!result.isValid()) {
      stats_.programs_failed++;
      return result;
    }
    
    stats_.programs_passed++;
    stats_.total_instructions += compile_validator_.getContext().instruction_count;
    
    return result;
  }
  
  // Get runtime validator for use during execution
  RuntimeValidator& getRuntimeValidator() { return runtime_validator_; }
  
  // Get statistics
  const ValidationStats& getStats() const { return stats_; }
  void resetStats() { stats_.reset(); }

private:
  CompileTimeValidator compile_validator_;
  RuntimeValidator     runtime_validator_;
  ValidationStats      stats_;
  
  ValidationResult validateContext(const uint8_t* program, size_t size,
                                   ExecutionContext context) {
    ValidationResult result;
    result.clear();
    
    BytecodeReader reader(program, size);
    
    while (!reader.atEnd()) {
      uint32_t offset = reader.position();
      Opcode op = reader.readOpcode();
      reader.read8();  // flags
      
      if (!InstructionValidator::isValidForContext(op, context)) {
        result.setError(ValidationError::SYNTAX_INVALID_OPCODE, offset,
                        "Opcode not valid for execution context");
        result.opcode = static_cast<uint8_t>(op);
        return result;
      }
      
      // Skip operands
      int count = InstructionValidator::getOperandCount(op);
      for (int i = 0; i < count * 4 && !reader.atEnd(); i++) {
        reader.read8();
      }
    }
    
    return result;
  }
  
  void categorizeError(ValidationError error) {
    uint16_t category = static_cast<uint16_t>(error) & 0xFF00;
    switch (category) {
      case 0x0100: stats_.syntax_errors++; break;
      case 0x0200: stats_.type_errors++; break;
      case 0x0300: stats_.memory_errors++; break;
      case 0x0400: stats_.flow_errors++; break;
      default: break;
    }
  }
};

} // namespace validator
} // namespace gpu

#endif // GPU_VALIDATOR_HPP_
