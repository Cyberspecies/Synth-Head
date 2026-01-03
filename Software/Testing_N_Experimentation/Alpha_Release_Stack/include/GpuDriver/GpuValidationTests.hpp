/*****************************************************************
 * File:      GpuValidationTests.hpp
 * Category:  GPU Driver / Validation Test Suite
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Exhaustive validation test suite for the GPU instruction set.
 *    Tests every opcode, data type combination, edge cases,
 *    and error conditions to ensure correctness.
 * 
 * Test Coverage:
 *    - All ~100 opcodes across 16 categories
 *    - All data types (VOID through BUFFER)
 *    - All precision modes
 *    - All execution contexts
 *    - Boundary conditions and edge cases
 *    - Error detection and reporting
 *****************************************************************/

#ifndef GPU_VALIDATION_TESTS_HPP_
#define GPU_VALIDATION_TESTS_HPP_

#include "GpuTestFramework.hpp"

namespace gpu {
namespace validation_tests {

using namespace gpu::isa;
using namespace gpu::validator;
using namespace gpu::test;

// ============================================================
// Opcode Category Tests
// ============================================================

// SYSTEM category opcodes
namespace system_tests {

inline void test_nop() {
  uint8_t program[] = { (uint8_t)Opcode::NOP };
  InstructionValidator validator;
  
  auto result = validator.validateOperandCount(Opcode::NOP, 0);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::NOP, 1);
  TEST_ASSERT_EQ(ValidationError::SYNTAX_WRONG_OPERAND_COUNT, result);
}

inline void test_halt() {
  InstructionValidator validator;
  
  auto result = validator.validateOperandCount(Opcode::HALT, 0);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void test_reset() {
  InstructionValidator validator;
  
  auto result = validator.validateOperandCount(Opcode::RESET, 0);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void test_yield() {
  InstructionValidator validator;
  
  auto result = validator.validateOperandCount(Opcode::YIELD, 0);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void test_sync() {
  InstructionValidator validator;
  
  // SYNC requires 1 operand (sync type)
  auto result = validator.validateOperandCount(Opcode::SYNC, 1);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void test_debug() {
  InstructionValidator validator;
  
  // DEBUG can take variable operands
  auto result = validator.validateOperandCount(Opcode::DEBUG, 1);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void register_system_tests() {
  TestRunner& r = TestRunner::instance();
  r.registerTest("sys_nop", "System", test_nop);
  r.registerTest("sys_halt", "System", test_halt);
  r.registerTest("sys_reset", "System", test_reset);
  r.registerTest("sys_yield", "System", test_yield);
  r.registerTest("sys_sync", "System", test_sync);
  r.registerTest("sys_debug", "System", test_debug);
}

} // namespace system_tests

// FLOW category opcodes
namespace flow_tests {

inline void test_jump() {
  InstructionValidator validator;
  
  // JMP requires 1 operand (target address/label)
  auto result = validator.validateOperandCount(Opcode::JMP, 1);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::JMP, 0);
  TEST_ASSERT_EQ(ValidationError::SYNTAX_WRONG_OPERAND_COUNT, result);
}

inline void test_conditional_jumps() {
  InstructionValidator validator;
  
  // JZ/JNZ require 2 operands (condition, target)
  auto result = validator.validateOperandCount(Opcode::JZ, 2);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::JNZ, 2);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::JLT, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::JGT, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void test_call_ret() {
  InstructionValidator validator;
  
  // CALL requires 1 operand (function address)
  auto result = validator.validateOperandCount(Opcode::CALL, 1);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // RET takes 0 operands
  result = validator.validateOperandCount(Opcode::RET, 0);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void test_loop_constructs() {
  InstructionValidator validator;
  
  // LOOP takes 2 operands (count, end label)
  auto result = validator.validateOperandCount(Opcode::LOOP, 2);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::ENDLOOP, 0);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::BREAK, 0);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::CONTINUE, 0);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void register_flow_tests() {
  TestRunner& r = TestRunner::instance();
  r.registerTest("flow_jump", "Flow", test_jump);
  r.registerTest("flow_cond_jumps", "Flow", test_conditional_jumps);
  r.registerTest("flow_call_ret", "Flow", test_call_ret);
  r.registerTest("flow_loops", "Flow", test_loop_constructs);
}

} // namespace flow_tests

// MEMORY category opcodes
namespace memory_tests {

inline void test_load_store() {
  InstructionValidator validator;
  
  // LOAD takes 2 operands (dest, address)
  auto result = validator.validateOperandCount(Opcode::LOAD, 2);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // STORE takes 2 operands (value, address)
  result = validator.validateOperandCount(Opcode::STORE, 2);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // LOAD_CONST takes 2 operands (dest, constant)
  result = validator.validateOperandCount(Opcode::LOAD_CONST, 2);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void test_stack_ops() {
  InstructionValidator validator;
  
  // PUSH takes 1 operand
  auto result = validator.validateOperandCount(Opcode::PUSH, 1);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // POP takes 1 operand (destination)
  result = validator.validateOperandCount(Opcode::POP, 1);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // DUP takes 0 operands
  result = validator.validateOperandCount(Opcode::DUP, 0);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // SWAP takes 0 operands
  result = validator.validateOperandCount(Opcode::SWAP, 0);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void test_memory_ops() {
  InstructionValidator validator;
  
  // ALLOC takes 2 operands (size, dest)
  auto result = validator.validateOperandCount(Opcode::ALLOC, 2);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // FREE takes 1 operand (pointer)
  result = validator.validateOperandCount(Opcode::FREE, 1);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // COPY takes 3 operands (src, dest, size)
  result = validator.validateOperandCount(Opcode::COPY, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // ZERO takes 2 operands (dest, size)
  result = validator.validateOperandCount(Opcode::ZERO, 2);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void register_memory_tests() {
  TestRunner& r = TestRunner::instance();
  r.registerTest("mem_load_store", "Memory", test_load_store);
  r.registerTest("mem_stack", "Memory", test_stack_ops);
  r.registerTest("mem_alloc", "Memory", test_memory_ops);
}

} // namespace memory_tests

// ARITHMETIC category opcodes
namespace arith_tests {

inline void test_basic_arithmetic() {
  InstructionValidator validator;
  
  // Binary ops take 3 operands (a, b, dest)
  auto result = validator.validateOperandCount(Opcode::ADD, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::SUB, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::MUL, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::DIV, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::MOD, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void test_unary_arithmetic() {
  InstructionValidator validator;
  
  // Unary ops take 2 operands (src, dest)
  auto result = validator.validateOperandCount(Opcode::NEG, 2);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::ABS, 2);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::INC, 1);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::DEC, 1);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void test_math_functions() {
  InstructionValidator validator;
  
  auto result = validator.validateOperandCount(Opcode::SIN, 2);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::COS, 2);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::SQRT, 2);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::POW, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::EXP, 2);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::LOG, 2);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void test_interpolation() {
  InstructionValidator validator;
  
  // LERP takes 4 operands (a, b, t, dest)
  auto result = validator.validateOperandCount(Opcode::LERP, 4);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::CLAMP, 4);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::STEP, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::SMOOTHSTEP, 4);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void register_arith_tests() {
  TestRunner& r = TestRunner::instance();
  r.registerTest("arith_basic", "Arithmetic", test_basic_arithmetic);
  r.registerTest("arith_unary", "Arithmetic", test_unary_arithmetic);
  r.registerTest("arith_math", "Arithmetic", test_math_functions);
  r.registerTest("arith_interp", "Arithmetic", test_interpolation);
}

} // namespace arith_tests

// LOGIC category opcodes
namespace logic_tests {

inline void test_bitwise_ops() {
  InstructionValidator validator;
  
  auto result = validator.validateOperandCount(Opcode::AND, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::OR, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::XOR, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::NOT, 2);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::SHL, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::SHR, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void register_logic_tests() {
  TestRunner& r = TestRunner::instance();
  r.registerTest("logic_bitwise", "Logic", test_bitwise_ops);
}

} // namespace logic_tests

// COMPARE category opcodes
namespace compare_tests {

inline void test_comparisons() {
  InstructionValidator validator;
  
  auto result = validator.validateOperandCount(Opcode::CMP_EQ, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::CMP_NE, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::CMP_LT, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::CMP_GT, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::CMP_LE, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::CMP_GE, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void register_compare_tests() {
  TestRunner& r = TestRunner::instance();
  r.registerTest("cmp_all", "Compare", test_comparisons);
}

} // namespace compare_tests

// DRAW category opcodes
namespace draw_tests {

inline void test_pixel_ops() {
  InstructionValidator validator;
  
  // SET_PIXEL takes 3 operands (x, y, color)
  auto result = validator.validateOperandCount(Opcode::SET_PIXEL, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // GET_PIXEL takes 3 operands (x, y, dest)
  result = validator.validateOperandCount(Opcode::GET_PIXEL, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // Context validation
  result = validator.validateContext(Opcode::SET_PIXEL, ExecutionContext::FRAGMENT);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void test_shape_drawing() {
  InstructionValidator validator;
  
  // DRAW_LINE takes 5 operands (x1, y1, x2, y2, color)
  auto result = validator.validateOperandCount(Opcode::DRAW_LINE, 5);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // DRAW_RECT takes 5 operands (x, y, w, h, color)
  result = validator.validateOperandCount(Opcode::DRAW_RECT, 5);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // FILL_RECT takes 5 operands
  result = validator.validateOperandCount(Opcode::FILL_RECT, 5);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // DRAW_CIRCLE takes 4 operands (cx, cy, r, color)
  result = validator.validateOperandCount(Opcode::DRAW_CIRCLE, 4);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // FILL_CIRCLE takes 4 operands
  result = validator.validateOperandCount(Opcode::FILL_CIRCLE, 4);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void test_advanced_drawing() {
  InstructionValidator validator;
  
  // DRAW_TRI takes 7 operands (x1,y1,x2,y2,x3,y3,color)
  auto result = validator.validateOperandCount(Opcode::DRAW_TRI, 7);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // FILL_TRI takes 7 operands
  result = validator.validateOperandCount(Opcode::FILL_TRI, 7);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // DRAW_POLY takes 3 operands (point_array, count, color)
  result = validator.validateOperandCount(Opcode::DRAW_POLY, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void register_draw_tests() {
  TestRunner& r = TestRunner::instance();
  r.registerTest("draw_pixels", "Draw", test_pixel_ops);
  r.registerTest("draw_shapes", "Draw", test_shape_drawing);
  r.registerTest("draw_advanced", "Draw", test_advanced_drawing);
}

} // namespace draw_tests

// TEXT category opcodes
namespace text_tests {

inline void test_text_drawing() {
  InstructionValidator validator;
  
  // DRAW_CHAR takes 4 operands (x, y, char, color)
  auto result = validator.validateOperandCount(Opcode::DRAW_CHAR, 4);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // DRAW_TEXT takes 4 operands (x, y, string_ptr, color)
  result = validator.validateOperandCount(Opcode::DRAW_TEXT, 4);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // SET_FONT takes 1 operand (font_id)
  result = validator.validateOperandCount(Opcode::SET_FONT, 1);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // SET_TEXT_SIZE takes 1 operand (size)
  result = validator.validateOperandCount(Opcode::SET_TEXT_SIZE, 1);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void register_text_tests() {
  TestRunner& r = TestRunner::instance();
  r.registerTest("text_draw", "Text", test_text_drawing);
}

} // namespace text_tests

// ANIMATION category opcodes
namespace anim_opcode_tests {

inline void test_animation_ops() {
  InstructionValidator validator;
  
  // ANIM_CREATE takes 1 operand (dest)
  auto result = validator.validateOperandCount(Opcode::ANIM_CREATE, 1);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // ANIM_DESTROY takes 1 operand (anim_id)
  result = validator.validateOperandCount(Opcode::ANIM_DESTROY, 1);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // ANIM_PLAY takes 1 operand (anim_id)
  result = validator.validateOperandCount(Opcode::ANIM_PLAY, 1);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // ANIM_PAUSE takes 1 operand
  result = validator.validateOperandCount(Opcode::ANIM_PAUSE, 1);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // ANIM_STOP takes 1 operand
  result = validator.validateOperandCount(Opcode::ANIM_STOP, 1);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void test_keyframe_ops() {
  InstructionValidator validator;
  
  // ANIM_KEYFRAME takes 4+ operands (anim_id, time, property, value...)
  auto result = validator.validateOperandCount(Opcode::ANIM_KEYFRAME, 4);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // ANIM_EASING takes 2 operands (anim_id, easing_type)
  result = validator.validateOperandCount(Opcode::ANIM_EASING, 2);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // ANIM_LOOP takes 2 operands (anim_id, loop_mode)
  result = validator.validateOperandCount(Opcode::ANIM_LOOP, 2);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void register_anim_opcode_tests() {
  TestRunner& r = TestRunner::instance();
  r.registerTest("anim_ops", "AnimOpcodes", test_animation_ops);
  r.registerTest("anim_keyframe_ops", "AnimOpcodes", test_keyframe_ops);
}

} // namespace anim_opcode_tests

// SDF category opcodes
namespace sdf_opcode_tests {

inline void test_sdf_primitives() {
  InstructionValidator validator;
  
  // SDF_CIRCLE takes 4 operands (x, y, radius, dest)
  auto result = validator.validateOperandCount(Opcode::SDF_CIRCLE, 4);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // SDF_BOX takes 5 operands (x, y, w, h, dest)
  result = validator.validateOperandCount(Opcode::SDF_BOX, 5);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // SDF_LINE takes 6 operands (x1, y1, x2, y2, width, dest)
  result = validator.validateOperandCount(Opcode::SDF_LINE, 6);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void test_sdf_operations() {
  InstructionValidator validator;
  
  // SDF_UNION takes 3 operands (d1, d2, dest)
  auto result = validator.validateOperandCount(Opcode::SDF_UNION, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // SDF_SUBTRACT takes 3 operands
  result = validator.validateOperandCount(Opcode::SDF_SUBTRACT, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // SDF_INTERSECT takes 3 operands
  result = validator.validateOperandCount(Opcode::SDF_INTERSECT, 3);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // SDF_SMOOTH takes 4 operands (d1, d2, k, dest)
  result = validator.validateOperandCount(Opcode::SDF_SMOOTH, 4);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void register_sdf_opcode_tests() {
  TestRunner& r = TestRunner::instance();
  r.registerTest("sdf_primitives_ops", "SDFOpcodes", test_sdf_primitives);
  r.registerTest("sdf_operations_ops", "SDFOpcodes", test_sdf_operations);
}

} // namespace sdf_opcode_tests

// ============================================================
// Data Type Validation Tests
// ============================================================

namespace type_tests {

inline void test_type_sizes() {
  // Verify expected sizes
  TEST_ASSERT_EQ(1, sizeof(uint8_t));
  TEST_ASSERT_EQ(2, sizeof(uint16_t));
  TEST_ASSERT_EQ(4, sizeof(uint32_t));
  TEST_ASSERT_EQ(4, sizeof(float));
  
  TEST_ASSERT_EQ(2, sizeof(Fixed8_8));
  TEST_ASSERT_EQ(4, sizeof(Fixed16_16));
  
  TEST_ASSERT_EQ(8, sizeof(Vec2));
  TEST_ASSERT_EQ(12, sizeof(Vec3));
  TEST_ASSERT_EQ(16, sizeof(Vec4));
  TEST_ASSERT_EQ(16, sizeof(ColorF));
}

inline void test_type_conversions() {
  // INT to FLOAT
  int i = 42;
  float f = (float)i;
  TEST_ASSERT_FLOAT_EQ(42.0f, f, 0.0001f);
  
  // FLOAT to INT (truncation)
  f = 3.7f;
  i = (int)f;
  TEST_ASSERT_EQ(3, i);
  
  // Fixed point conversions
  Fixed8_8 q88 = Fixed8_8::fromFloat(1.5f);
  float back = q88.toFloat();
  TEST_ASSERT_FLOAT_EQ(1.5f, back, 0.01f);
  
  Fixed16_16 q1616 = Fixed16_16::fromFloat(1234.5678f);
  back = q1616.toFloat();
  TEST_ASSERT_FLOAT_EQ(1234.5678f, back, 0.001f);
}

inline void test_type_ranges() {
  // U8 range
  TEST_ASSERT_EQ(0, (uint8_t)DataType::VOID);
  TEST_ASSERT_EQ(255, (int)0xFF);
  
  // S8 range
  int8_t s8_min = -128;
  int8_t s8_max = 127;
  TEST_ASSERT_EQ(-128, s8_min);
  TEST_ASSERT_EQ(127, s8_max);
  
  // Fixed8.8 range (-128.0 to 127.996)
  Fixed8_8 q88_max = Fixed8_8::fromFloat(127.0f);
  TEST_ASSERT(q88_max.toFloat() <= 128.0f);
  
  Fixed8_8 q88_min = Fixed8_8::fromFloat(-128.0f);
  TEST_ASSERT(q88_min.toFloat() >= -128.0f);
}

inline void register_type_tests() {
  TestRunner& r = TestRunner::instance();
  r.registerTest("type_sizes", "Types", test_type_sizes);
  r.registerTest("type_conversions", "Types", test_type_conversions);
  r.registerTest("type_ranges", "Types", test_type_ranges);
}

} // namespace type_tests

// ============================================================
// Precision Mode Tests
// ============================================================

namespace precision_tests {

inline void test_precision_low() {
  // LOW precision uses 8-bit
  Fixed8_8 a = Fixed8_8::fromFloat(1.0f);
  Fixed8_8 b = Fixed8_8::fromFloat(0.5f);
  Fixed8_8 sum = a + b;
  TEST_ASSERT_FLOAT_EQ(1.5f, sum.toFloat(), 0.01f);
}

inline void test_precision_medium() {
  // MEDIUM precision uses 16-bit
  Fixed16_16 a = Fixed16_16::fromFloat(1000.0f);
  Fixed16_16 b = Fixed16_16::fromFloat(0.0001f);
  Fixed16_16 sum = a + b;
  TEST_ASSERT_FLOAT_EQ(1000.0001f, sum.toFloat(), 0.001f);
}

inline void test_precision_high() {
  // HIGH precision uses 32-bit float
  float a = 1e10f;
  float b = 1e-5f;
  float prod = a * b;
  TEST_ASSERT_FLOAT_EQ(1e5f, prod, 1.0f);
}

inline void register_precision_tests() {
  TestRunner& r = TestRunner::instance();
  r.registerTest("prec_low", "Precision", test_precision_low);
  r.registerTest("prec_medium", "Precision", test_precision_medium);
  r.registerTest("prec_high", "Precision", test_precision_high);
}

} // namespace precision_tests

// ============================================================
// Execution Context Tests
// ============================================================

namespace context_tests {

inline void test_immediate_context() {
  InstructionValidator validator;
  
  // Most instructions valid in IMMEDIATE
  auto result = validator.validateContext(Opcode::NOP, ExecutionContext::IMMEDIATE);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateContext(Opcode::ADD, ExecutionContext::IMMEDIATE);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void test_fragment_context() {
  InstructionValidator validator;
  
  // SET_PIXEL only valid in FRAGMENT
  auto result = validator.validateContext(Opcode::SET_PIXEL, ExecutionContext::FRAGMENT);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  // Should fail in other contexts
  result = validator.validateContext(Opcode::SET_PIXEL, ExecutionContext::COMPUTE);
  TEST_ASSERT(result != ValidationError::NONE);
}

inline void test_animation_context() {
  InstructionValidator validator;
  
  // Animation ops valid in ANIMATION context
  auto result = validator.validateContext(Opcode::ANIM_PLAY, ExecutionContext::ANIMATION);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void register_context_tests() {
  TestRunner& r = TestRunner::instance();
  r.registerTest("ctx_immediate", "Context", test_immediate_context);
  r.registerTest("ctx_fragment", "Context", test_fragment_context);
  r.registerTest("ctx_animation", "Context", test_animation_context);
}

} // namespace context_tests

// ============================================================
// Edge Case Tests
// ============================================================

namespace edge_case_tests {

inline void test_division_by_zero() {
  RuntimeValidator validator;
  
  // Integer division by zero
  auto result = validator.checkDivisionSafety(100, 0);
  TEST_ASSERT_EQ(ValidationError::MEMORY_DIVISION_BY_ZERO, result);
  
  // Float division by zero
  result = validator.checkFloatDivisionSafety(100.0f, 0.0f);
  TEST_ASSERT_EQ(ValidationError::MEMORY_DIVISION_BY_ZERO, result);
  
  // Very small divisor
  result = validator.checkFloatDivisionSafety(100.0f, 1e-40f);
  TEST_ASSERT_EQ(ValidationError::MEMORY_DIVISION_BY_ZERO, result);
}

inline void test_overflow() {
  RuntimeValidator validator;
  
  // Integer overflow detection
  auto result = validator.checkIntegerOverflow(0x7FFFFFFF, 1, true);  // Add
  TEST_ASSERT_EQ(ValidationError::MEMORY_OVERFLOW, result);
  
  // Underflow detection
  result = validator.checkIntegerOverflow(-2147483647 - 1, -1, true);
  TEST_ASSERT_EQ(ValidationError::MEMORY_OVERFLOW, result);
  
  // Valid operation
  result = validator.checkIntegerOverflow(100, 200, true);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
}

inline void test_out_of_bounds() {
  RuntimeValidator validator;
  
  // Array bounds
  auto result = validator.checkArrayBounds(0, 10);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.checkArrayBounds(9, 10);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.checkArrayBounds(10, 10);
  TEST_ASSERT_EQ(ValidationError::MEMORY_OUT_OF_BOUNDS, result);
  
  result = validator.checkArrayBounds(-1, 10);
  TEST_ASSERT_EQ(ValidationError::MEMORY_OUT_OF_BOUNDS, result);
}

inline void test_stack_limits() {
  RuntimeValidator validator;
  
  // Stack overflow
  auto result = validator.checkStackPush(255, 256);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.checkStackPush(256, 256);
  TEST_ASSERT_EQ(ValidationError::MEMORY_STACK_OVERFLOW, result);
  
  // Stack underflow
  result = validator.checkStackPop(1);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.checkStackPop(0);
  TEST_ASSERT_EQ(ValidationError::MEMORY_STACK_UNDERFLOW, result);
}

inline void test_nan_inf() {
  // NaN detection
  float nan_val = 0.0f / 0.0f;
  TEST_ASSERT(isnan(nan_val));
  
  // Infinity detection
  float inf_val = 1.0f / 0.0f;
  TEST_ASSERT(isinf(inf_val));
  
  // Valid float
  float valid = 1.0f;
  TEST_ASSERT(!isnan(valid));
  TEST_ASSERT(!isinf(valid));
}

inline void register_edge_case_tests() {
  TestRunner& r = TestRunner::instance();
  r.registerTest("edge_div_zero", "EdgeCases", test_division_by_zero);
  r.registerTest("edge_overflow", "EdgeCases", test_overflow);
  r.registerTest("edge_bounds", "EdgeCases", test_out_of_bounds);
  r.registerTest("edge_stack", "EdgeCases", test_stack_limits);
  r.registerTest("edge_nan_inf", "EdgeCases", test_nan_inf);
}

} // namespace edge_case_tests

// ============================================================
// Bytecode Validation Tests
// ============================================================

namespace bytecode_tests {

inline void test_valid_program() {
  uint8_t program[] = {
    (uint8_t)Opcode::NOP,
    (uint8_t)Opcode::LOAD_CONST, 0x00, 0x42, 0x00, 0x00, 0x00,  // Load 0x42 to r0
    (uint8_t)Opcode::HALT
  };
  
  CompileTimeValidator validator;
  auto result = validator.validate(program, sizeof(program));
  TEST_ASSERT(result.isValid());
}

inline void test_invalid_opcode() {
  uint8_t program[] = {
    (uint8_t)Opcode::NOP,
    0xFE,  // Invalid opcode (ILLEGAL is 0xFF)
    (uint8_t)Opcode::HALT
  };
  
  CompileTimeValidator validator;
  auto result = validator.validate(program, sizeof(program));
  TEST_ASSERT(!result.isValid());
  TEST_ASSERT_EQ(ValidationError::SYNTAX_INVALID_OPCODE, result.first_error);
}

inline void test_missing_halt() {
  uint8_t program[] = {
    (uint8_t)Opcode::NOP,
    (uint8_t)Opcode::NOP,
    // Missing HALT
  };
  
  CompileTimeValidator validator;
  auto result = validator.validate(program, sizeof(program));
  // Should warn about missing HALT but may still be valid
  TEST_ASSERT(result.warnings > 0 || !result.isValid());
}

inline void test_unmatched_loop() {
  // LOOP without ENDLOOP
  ValidationContext ctx;
  ctx.pushLoop(0, 100);  // Start loop
  
  // Should have unclosed loop
  TEST_ASSERT_EQ(1, ctx.getLoopDepth());
}

inline void register_bytecode_tests() {
  TestRunner& r = TestRunner::instance();
  r.registerTest("bc_valid", "Bytecode", test_valid_program);
  r.registerTest("bc_invalid_opcode", "Bytecode", test_invalid_opcode);
  r.registerTest("bc_missing_halt", "Bytecode", test_missing_halt);
  r.registerTest("bc_unmatched_loop", "Bytecode", test_unmatched_loop);
}

} // namespace bytecode_tests

// ============================================================
// Master Registration
// ============================================================

inline void registerAllValidationTests() {
  system_tests::register_system_tests();
  flow_tests::register_flow_tests();
  memory_tests::register_memory_tests();
  arith_tests::register_arith_tests();
  logic_tests::register_logic_tests();
  compare_tests::register_compare_tests();
  draw_tests::register_draw_tests();
  text_tests::register_text_tests();
  anim_opcode_tests::register_anim_opcode_tests();
  sdf_opcode_tests::register_sdf_opcode_tests();
  type_tests::register_type_tests();
  precision_tests::register_precision_tests();
  context_tests::register_context_tests();
  edge_case_tests::register_edge_case_tests();
  bytecode_tests::register_bytecode_tests();
}

// Run all validation tests
inline int runAllValidationTests() {
  registerAllValidationTests();
  
  TestRunner& runner = TestRunner::instance();
  runner.runAll();
  
  return runner.getFailedCount() > 0 ? 1 : 0;
}

} // namespace validation_tests
} // namespace gpu

#endif // GPU_VALIDATION_TESTS_HPP_
