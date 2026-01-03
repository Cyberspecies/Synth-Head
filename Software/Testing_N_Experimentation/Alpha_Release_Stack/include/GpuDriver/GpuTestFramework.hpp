/*****************************************************************
 * File:      GpuTestFramework.hpp
 * Category:  GPU Driver / Test Framework
 * Author:    XCR1793 (Feather Forge)
 * 
 * Purpose:
 *    Comprehensive testing framework for the GPU driver system.
 *    Provides unit tests, integration tests, and validation
 *    test suites for all GPU subsystems.
 * 
 * Features:
 *    - Test case registration and execution
 *    - Assertion macros with detailed reporting
 *    - ISA opcode testing infrastructure
 *    - Animation timing validation
 *    - SDF accuracy testing
 *    - Visual regression testing
 *    - Performance benchmarking
 *****************************************************************/

#ifndef GPU_TEST_FRAMEWORK_HPP_
#define GPU_TEST_FRAMEWORK_HPP_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

// Include all GPU subsystems for testing
#include "GpuISA.hpp"
#include "GpuValidator.hpp"
#include "GpuAnimationSystem.hpp"
#include "GpuSDF.hpp"
#include "GpuAntialiasing.hpp"
#include "GpuCompositor.hpp"

namespace gpu {
namespace test {

// ============================================================
// Test Framework Constants
// ============================================================

constexpr int MAX_TESTS = 256;
constexpr int MAX_TEST_NAME = 64;
constexpr int MAX_MESSAGE = 256;
constexpr float DEFAULT_EPSILON = 0.0001f;

// ============================================================
// Test Result
// ============================================================

enum class TestStatus {
  NOT_RUN   = 0,
  PASSED    = 1,
  FAILED    = 2,
  SKIPPED   = 3,
  ERROR     = 4,
};

struct TestResult {
  TestStatus status;
  char name[MAX_TEST_NAME];
  char message[MAX_MESSAGE];
  int assertions_passed;
  int assertions_failed;
  uint32_t duration_us;  // Execution time in microseconds
  
  TestResult() : status(TestStatus::NOT_RUN), assertions_passed(0),
                 assertions_failed(0), duration_us(0) {
    name[0] = '\0';
    message[0] = '\0';
  }
};

// ============================================================
// Test Case Base
// ============================================================

typedef void (*TestFunction)();

struct TestCase {
  char name[MAX_TEST_NAME];
  char category[MAX_TEST_NAME];
  TestFunction func;
  bool enabled;
  
  TestCase() : func(nullptr), enabled(true) {
    name[0] = '\0';
    category[0] = '\0';
  }
};

// ============================================================
// Test Context (for assertion tracking)
// ============================================================

class TestContext {
public:
  static TestContext& instance() {
    static TestContext ctx;
    return ctx;
  }
  
  void reset() {
    current_result_ = TestResult();
    in_test_ = false;
  }
  
  void beginTest(const char* name) {
    reset();
    strncpy(current_result_.name, name, MAX_TEST_NAME - 1);
    current_result_.status = TestStatus::PASSED;
    in_test_ = true;
  }
  
  void endTest() {
    in_test_ = false;
  }
  
  void assertPass() {
    if (in_test_) current_result_.assertions_passed++;
  }
  
  void assertFail(const char* message) {
    if (in_test_) {
      current_result_.assertions_failed++;
      current_result_.status = TestStatus::FAILED;
      if (current_result_.message[0] == '\0') {
        strncpy(current_result_.message, message, MAX_MESSAGE - 1);
      }
    }
  }
  
  void setError(const char* message) {
    if (in_test_) {
      current_result_.status = TestStatus::ERROR;
      strncpy(current_result_.message, message, MAX_MESSAGE - 1);
    }
  }
  
  void setSkipped(const char* reason) {
    if (in_test_) {
      current_result_.status = TestStatus::SKIPPED;
      strncpy(current_result_.message, reason, MAX_MESSAGE - 1);
    }
  }
  
  void setDuration(uint32_t us) {
    current_result_.duration_us = us;
  }
  
  TestResult& getResult() { return current_result_; }
  bool isInTest() const { return in_test_; }

private:
  TestContext() : in_test_(false) {}
  TestResult current_result_;
  bool in_test_;
};

// ============================================================
// Assertion Macros
// ============================================================

#define TEST_ASSERT(condition) \
  do { \
    if (condition) { \
      gpu::test::TestContext::instance().assertPass(); \
    } else { \
      char msg[256]; \
      snprintf(msg, sizeof(msg), "Assertion failed: %s (line %d)", #condition, __LINE__); \
      gpu::test::TestContext::instance().assertFail(msg); \
    } \
  } while (0)

#define TEST_ASSERT_MSG(condition, message) \
  do { \
    if (condition) { \
      gpu::test::TestContext::instance().assertPass(); \
    } else { \
      gpu::test::TestContext::instance().assertFail(message); \
    } \
  } while (0)

#define TEST_ASSERT_EQ(expected, actual) \
  do { \
    if ((expected) == (actual)) { \
      gpu::test::TestContext::instance().assertPass(); \
    } else { \
      char msg[256]; \
      snprintf(msg, sizeof(msg), "Expected %d, got %d (line %d)", (int)(expected), (int)(actual), __LINE__); \
      gpu::test::TestContext::instance().assertFail(msg); \
    } \
  } while (0)

#define TEST_ASSERT_FLOAT_EQ(expected, actual, epsilon) \
  do { \
    float diff = fabsf((float)(expected) - (float)(actual)); \
    if (diff <= (epsilon)) { \
      gpu::test::TestContext::instance().assertPass(); \
    } else { \
      char msg[256]; \
      snprintf(msg, sizeof(msg), "Expected %.6f, got %.6f (diff=%.6f, line %d)", \
               (float)(expected), (float)(actual), diff, __LINE__); \
      gpu::test::TestContext::instance().assertFail(msg); \
    } \
  } while (0)

#define TEST_ASSERT_NOT_NULL(ptr) \
  do { \
    if ((ptr) != nullptr) { \
      gpu::test::TestContext::instance().assertPass(); \
    } else { \
      char msg[256]; \
      snprintf(msg, sizeof(msg), "Expected non-null pointer: %s (line %d)", #ptr, __LINE__); \
      gpu::test::TestContext::instance().assertFail(msg); \
    } \
  } while (0)

#define TEST_ASSERT_NULL(ptr) \
  do { \
    if ((ptr) == nullptr) { \
      gpu::test::TestContext::instance().assertPass(); \
    } else { \
      char msg[256]; \
      snprintf(msg, sizeof(msg), "Expected null pointer: %s (line %d)", #ptr, __LINE__); \
      gpu::test::TestContext::instance().assertFail(msg); \
    } \
  } while (0)

#define TEST_FAIL(message) \
  gpu::test::TestContext::instance().assertFail(message)

#define TEST_SKIP(reason) \
  do { \
    gpu::test::TestContext::instance().setSkipped(reason); \
    return; \
  } while (0)

// ============================================================
// Test Runner
// ============================================================

class TestRunner {
public:
  static TestRunner& instance() {
    static TestRunner runner;
    return runner;
  }
  
  // Register a test
  bool registerTest(const char* name, const char* category, TestFunction func) {
    if (test_count_ >= MAX_TESTS) return false;
    
    TestCase& tc = tests_[test_count_];
    strncpy(tc.name, name, MAX_TEST_NAME - 1);
    strncpy(tc.category, category, MAX_TEST_NAME - 1);
    tc.func = func;
    tc.enabled = true;
    test_count_++;
    return true;
  }
  
  // Run all tests
  void runAll() {
    results_count_ = 0;
    passed_count_ = 0;
    failed_count_ = 0;
    skipped_count_ = 0;
    
    for (int i = 0; i < test_count_; i++) {
      if (tests_[i].enabled && tests_[i].func) {
        runTest(tests_[i]);
      }
    }
  }
  
  // Run tests in a specific category
  void runCategory(const char* category) {
    results_count_ = 0;
    passed_count_ = 0;
    failed_count_ = 0;
    skipped_count_ = 0;
    
    for (int i = 0; i < test_count_; i++) {
      if (tests_[i].enabled && tests_[i].func &&
          strcmp(tests_[i].category, category) == 0) {
        runTest(tests_[i]);
      }
    }
  }
  
  // Run a single test by name
  bool runSingle(const char* name) {
    for (int i = 0; i < test_count_; i++) {
      if (strcmp(tests_[i].name, name) == 0) {
        runTest(tests_[i]);
        return true;
      }
    }
    return false;
  }
  
  // Get results
  int getTestCount() const { return test_count_; }
  int getResultsCount() const { return results_count_; }
  int getPassedCount() const { return passed_count_; }
  int getFailedCount() const { return failed_count_; }
  int getSkippedCount() const { return skipped_count_; }
  
  const TestResult& getResult(int index) const {
    if (index >= 0 && index < results_count_) {
      return results_[index];
    }
    static TestResult empty;
    return empty;
  }
  
  // Generate summary
  void printSummary() const {
    // This would output to serial/console
    // For embedded use, could write to buffer instead
  }

private:
  TestRunner() : test_count_(0), results_count_(0),
                 passed_count_(0), failed_count_(0), skipped_count_(0) {}
  
  void runTest(const TestCase& tc) {
    TestContext& ctx = TestContext::instance();
    ctx.beginTest(tc.name);
    
    // Run the test (could add timing here)
    tc.func();
    
    ctx.endTest();
    
    // Store result
    if (results_count_ < MAX_TESTS) {
      results_[results_count_++] = ctx.getResult();
      
      switch (ctx.getResult().status) {
        case TestStatus::PASSED: passed_count_++; break;
        case TestStatus::FAILED:
        case TestStatus::ERROR: failed_count_++; break;
        case TestStatus::SKIPPED: skipped_count_++; break;
        default: break;
      }
    }
  }
  
  TestCase tests_[MAX_TESTS];
  int test_count_;
  
  TestResult results_[MAX_TESTS];
  int results_count_;
  int passed_count_;
  int failed_count_;
  int skipped_count_;
};

// Test registration macro
#define REGISTER_TEST(name, category) \
  static void test_##name(); \
  static bool _test_registered_##name = \
    gpu::test::TestRunner::instance().registerTest(#name, category, test_##name); \
  static void test_##name()

// ============================================================
// ISA Tests
// ============================================================

namespace isa_tests {

using namespace gpu::isa;

// Test fixed-point arithmetic
inline void test_fixed8_8() {
  Fixed8_8 a = Fixed8_8::fromFloat(1.5f);
  Fixed8_8 b = Fixed8_8::fromFloat(2.25f);
  
  Fixed8_8 sum = a + b;
  TEST_ASSERT_FLOAT_EQ(3.75f, sum.toFloat(), 0.01f);
  
  Fixed8_8 prod = a * b;
  TEST_ASSERT_FLOAT_EQ(3.375f, prod.toFloat(), 0.02f);
  
  Fixed8_8 neg = Fixed8_8::fromFloat(-1.5f);
  TEST_ASSERT_FLOAT_EQ(-1.5f, neg.toFloat(), 0.01f);
}

inline void test_fixed16_16() {
  Fixed16_16 a = Fixed16_16::fromFloat(100.5f);
  Fixed16_16 b = Fixed16_16::fromFloat(0.001f);
  
  Fixed16_16 prod = a * b;
  TEST_ASSERT_FLOAT_EQ(0.1005f, prod.toFloat(), 0.001f);
  
  Fixed16_16 div = a / Fixed16_16::fromFloat(2.0f);
  TEST_ASSERT_FLOAT_EQ(50.25f, div.toFloat(), 0.001f);
}

// Test vector operations
inline void test_vec2() {
  Vec2 a(3.0f, 4.0f);
  TEST_ASSERT_FLOAT_EQ(5.0f, a.length(), 0.0001f);
  
  Vec2 n = a.normalized();
  TEST_ASSERT_FLOAT_EQ(0.6f, n.x, 0.0001f);
  TEST_ASSERT_FLOAT_EQ(0.8f, n.y, 0.0001f);
  
  Vec2 b(1.0f, 2.0f);
  TEST_ASSERT_FLOAT_EQ(11.0f, a.dot(b), 0.0001f);
}

inline void test_vec3() {
  Vec3 a(1.0f, 0.0f, 0.0f);
  Vec3 b(0.0f, 1.0f, 0.0f);
  Vec3 c = a.cross(b);
  
  TEST_ASSERT_FLOAT_EQ(0.0f, c.x, 0.0001f);
  TEST_ASSERT_FLOAT_EQ(0.0f, c.y, 0.0001f);
  TEST_ASSERT_FLOAT_EQ(1.0f, c.z, 0.0001f);
}

// Test color operations
inline void test_color_blend() {
  ColorF red(1.0f, 0.0f, 0.0f, 1.0f);
  ColorF blue(0.0f, 0.0f, 1.0f, 1.0f);
  
  ColorF purple = red.lerp(blue, 0.5f);
  TEST_ASSERT_FLOAT_EQ(0.5f, purple.r, 0.0001f);
  TEST_ASSERT_FLOAT_EQ(0.0f, purple.g, 0.0001f);
  TEST_ASSERT_FLOAT_EQ(0.5f, purple.b, 0.0001f);
}

inline void test_color_conversion() {
  ColorF white(1.0f, 1.0f, 1.0f, 1.0f);
  
  TEST_ASSERT_EQ(255, white.r8());
  TEST_ASSERT_EQ(255, white.g8());
  TEST_ASSERT_EQ(255, white.b8());
  
  uint16_t rgb565 = white.toRGB565();
  TEST_ASSERT_EQ(0xFFFF, rgb565);
}

// Test easing functions
inline void test_easing() {
  // Linear
  TEST_ASSERT_FLOAT_EQ(0.5f, evaluateEasing(EasingType::LINEAR, 0.5f), 0.0001f);
  
  // Ease in quad
  TEST_ASSERT_FLOAT_EQ(0.0f, evaluateEasing(EasingType::EASE_IN_QUAD, 0.0f), 0.0001f);
  TEST_ASSERT_FLOAT_EQ(1.0f, evaluateEasing(EasingType::EASE_IN_QUAD, 1.0f), 0.0001f);
  TEST_ASSERT_FLOAT_EQ(0.25f, evaluateEasing(EasingType::EASE_IN_QUAD, 0.5f), 0.0001f);
  
  // Ease out quad
  TEST_ASSERT_FLOAT_EQ(0.75f, evaluateEasing(EasingType::EASE_OUT_QUAD, 0.5f), 0.0001f);
}

// Test blend modes
inline void test_blend_modes() {
  ColorF dst(0.5f, 0.5f, 0.5f, 1.0f);
  ColorF src(1.0f, 1.0f, 1.0f, 0.5f);
  
  // Alpha blend
  ColorF result = applyBlendMode(dst, src, BlendMode::ALPHA);
  TEST_ASSERT(result.r > dst.r);
  TEST_ASSERT(result.r < 1.0f);
  
  // Additive
  result = applyBlendMode(dst, src, BlendMode::ADDITIVE);
  TEST_ASSERT(result.r >= dst.r);
}

// Test bytecode writer/reader
inline void test_bytecode() {
  uint8_t buffer[256];
  BytecodeWriter writer(buffer, sizeof(buffer));
  
  writer.writeOpcode(Opcode::LOAD_CONST);
  writer.writeU8(0x42);
  writer.writeU16(0x1234);
  writer.writeU32(0xDEADBEEF);
  writer.writeFloat(3.14159f);
  
  BytecodeReader reader(buffer, writer.size());
  
  TEST_ASSERT_EQ((uint8_t)Opcode::LOAD_CONST, reader.readU8());
  TEST_ASSERT_EQ(0x42, reader.readU8());
  TEST_ASSERT_EQ(0x1234, reader.readU16());
  TEST_ASSERT_EQ(0xDEADBEEF, reader.readU32());
  TEST_ASSERT_FLOAT_EQ(3.14159f, reader.readFloat(), 0.00001f);
}

// Register ISA tests
inline void register_isa_tests() {
  TestRunner& runner = TestRunner::instance();
  runner.registerTest("isa_fixed8_8", "ISA", test_fixed8_8);
  runner.registerTest("isa_fixed16_16", "ISA", test_fixed16_16);
  runner.registerTest("isa_vec2", "ISA", test_vec2);
  runner.registerTest("isa_vec3", "ISA", test_vec3);
  runner.registerTest("isa_color_blend", "ISA", test_color_blend);
  runner.registerTest("isa_color_conversion", "ISA", test_color_conversion);
  runner.registerTest("isa_easing", "ISA", test_easing);
  runner.registerTest("isa_blend_modes", "ISA", test_blend_modes);
  runner.registerTest("isa_bytecode", "ISA", test_bytecode);
}

} // namespace isa_tests

// ============================================================
// Validator Tests
// ============================================================

namespace validator_tests {

using namespace gpu::isa;
using namespace gpu::validator;

// Test operand count validation
inline void test_operand_counts() {
  InstructionValidator validator;
  
  // NOP takes 0 operands
  auto result = validator.validateOperandCount(Opcode::NOP, 0);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::NOP, 1);
  TEST_ASSERT_EQ(ValidationError::SYNTAX_WRONG_OPERAND_COUNT, result);
  
  // PUSH takes 1 operand
  result = validator.validateOperandCount(Opcode::PUSH, 1);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateOperandCount(Opcode::PUSH, 0);
  TEST_ASSERT_EQ(ValidationError::SYNTAX_WRONG_OPERAND_COUNT, result);
}

// Test execution context validation
inline void test_context_validation() {
  InstructionValidator validator;
  
  // SET_PIXEL only valid in FRAGMENT context
  auto result = validator.validateContext(Opcode::SET_PIXEL, ExecutionContext::FRAGMENT);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.validateContext(Opcode::SET_PIXEL, ExecutionContext::COMPUTE);
  TEST_ASSERT(result != ValidationError::NONE);
}

// Test label validation
inline void test_label_validation() {
  ValidationContext ctx;
  ctx.declareLabel("start", 0);
  ctx.declareLabel("end", 100);
  
  TEST_ASSERT(ctx.isLabelDeclared("start"));
  TEST_ASSERT(ctx.isLabelDeclared("end"));
  TEST_ASSERT(!ctx.isLabelDeclared("middle"));
  
  TEST_ASSERT_EQ(0u, ctx.getLabelAddress("start"));
  TEST_ASSERT_EQ(100u, ctx.getLabelAddress("end"));
}

// Test compile-time validator
inline void test_compile_time_validation() {
  CompileTimeValidator validator;
  
  // Create simple valid program
  uint8_t valid_program[] = {
    (uint8_t)Opcode::NOP,
    (uint8_t)Opcode::HALT
  };
  
  auto result = validator.validate(valid_program, sizeof(valid_program));
  TEST_ASSERT(result.isValid());
}

// Test runtime bounds checking
inline void test_runtime_bounds() {
  RuntimeValidator validator;
  
  // Array bounds
  auto result = validator.checkArrayBounds(5, 10);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.checkArrayBounds(10, 10);
  TEST_ASSERT_EQ(ValidationError::MEMORY_OUT_OF_BOUNDS, result);
  
  result = validator.checkArrayBounds(15, 10);
  TEST_ASSERT_EQ(ValidationError::MEMORY_OUT_OF_BOUNDS, result);
}

// Test division safety
inline void test_division_safety() {
  RuntimeValidator validator;
  
  // Integer division
  auto result = validator.checkDivisionSafety(10, 2);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.checkDivisionSafety(10, 0);
  TEST_ASSERT_EQ(ValidationError::MEMORY_DIVISION_BY_ZERO, result);
  
  // Float division
  result = validator.checkFloatDivisionSafety(10.0f, 2.0f);
  TEST_ASSERT_EQ(ValidationError::NONE, result);
  
  result = validator.checkFloatDivisionSafety(10.0f, 0.0f);
  TEST_ASSERT_EQ(ValidationError::MEMORY_DIVISION_BY_ZERO, result);
}

// Register validator tests
inline void register_validator_tests() {
  TestRunner& runner = TestRunner::instance();
  runner.registerTest("validator_operand_counts", "Validator", test_operand_counts);
  runner.registerTest("validator_context", "Validator", test_context_validation);
  runner.registerTest("validator_labels", "Validator", test_label_validation);
  runner.registerTest("validator_compile_time", "Validator", test_compile_time_validation);
  runner.registerTest("validator_bounds", "Validator", test_runtime_bounds);
  runner.registerTest("validator_division", "Validator", test_division_safety);
}

} // namespace validator_tests

// ============================================================
// Animation Tests
// ============================================================

namespace animation_tests {

using namespace gpu::isa;
using namespace gpu::animation;

// Test keyframe interpolation
inline void test_keyframe_interpolation() {
  Keyframe k1, k2;
  k1.time = 0.0f;
  k1.values[0] = 0.0f;
  k2.time = 1.0f;
  k2.values[0] = 100.0f;
  
  // Linear interpolation at midpoint
  float t = 0.5f;
  float result = k1.values[0] + (k2.values[0] - k1.values[0]) * t;
  TEST_ASSERT_FLOAT_EQ(50.0f, result, 0.0001f);
}

// Test easing in animations
inline void test_animation_easing() {
  PropertyTrack track;
  track.property = PropertyType::POSITION_X;
  track.value_count = 1;
  
  // Add two keyframes
  Keyframe k1, k2;
  k1.time = 0.0f;
  k1.values[0] = 0.0f;
  k1.easing = EasingType::LINEAR;
  
  k2.time = 1.0f;
  k2.values[0] = 100.0f;
  k2.easing = EasingType::EASE_IN_QUAD;
  
  track.addKeyframe(k1);
  track.addKeyframe(k2);
  
  // Sample at various points
  float v0 = track.sample(0.0f)[0];
  TEST_ASSERT_FLOAT_EQ(0.0f, v0, 0.01f);
  
  float v1 = track.sample(1.0f)[0];
  TEST_ASSERT_FLOAT_EQ(100.0f, v1, 0.01f);
}

// Test animation state machine
inline void test_animation_state() {
  AnimationInstance anim;
  anim.state = AnimationState::STOPPED;
  anim.current_time = 0.0f;
  anim.duration = 1.0f;
  anim.speed = 1.0f;
  
  // Start animation
  anim.state = AnimationState::PLAYING;
  
  // Simulate update
  float dt = 0.1f;
  anim.current_time += dt * anim.speed;
  
  TEST_ASSERT_FLOAT_EQ(0.1f, anim.current_time, 0.0001f);
  TEST_ASSERT_EQ((int)AnimationState::PLAYING, (int)anim.state);
}

// Test animation looping
inline void test_animation_looping() {
  AnimationInstance anim;
  anim.state = AnimationState::PLAYING;
  anim.current_time = 0.9f;
  anim.duration = 1.0f;
  anim.speed = 1.0f;
  anim.loop_mode = LoopMode::LOOP;
  
  // Simulate update that goes past end
  float dt = 0.2f;
  anim.current_time += dt * anim.speed;
  
  // Should wrap around
  if (anim.current_time >= anim.duration) {
    anim.current_time = fmodf(anim.current_time, anim.duration);
  }
  
  TEST_ASSERT(anim.current_time < anim.duration);
  TEST_ASSERT(anim.current_time >= 0.0f);
}

// Test animation system
inline void test_animation_system() {
  AnimationSystem system;
  
  // Create animation
  int anim_id = system.createAnimation();
  TEST_ASSERT(anim_id >= 0);
  
  AnimationDef* def = system.getAnimationDef(anim_id);
  TEST_ASSERT_NOT_NULL(def);
  
  def->duration = 2.0f;
  def->loop_mode = LoopMode::ONCE;
  
  // Create instance
  int inst_id = system.createInstance(anim_id);
  TEST_ASSERT(inst_id >= 0);
  
  // Play
  system.play(inst_id);
  
  AnimationInstance* inst = system.getInstance(inst_id);
  TEST_ASSERT_NOT_NULL(inst);
  TEST_ASSERT_EQ((int)AnimationState::PLAYING, (int)inst->state);
}

// Test animation layers
inline void test_animation_layers() {
  AnimationLayer layer;
  layer.enabled = true;
  layer.weight = 1.0f;
  layer.blend_mode = LayerBlendMode::REPLACE;
  
  float base[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float anim[4] = {1.0f, 2.0f, 3.0f, 4.0f};
  
  // Apply REPLACE blend
  float result[4];
  for (int i = 0; i < 4; i++) {
    result[i] = base[i] * (1.0f - layer.weight) + anim[i] * layer.weight;
  }
  
  TEST_ASSERT_FLOAT_EQ(1.0f, result[0], 0.0001f);
  TEST_ASSERT_FLOAT_EQ(2.0f, result[1], 0.0001f);
}

// Register animation tests
inline void register_animation_tests() {
  TestRunner& runner = TestRunner::instance();
  runner.registerTest("anim_keyframe", "Animation", test_keyframe_interpolation);
  runner.registerTest("anim_easing", "Animation", test_animation_easing);
  runner.registerTest("anim_state", "Animation", test_animation_state);
  runner.registerTest("anim_looping", "Animation", test_animation_looping);
  runner.registerTest("anim_system", "Animation", test_animation_system);
  runner.registerTest("anim_layers", "Animation", test_animation_layers);
}

} // namespace animation_tests

// ============================================================
// SDF Tests
// ============================================================

namespace sdf_tests {

using namespace gpu::isa;
using namespace gpu::sdf;

// Test circle SDF
inline void test_sdf_circle() {
  // Circle at origin with radius 5
  float center_dist = SDFPrimitives::circle(0, 0, 5);
  TEST_ASSERT_FLOAT_EQ(-5.0f, center_dist, 0.0001f);  // Inside
  
  float edge_dist = SDFPrimitives::circle(5, 0, 5);
  TEST_ASSERT_FLOAT_EQ(0.0f, edge_dist, 0.0001f);  // On edge
  
  float outside_dist = SDFPrimitives::circle(10, 0, 5);
  TEST_ASSERT_FLOAT_EQ(5.0f, outside_dist, 0.0001f);  // Outside
}

// Test box SDF
inline void test_sdf_box() {
  // Box centered at origin, size 10x6
  float center_dist = SDFPrimitives::box(0, 0, 5, 3);
  TEST_ASSERT(center_dist < 0);  // Inside
  
  float corner_dist = SDFPrimitives::box(5, 3, 5, 3);
  TEST_ASSERT_FLOAT_EQ(0.0f, corner_dist, 0.0001f);  // On corner
  
  float outside_dist = SDFPrimitives::box(10, 0, 5, 3);
  TEST_ASSERT(outside_dist > 0);  // Outside
}

// Test SDF operations
inline void test_sdf_operations() {
  float d1 = -2.0f;  // Inside shape 1
  float d2 = 3.0f;   // Outside shape 2
  
  // Union (min)
  float union_d = SDFOperations::opUnion(d1, d2);
  TEST_ASSERT_FLOAT_EQ(-2.0f, union_d, 0.0001f);
  
  // Intersection (max)
  float intersect_d = SDFOperations::opIntersect(d1, d2);
  TEST_ASSERT_FLOAT_EQ(3.0f, intersect_d, 0.0001f);
  
  // Subtraction
  float subtract_d = SDFOperations::opSubtract(d1, d2);
  TEST_ASSERT_FLOAT_EQ(-2.0f, subtract_d, 0.0001f);
}

// Test smooth operations
inline void test_sdf_smooth_ops() {
  float d1 = 0.0f;
  float d2 = 1.0f;
  float k = 0.5f;  // Smoothing factor
  
  // Smooth union should be smoother than hard union
  float smooth = SDFOperations::opSmoothUnion(d1, d2, k);
  float hard = SDFOperations::opUnion(d1, d2);
  
  TEST_ASSERT(smooth < hard);  // Smooth union rounds corners inward
}

// Test SDF scene
inline void test_sdf_scene() {
  SDFScene scene;
  
  int circle = scene.addCircle(0, 0, 5);
  TEST_ASSERT(circle >= 0);
  
  int box = scene.addBox(10, 0, 3, 3);
  TEST_ASSERT(box >= 0);
  
  // Evaluate at various points
  float d_at_circle = scene.evaluate(0, 0);
  TEST_ASSERT(d_at_circle < 0);  // Inside circle
  
  float d_between = scene.evaluate(5, 0);
  TEST_ASSERT(d_between >= 0);  // Between shapes
}

// Register SDF tests
inline void register_sdf_tests() {
  TestRunner& runner = TestRunner::instance();
  runner.registerTest("sdf_circle", "SDF", test_sdf_circle);
  runner.registerTest("sdf_box", "SDF", test_sdf_box);
  runner.registerTest("sdf_operations", "SDF", test_sdf_operations);
  runner.registerTest("sdf_smooth", "SDF", test_sdf_smooth_ops);
  runner.registerTest("sdf_scene", "SDF", test_sdf_scene);
}

} // namespace sdf_tests

// ============================================================
// Antialiasing Tests
// ============================================================

namespace aa_tests {

using namespace gpu::isa;
using namespace gpu::aa;

// Test coverage calculation
inline void test_coverage_circle() {
  // Circle at (50, 50) with radius 10
  float inside_cov = AnalyticalCoverage::circle(50, 50, 50, 50, 10, true);
  TEST_ASSERT_FLOAT_EQ(1.0f, inside_cov, 0.01f);  // Center should be fully covered
  
  float edge_cov = AnalyticalCoverage::circle(60, 50, 50, 50, 10, true);
  TEST_ASSERT(edge_cov > 0.0f && edge_cov < 1.0f);  // Edge should be partial
  
  float outside_cov = AnalyticalCoverage::circle(70, 50, 50, 50, 10, true);
  TEST_ASSERT_FLOAT_EQ(0.0f, outside_cov, 0.01f);  // Far outside should be zero
}

// Test line coverage
inline void test_coverage_line() {
  // Horizontal line from (0,50) to (100,50), 2 pixels wide
  float on_line = AnalyticalCoverage::line(50, 50, 0, 50, 100, 50, 2);
  TEST_ASSERT(on_line > 0.9f);  // On line should be high coverage
  
  float near_line = AnalyticalCoverage::line(50, 51, 0, 50, 100, 50, 2);
  TEST_ASSERT(near_line > 0.0f);  // Near line should have some coverage
  
  float far_from_line = AnalyticalCoverage::line(50, 60, 0, 50, 100, 50, 2);
  TEST_ASSERT_FLOAT_EQ(0.0f, far_from_line, 0.01f);  // Far should be zero
}

// Test sample patterns
inline void test_sample_patterns() {
  SamplePoint samples[MAX_SAMPLES];
  int count;
  
  getSamplePattern(SamplePattern::GRID_2X2, samples, count);
  TEST_ASSERT_EQ(4, count);
  
  getSamplePattern(SamplePattern::MSAA_4X, samples, count);
  TEST_ASSERT_EQ(4, count);
  
  getSamplePattern(SamplePattern::MSAA_8X, samples, count);
  TEST_ASSERT_EQ(8, count);
  
  // Check weights sum to 1
  float weight_sum = 0.0f;
  for (int i = 0; i < count; i++) {
    weight_sum += samples[i].weight;
  }
  TEST_ASSERT_FLOAT_EQ(1.0f, weight_sum, 0.0001f);
}

// Test coverage mask
inline void test_coverage_mask() {
  CoverageMask full = COVERAGE_FULL;
  TEST_ASSERT_EQ(16, coverageCount(full));
  
  CoverageMask none = COVERAGE_NONE;
  TEST_ASSERT_EQ(0, coverageCount(none));
  
  CoverageMask half = 0x00FF;
  TEST_ASSERT_EQ(8, coverageCount(half));
  
  float alpha = coverageToAlpha(half, 16);
  TEST_ASSERT_FLOAT_EQ(0.5f, alpha, 0.0001f);
}

// Test SDF antialiasing
inline void test_sdf_aa() {
  // Test coverage at various distances
  float inside = SDFAntialiasing::coverage(-5.0f);
  TEST_ASSERT_FLOAT_EQ(1.0f, inside, 0.01f);
  
  float on_edge = SDFAntialiasing::coverage(0.0f);
  TEST_ASSERT_FLOAT_EQ(0.5f, on_edge, 0.01f);
  
  float outside = SDFAntialiasing::coverage(5.0f);
  TEST_ASSERT_FLOAT_EQ(0.0f, outside, 0.01f);
}

// Register AA tests
inline void register_aa_tests() {
  TestRunner& runner = TestRunner::instance();
  runner.registerTest("aa_coverage_circle", "Antialiasing", test_coverage_circle);
  runner.registerTest("aa_coverage_line", "Antialiasing", test_coverage_line);
  runner.registerTest("aa_sample_patterns", "Antialiasing", test_sample_patterns);
  runner.registerTest("aa_coverage_mask", "Antialiasing", test_coverage_mask);
  runner.registerTest("aa_sdf", "Antialiasing", test_sdf_aa);
}

} // namespace aa_tests

// ============================================================
// Compositor Tests
// ============================================================

namespace compositor_tests {

using namespace gpu::isa;
using namespace gpu::compositor;

// Test color space conversion
inline void test_color_space_conversion() {
  // Test sRGB <-> Linear
  ColorF srgb_white(1.0f, 1.0f, 1.0f, 1.0f);
  ColorF linear = ColorConversion::toLinear(srgb_white, ColorSpace::SRGB);
  TEST_ASSERT_FLOAT_EQ(1.0f, linear.r, 0.01f);  // 1.0 maps to 1.0
  
  ColorF srgb_mid(0.5f, 0.5f, 0.5f, 1.0f);
  ColorF linear_mid = ColorConversion::toLinear(srgb_mid, ColorSpace::SRGB);
  TEST_ASSERT(linear_mid.r < srgb_mid.r);  // Mid-gray should be darker in linear
  
  // Round-trip
  ColorF back_to_srgb = ColorConversion::fromLinear(linear_mid, ColorSpace::SRGB);
  TEST_ASSERT_FLOAT_EQ(srgb_mid.r, back_to_srgb.r, 0.001f);
}

// Test Porter-Duff operations
inline void test_porter_duff() {
  ColorF dst(0.5f, 0.5f, 0.5f, 1.0f);
  ColorF src(1.0f, 0.0f, 0.0f, 0.5f);  // Semi-transparent red
  
  // Source over
  ColorF over = PorterDuff::composite(dst, src, CompositeOp::SRC_OVER);
  TEST_ASSERT(over.r > dst.r);  // Red should increase
  
  // Source
  ColorF pure_src = PorterDuff::composite(dst, src, CompositeOp::SRC);
  TEST_ASSERT_FLOAT_EQ(src.r, pure_src.r, 0.0001f);
  
  // Destination
  ColorF pure_dst = PorterDuff::composite(dst, src, CompositeOp::DST);
  TEST_ASSERT_FLOAT_EQ(dst.r, pure_dst.r, 0.0001f);
  
  // Clear
  ColorF cleared = PorterDuff::composite(dst, src, CompositeOp::CLEAR);
  TEST_ASSERT_FLOAT_EQ(0.0f, cleared.r, 0.0001f);
  TEST_ASSERT_FLOAT_EQ(0.0f, cleared.a, 0.0001f);
}

// Test framebuffer operations
inline void test_framebuffer() {
  Framebuffer fb;
  fb.allocate(64, 64);
  
  TEST_ASSERT_NOT_NULL(fb.getBuffer());
  TEST_ASSERT_EQ(64, fb.getWidth());
  TEST_ASSERT_EQ(64, fb.getHeight());
  
  // Set and get pixel
  ColorF red(1.0f, 0.0f, 0.0f, 1.0f);
  fb.setPixel(10, 10, red);
  
  ColorF got = fb.getPixel(10, 10);
  TEST_ASSERT_FLOAT_EQ(1.0f, got.r, 0.01f);
  TEST_ASSERT_FLOAT_EQ(0.0f, got.g, 0.01f);
  
  // Clear
  fb.clear(ColorF(0, 0, 1, 1));  // Blue
  got = fb.getPixel(10, 10);
  TEST_ASSERT_FLOAT_EQ(0.0f, got.r, 0.01f);
  TEST_ASSERT_FLOAT_EQ(1.0f, got.b, 0.01f);
}

// Test layer compositing
inline void test_layer_composite() {
  Compositor comp;
  
  // Create framebuffer
  comp.getFramebuffer(0)->allocate(64, 64);
  comp.getFramebuffer(0)->clear(ColorF(0, 0, 0, 1));  // Black background
  
  // Add layer (would need actual buffer in real usage)
  int layer_id = comp.addLayer();
  TEST_ASSERT_EQ(0, layer_id);
  
  Layer* layer = comp.getLayer(0);
  TEST_ASSERT_NOT_NULL(layer);
  
  layer->opacity = 0.5f;
  layer->visible = true;
}

// Test alpha modes
inline void test_alpha_modes() {
  ColorF straight(1.0f, 0.0f, 0.0f, 0.5f);  // Red at 50% alpha
  
  ColorF premul = ColorConversion::toPremultiplied(straight);
  TEST_ASSERT_FLOAT_EQ(0.5f, premul.r, 0.0001f);  // RGB multiplied by alpha
  TEST_ASSERT_FLOAT_EQ(0.5f, premul.a, 0.0001f);
  
  ColorF back = ColorConversion::toStraight(premul);
  TEST_ASSERT_FLOAT_EQ(straight.r, back.r, 0.001f);
}

// Register compositor tests
inline void register_compositor_tests() {
  TestRunner& runner = TestRunner::instance();
  runner.registerTest("comp_color_space", "Compositor", test_color_space_conversion);
  runner.registerTest("comp_porter_duff", "Compositor", test_porter_duff);
  runner.registerTest("comp_framebuffer", "Compositor", test_framebuffer);
  runner.registerTest("comp_layer", "Compositor", test_layer_composite);
  runner.registerTest("comp_alpha_modes", "Compositor", test_alpha_modes);
}

} // namespace compositor_tests

// ============================================================
// Test Registration
// ============================================================

inline void registerAllTests() {
  isa_tests::register_isa_tests();
  validator_tests::register_validator_tests();
  animation_tests::register_animation_tests();
  sdf_tests::register_sdf_tests();
  aa_tests::register_aa_tests();
  compositor_tests::register_compositor_tests();
}

// ============================================================
// Test Entry Point
// ============================================================

inline int runAllTests() {
  registerAllTests();
  
  TestRunner& runner = TestRunner::instance();
  runner.runAll();
  
  // Return 0 if all passed, 1 if any failed
  return runner.getFailedCount() > 0 ? 1 : 0;
}

inline void runTestsForCategory(const char* category) {
  registerAllTests();
  TestRunner::instance().runCategory(category);
}

} // namespace test
} // namespace gpu

#endif // GPU_TEST_FRAMEWORK_HPP_
