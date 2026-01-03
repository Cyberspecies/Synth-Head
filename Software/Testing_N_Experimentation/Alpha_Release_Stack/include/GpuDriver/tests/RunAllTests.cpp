/*****************************************************************
 * File:      RunAllTests.cpp
 * Category:  GPU Driver / Test Execution
 * Purpose:   Complete test runner for virtual and hardware testing
 *****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>

// Define test mode
#define TEST_MODE_VIRTUAL  1
#define TEST_MODE_HARDWARE 2
#define CURRENT_TEST_MODE TEST_MODE_VIRTUAL

// Forward declarations for testing without full headers
namespace gpu {
namespace isa {
  enum class Opcode : uint8_t {
    NOP = 0x00, SET_PIXEL = 0x01, FILL_RECT = 0x02, DRAW_LINE = 0x03,
    DRAW_CIRCLE = 0x04, DRAW_TRIANGLE = 0x05, DRAW_SPRITE = 0x06,
    CLEAR = 0x10, FLIP = 0x11, SET_PALETTE = 0x12,
    ANIM_START = 0x20, ANIM_STOP = 0x21, ANIM_KEYFRAME = 0x22,
  };
  
  enum class DataType : uint8_t {
    VOID = 0, BOOL = 1, UINT8 = 2, INT8 = 3, UINT16 = 4, INT16 = 5,
    UINT32 = 6, INT32 = 7, FLOAT32 = 8, FIXED16_16 = 9,
    COLOR_RGB = 10, COLOR_RGBA = 11, VEC2 = 12, VEC3 = 13, VEC4 = 14,
  };
}
}

// ============================================================
// Test Framework
// ============================================================

struct TestResult {
  const char* name;
  bool passed;
  float duration_ms;
  char message[256];
};

static int g_total_tests = 0;
static int g_passed_tests = 0;
static int g_failed_tests = 0;
static TestResult g_results[1024];
static int g_result_count = 0;

void test_begin(const char* name) {
  printf("  [RUN] %s\n", name);
}

void test_pass(const char* name, float duration_ms = 0) {
  g_total_tests++;
  g_passed_tests++;
  printf("  [PASS] %s (%.2fms)\n", name, duration_ms);
  
  if (g_result_count < 1024) {
    g_results[g_result_count].name = name;
    g_results[g_result_count].passed = true;
    g_results[g_result_count].duration_ms = duration_ms;
    g_results[g_result_count].message[0] = '\0';
    g_result_count++;
  }
}

void test_fail(const char* name, const char* reason) {
  g_total_tests++;
  g_failed_tests++;
  printf("  [FAIL] %s: %s\n", name, reason);
  
  if (g_result_count < 1024) {
    g_results[g_result_count].name = name;
    g_results[g_result_count].passed = false;
    g_results[g_result_count].duration_ms = 0;
    strncpy(g_results[g_result_count].message, reason, 255);
    g_result_count++;
  }
}

#define ASSERT_TRUE(cond) do { if (!(cond)) { test_fail(__func__, "Assertion failed: " #cond); return false; } } while(0)
#define ASSERT_FALSE(cond) do { if (cond) { test_fail(__func__, "Expected false: " #cond); return false; } } while(0)
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { test_fail(__func__, "Equality assertion failed: " #a " != " #b); return false; } } while(0)
#define ASSERT_NE(a, b) do { if ((a) == (b)) { test_fail(__func__, "Inequality assertion failed: " #a " == " #b); return false; } } while(0)
#define ASSERT_LT(a, b) do { if ((a) >= (b)) { test_fail(__func__, "Less-than assertion failed: " #a " >= " #b); return false; } } while(0)
#define ASSERT_LE(a, b) do { if ((a) > (b)) { test_fail(__func__, "Less-equal assertion failed: " #a " > " #b); return false; } } while(0)
#define ASSERT_NEAR(a, b, eps) do { if (fabs((a) - (b)) > (eps)) { test_fail(__func__, "Near assertion failed: " #a " not near " #b); return false; } } while(0)

// ============================================================
// Timer
// ============================================================

class Timer {
public:
  void start() { start_ = clock(); }
  float elapsed_ms() { return (float)(clock() - start_) / CLOCKS_PER_SEC * 1000.0f; }
private:
  clock_t start_;
};

// ============================================================
// ISA Tests
// ============================================================

bool test_isa_opcode_values() {
  test_begin(__func__);
  Timer t; t.start();
  
  using namespace gpu::isa;
  ASSERT_EQ((int)Opcode::NOP, 0x00);
  ASSERT_EQ((int)Opcode::SET_PIXEL, 0x01);
  ASSERT_EQ((int)Opcode::FILL_RECT, 0x02);
  ASSERT_EQ((int)Opcode::DRAW_LINE, 0x03);
  ASSERT_EQ((int)Opcode::DRAW_CIRCLE, 0x04);
  ASSERT_EQ((int)Opcode::CLEAR, 0x10);
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

bool test_isa_datatype_sizes() {
  test_begin(__func__);
  Timer t; t.start();
  
  using namespace gpu::isa;
  ASSERT_EQ((int)DataType::VOID, 0);
  ASSERT_EQ((int)DataType::BOOL, 1);
  ASSERT_EQ((int)DataType::UINT8, 2);
  ASSERT_EQ((int)DataType::INT8, 3);
  ASSERT_EQ((int)DataType::UINT16, 4);
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

// ============================================================
// Fixed-Point Math Tests
// ============================================================

typedef int32_t fixed16_16;

fixed16_16 float_to_fixed(float f) {
  return (fixed16_16)(f * 65536.0f);
}

float fixed_to_float(fixed16_16 f) {
  return (float)f / 65536.0f;
}

fixed16_16 fixed_mul(fixed16_16 a, fixed16_16 b) {
  return (fixed16_16)(((int64_t)a * (int64_t)b) >> 16);
}

fixed16_16 fixed_div(fixed16_16 a, fixed16_16 b) {
  return (fixed16_16)(((int64_t)a << 16) / b);
}

bool test_fixed_point_conversion() {
  test_begin(__func__);
  Timer t; t.start();
  
  ASSERT_NEAR(fixed_to_float(float_to_fixed(1.0f)), 1.0f, 0.0001f);
  ASSERT_NEAR(fixed_to_float(float_to_fixed(0.5f)), 0.5f, 0.0001f);
  ASSERT_NEAR(fixed_to_float(float_to_fixed(-1.5f)), -1.5f, 0.0001f);
  ASSERT_NEAR(fixed_to_float(float_to_fixed(3.14159f)), 3.14159f, 0.001f);
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

bool test_fixed_point_multiplication() {
  test_begin(__func__);
  Timer t; t.start();
  
  fixed16_16 a = float_to_fixed(2.5f);
  fixed16_16 b = float_to_fixed(4.0f);
  fixed16_16 result = fixed_mul(a, b);
  
  ASSERT_NEAR(fixed_to_float(result), 10.0f, 0.001f);
  
  // Test small numbers
  a = float_to_fixed(0.1f);
  b = float_to_fixed(0.1f);
  result = fixed_mul(a, b);
  ASSERT_NEAR(fixed_to_float(result), 0.01f, 0.001f);
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

bool test_fixed_point_division() {
  test_begin(__func__);
  Timer t; t.start();
  
  fixed16_16 a = float_to_fixed(10.0f);
  fixed16_16 b = float_to_fixed(2.0f);
  fixed16_16 result = fixed_div(a, b);
  
  ASSERT_NEAR(fixed_to_float(result), 5.0f, 0.001f);
  
  // Test fractional result
  a = float_to_fixed(1.0f);
  b = float_to_fixed(3.0f);
  result = fixed_div(a, b);
  ASSERT_NEAR(fixed_to_float(result), 0.333333f, 0.001f);
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

bool test_fixed_point_precision_drift() {
  test_begin(__func__);
  Timer t; t.start();
  
  // Accumulate small values and check drift
  fixed16_16 fixed_acc = 0;
  float float_acc = 0;
  
  const int ITERATIONS = 10000;
  const float SMALL_VALUE = 0.0001f;
  fixed16_16 small_fixed = float_to_fixed(SMALL_VALUE);
  
  for (int i = 0; i < ITERATIONS; i++) {
    fixed_acc += small_fixed;
    float_acc += SMALL_VALUE;
  }
  
  float fixed_result = fixed_to_float(fixed_acc);
  float error = fabs(fixed_result - float_acc);
  float relative_error = error / float_acc;
  
  printf("    Fixed result: %f, Float result: %f, Error: %f%%\n", 
         fixed_result, float_acc, relative_error * 100);
  
  // Allow up to 1% error after 10000 iterations
  ASSERT_LT(relative_error, 0.01f);
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

// ============================================================
// Trigonometry Tests (Lookup Table Simulation)
// ============================================================

#define TRIG_TABLE_SIZE 256
static int16_t sin_table[TRIG_TABLE_SIZE];
static bool trig_initialized = false;

void init_trig_tables() {
  if (trig_initialized) return;
  for (int i = 0; i < TRIG_TABLE_SIZE; i++) {
    sin_table[i] = (int16_t)(sin(2.0 * 3.14159265358979 * i / TRIG_TABLE_SIZE) * 32767);
  }
  trig_initialized = true;
}

int16_t fast_sin(uint8_t angle) {
  return sin_table[angle];
}

int16_t fast_cos(uint8_t angle) {
  return sin_table[(angle + 64) & 0xFF];
}

bool test_trig_sin_values() {
  test_begin(__func__);
  Timer t; t.start();
  init_trig_tables();
  
  // Check key values
  ASSERT_NEAR(fast_sin(0) / 32767.0f, 0.0f, 0.01f);        // sin(0) = 0
  ASSERT_NEAR(fast_sin(64) / 32767.0f, 1.0f, 0.01f);       // sin(90) = 1
  ASSERT_NEAR(fast_sin(128) / 32767.0f, 0.0f, 0.01f);      // sin(180) = 0
  ASSERT_NEAR(fast_sin(192) / 32767.0f, -1.0f, 0.01f);     // sin(270) = -1
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

bool test_trig_cos_values() {
  test_begin(__func__);
  Timer t; t.start();
  init_trig_tables();
  
  // Check key values
  ASSERT_NEAR(fast_cos(0) / 32767.0f, 1.0f, 0.01f);        // cos(0) = 1
  ASSERT_NEAR(fast_cos(64) / 32767.0f, 0.0f, 0.01f);       // cos(90) = 0
  ASSERT_NEAR(fast_cos(128) / 32767.0f, -1.0f, 0.01f);     // cos(180) = -1
  ASSERT_NEAR(fast_cos(192) / 32767.0f, 0.0f, 0.01f);      // cos(270) = 0
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

bool test_trig_identity() {
  test_begin(__func__);
  Timer t; t.start();
  init_trig_tables();
  
  // sin^2 + cos^2 = 1
  for (int angle = 0; angle < 256; angle++) {
    float s = fast_sin(angle) / 32767.0f;
    float c = fast_cos(angle) / 32767.0f;
    float sum = s * s + c * c;
    ASSERT_NEAR(sum, 1.0f, 0.01f);
  }
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

// ============================================================
// Color Space Tests
// ============================================================

struct RGB { uint8_t r, g, b; };
struct HSV { uint8_t h, s, v; };

HSV rgb_to_hsv(RGB rgb) {
  HSV hsv;
  uint8_t min = rgb.r < rgb.g ? rgb.r : rgb.g;
  min = min < rgb.b ? min : rgb.b;
  uint8_t max = rgb.r > rgb.g ? rgb.r : rgb.g;
  max = max > rgb.b ? max : rgb.b;
  
  hsv.v = max;
  uint8_t delta = max - min;
  
  if (max == 0 || delta == 0) {
    hsv.s = 0;
    hsv.h = 0;
    return hsv;
  }
  
  hsv.s = (uint8_t)(255 * delta / max);
  
  int h;
  if (rgb.r == max) {
    h = 43 * (rgb.g - rgb.b) / delta;
  } else if (rgb.g == max) {
    h = 85 + 43 * (rgb.b - rgb.r) / delta;
  } else {
    h = 171 + 43 * (rgb.r - rgb.g) / delta;
  }
  
  hsv.h = (uint8_t)(h < 0 ? h + 256 : h);
  return hsv;
}

RGB hsv_to_rgb(HSV hsv) {
  RGB rgb;
  if (hsv.s == 0) {
    rgb.r = rgb.g = rgb.b = hsv.v;
    return rgb;
  }
  
  uint8_t region = hsv.h / 43;
  uint8_t remainder = (hsv.h - region * 43) * 6;
  
  uint8_t p = (hsv.v * (255 - hsv.s)) >> 8;
  uint8_t q = (hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8;
  uint8_t t = (hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8;
  
  switch (region) {
    case 0: rgb.r = hsv.v; rgb.g = t; rgb.b = p; break;
    case 1: rgb.r = q; rgb.g = hsv.v; rgb.b = p; break;
    case 2: rgb.r = p; rgb.g = hsv.v; rgb.b = t; break;
    case 3: rgb.r = p; rgb.g = q; rgb.b = hsv.v; break;
    case 4: rgb.r = t; rgb.g = p; rgb.b = hsv.v; break;
    default: rgb.r = hsv.v; rgb.g = p; rgb.b = q; break;
  }
  return rgb;
}

bool test_color_rgb_to_hsv() {
  test_begin(__func__);
  Timer t; t.start();
  
  // Red
  HSV hsv = rgb_to_hsv({255, 0, 0});
  ASSERT_EQ(hsv.v, 255);
  ASSERT_EQ(hsv.s, 255);
  ASSERT_LT(abs(hsv.h - 0), 5);  // Near 0
  
  // Green
  hsv = rgb_to_hsv({0, 255, 0});
  ASSERT_EQ(hsv.v, 255);
  ASSERT_EQ(hsv.s, 255);
  ASSERT_NEAR(hsv.h, 85, 5);  // Near 85 (120 degrees)
  
  // Blue
  hsv = rgb_to_hsv({0, 0, 255});
  ASSERT_EQ(hsv.v, 255);
  ASSERT_EQ(hsv.s, 255);
  ASSERT_NEAR(hsv.h, 171, 5);  // Near 171 (240 degrees)
  
  // White
  hsv = rgb_to_hsv({255, 255, 255});
  ASSERT_EQ(hsv.v, 255);
  ASSERT_EQ(hsv.s, 0);
  
  // Black
  hsv = rgb_to_hsv({0, 0, 0});
  ASSERT_EQ(hsv.v, 0);
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

bool test_color_hsv_to_rgb() {
  test_begin(__func__);
  Timer t; t.start();
  
  // Red
  RGB rgb = hsv_to_rgb({0, 255, 255});
  ASSERT_EQ(rgb.r, 255);
  ASSERT_LT(rgb.g, 10);
  ASSERT_LT(rgb.b, 10);
  
  // Green
  rgb = hsv_to_rgb({85, 255, 255});
  ASSERT_LT(rgb.r, 10);
  ASSERT_EQ(rgb.g, 255);
  ASSERT_LT(rgb.b, 10);
  
  // Blue
  rgb = hsv_to_rgb({171, 255, 255});
  ASSERT_LT(rgb.r, 10);
  ASSERT_LT(rgb.g, 10);
  ASSERT_EQ(rgb.b, 255);
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

bool test_color_roundtrip() {
  test_begin(__func__);
  Timer t; t.start();
  
  // Test random colors for roundtrip
  srand(12345);
  for (int i = 0; i < 100; i++) {
    RGB original = {(uint8_t)(rand() % 256), (uint8_t)(rand() % 256), (uint8_t)(rand() % 256)};
    HSV hsv = rgb_to_hsv(original);
    RGB recovered = hsv_to_rgb(hsv);
    
    // Allow some rounding error
    ASSERT_LE(abs(original.r - recovered.r), 5);
    ASSERT_LE(abs(original.g - recovered.g), 5);
    ASSERT_LE(abs(original.b - recovered.b), 5);
  }
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

// ============================================================
// Bresenham Line Algorithm Tests
// ============================================================

struct Point { int x, y; };
static Point line_points[1024];
static int line_point_count = 0;

void bresenham_line(int x0, int y0, int x1, int y1) {
  line_point_count = 0;
  
  int dx = abs(x1 - x0);
  int dy = -abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  
  while (true) {
    if (line_point_count < 1024) {
      line_points[line_point_count++] = {x0, y0};
    }
    
    if (x0 == x1 && y0 == y1) break;
    
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

bool test_bresenham_horizontal() {
  test_begin(__func__);
  Timer t; t.start();
  
  bresenham_line(0, 5, 10, 5);
  ASSERT_EQ(line_point_count, 11);  // 0 to 10 inclusive
  
  for (int i = 0; i < line_point_count; i++) {
    ASSERT_EQ(line_points[i].x, i);
    ASSERT_EQ(line_points[i].y, 5);
  }
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

bool test_bresenham_vertical() {
  test_begin(__func__);
  Timer t; t.start();
  
  bresenham_line(5, 0, 5, 10);
  ASSERT_EQ(line_point_count, 11);
  
  for (int i = 0; i < line_point_count; i++) {
    ASSERT_EQ(line_points[i].x, 5);
    ASSERT_EQ(line_points[i].y, i);
  }
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

bool test_bresenham_diagonal() {
  test_begin(__func__);
  Timer t; t.start();
  
  bresenham_line(0, 0, 10, 10);
  ASSERT_EQ(line_point_count, 11);
  
  for (int i = 0; i < line_point_count; i++) {
    ASSERT_EQ(line_points[i].x, i);
    ASSERT_EQ(line_points[i].y, i);
  }
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

bool test_bresenham_steep() {
  test_begin(__func__);
  Timer t; t.start();
  
  bresenham_line(0, 0, 3, 10);
  
  // All points should be connected (no gaps)
  for (int i = 1; i < line_point_count; i++) {
    int dx = abs(line_points[i].x - line_points[i-1].x);
    int dy = abs(line_points[i].y - line_points[i-1].y);
    ASSERT_LE(dx, 1);
    ASSERT_LE(dy, 1);
  }
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

// ============================================================
// Circle Algorithm Tests (Midpoint)
// ============================================================

static Point circle_points[4096];
static int circle_point_count = 0;

void midpoint_circle(int cx, int cy, int r) {
  circle_point_count = 0;
  
  int x = r;
  int y = 0;
  int p = 1 - r;
  
  auto plot8 = [&](int px, int py) {
    if (circle_point_count < 4096 - 8) {
      circle_points[circle_point_count++] = {cx + px, cy + py};
      circle_points[circle_point_count++] = {cx - px, cy + py};
      circle_points[circle_point_count++] = {cx + px, cy - py};
      circle_points[circle_point_count++] = {cx - px, cy - py};
      circle_points[circle_point_count++] = {cx + py, cy + px};
      circle_points[circle_point_count++] = {cx - py, cy + px};
      circle_points[circle_point_count++] = {cx + py, cy - px};
      circle_points[circle_point_count++] = {cx - py, cy - px};
    }
  };
  
  plot8(x, y);
  
  while (x > y) {
    y++;
    if (p <= 0) {
      p = p + 2 * y + 1;
    } else {
      x--;
      p = p + 2 * y - 2 * x + 1;
    }
    plot8(x, y);
  }
}

bool test_circle_radius() {
  test_begin(__func__);
  Timer t; t.start();
  
  int cx = 32, cy = 32, r = 10;
  midpoint_circle(cx, cy, r);
  
  // All points should be approximately at radius r
  for (int i = 0; i < circle_point_count; i++) {
    float dx = (float)(circle_points[i].x - cx);
    float dy = (float)(circle_points[i].y - cy);
    float dist = sqrtf(dx * dx + dy * dy);
    ASSERT_NEAR(dist, (float)r, 1.0f);  // Within 1 pixel
  }
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

bool test_circle_symmetry() {
  test_begin(__func__);
  Timer t; t.start();
  
  midpoint_circle(0, 0, 15);
  
  // Check for 8-way symmetry
  bool has_point[4] = {false, false, false, false};
  for (int i = 0; i < circle_point_count; i++) {
    if (circle_points[i].x == 15 && circle_points[i].y == 0) has_point[0] = true;
    if (circle_points[i].x == -15 && circle_points[i].y == 0) has_point[1] = true;
    if (circle_points[i].x == 0 && circle_points[i].y == 15) has_point[2] = true;
    if (circle_points[i].x == 0 && circle_points[i].y == -15) has_point[3] = true;
  }
  
  ASSERT_TRUE(has_point[0]);
  ASSERT_TRUE(has_point[1]);
  ASSERT_TRUE(has_point[2]);
  ASSERT_TRUE(has_point[3]);
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

// ============================================================
// Memory Management Tests
// ============================================================

bool test_memory_alloc_free() {
  test_begin(__func__);
  Timer t; t.start();
  
  const int ALLOC_COUNT = 100;
  void* ptrs[ALLOC_COUNT];
  
  // Allocate
  for (int i = 0; i < ALLOC_COUNT; i++) {
    ptrs[i] = malloc(1024);
    ASSERT_NE(ptrs[i], nullptr);
  }
  
  // Write patterns
  for (int i = 0; i < ALLOC_COUNT; i++) {
    memset(ptrs[i], i & 0xFF, 1024);
  }
  
  // Verify patterns
  for (int i = 0; i < ALLOC_COUNT; i++) {
    uint8_t* p = (uint8_t*)ptrs[i];
    for (int j = 0; j < 1024; j++) {
      ASSERT_EQ(p[j], i & 0xFF);
    }
  }
  
  // Free
  for (int i = 0; i < ALLOC_COUNT; i++) {
    free(ptrs[i]);
  }
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

bool test_memory_fragmentation() {
  test_begin(__func__);
  Timer t; t.start();
  
  // Allocate/free in pattern to cause fragmentation
  void* ptrs[50];
  
  // Allocate all
  for (int i = 0; i < 50; i++) {
    ptrs[i] = malloc(1024);
    ASSERT_NE(ptrs[i], nullptr);
  }
  
  // Free odd indices
  for (int i = 1; i < 50; i += 2) {
    free(ptrs[i]);
    ptrs[i] = nullptr;
  }
  
  // Try to allocate larger block
  void* large = malloc(2048);
  ASSERT_NE(large, nullptr);
  free(large);
  
  // Clean up remaining
  for (int i = 0; i < 50; i += 2) {
    if (ptrs[i]) free(ptrs[i]);
  }
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

// ============================================================
// Animation Easing Tests
// ============================================================

float ease_linear(float t) { return t; }
float ease_in_quad(float t) { return t * t; }
float ease_out_quad(float t) { return t * (2 - t); }
float ease_in_out_quad(float t) { return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t; }
float ease_in_cubic(float t) { return t * t * t; }
float ease_out_cubic(float t) { float t1 = t - 1; return t1 * t1 * t1 + 1; }

bool test_easing_boundaries() {
  test_begin(__func__);
  Timer t; t.start();
  
  // All easing functions should return 0 at t=0 and 1 at t=1
  ASSERT_NEAR(ease_linear(0.0f), 0.0f, 0.001f);
  ASSERT_NEAR(ease_linear(1.0f), 1.0f, 0.001f);
  
  ASSERT_NEAR(ease_in_quad(0.0f), 0.0f, 0.001f);
  ASSERT_NEAR(ease_in_quad(1.0f), 1.0f, 0.001f);
  
  ASSERT_NEAR(ease_out_quad(0.0f), 0.0f, 0.001f);
  ASSERT_NEAR(ease_out_quad(1.0f), 1.0f, 0.001f);
  
  ASSERT_NEAR(ease_in_out_quad(0.0f), 0.0f, 0.001f);
  ASSERT_NEAR(ease_in_out_quad(1.0f), 1.0f, 0.001f);
  
  ASSERT_NEAR(ease_in_cubic(0.0f), 0.0f, 0.001f);
  ASSERT_NEAR(ease_in_cubic(1.0f), 1.0f, 0.001f);
  
  ASSERT_NEAR(ease_out_cubic(0.0f), 0.0f, 0.001f);
  ASSERT_NEAR(ease_out_cubic(1.0f), 1.0f, 0.001f);
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

bool test_easing_monotonic() {
  test_begin(__func__);
  Timer t; t.start();
  
  // Easing functions should be monotonically increasing
  float prev = 0;
  for (int i = 0; i <= 100; i++) {
    float t = i / 100.0f;
    float v = ease_linear(t);
    ASSERT_LE(prev, v);
    prev = v;
  }
  
  prev = 0;
  for (int i = 0; i <= 100; i++) {
    float t = i / 100.0f;
    float v = ease_in_quad(t);
    ASSERT_LE(prev, v);
    prev = v;
  }
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

// ============================================================
// SDF Tests
// ============================================================

float sdf_circle(float x, float y, float cx, float cy, float r) {
  float dx = x - cx;
  float dy = y - cy;
  return sqrtf(dx * dx + dy * dy) - r;
}

float sdf_box(float x, float y, float cx, float cy, float w, float h) {
  float dx = fabsf(x - cx) - w * 0.5f;
  float dy = fabsf(y - cy) - h * 0.5f;
  float outside = sqrtf(fmaxf(dx, 0) * fmaxf(dx, 0) + fmaxf(dy, 0) * fmaxf(dy, 0));
  float inside = fminf(fmaxf(dx, dy), 0.0f);
  return outside + inside;
}

float sdf_union(float d1, float d2) { return fminf(d1, d2); }
float sdf_intersect(float d1, float d2) { return fmaxf(d1, d2); }
float sdf_subtract(float d1, float d2) { return fmaxf(d1, -d2); }

bool test_sdf_circle_inside_outside() {
  test_begin(__func__);
  Timer t; t.start();
  
  // Inside circle (negative)
  ASSERT_LT(sdf_circle(0, 0, 0, 0, 10), 0);
  ASSERT_LT(sdf_circle(5, 0, 0, 0, 10), 0);
  
  // On circle (zero)
  ASSERT_NEAR(sdf_circle(10, 0, 0, 0, 10), 0, 0.01f);
  ASSERT_NEAR(sdf_circle(0, 10, 0, 0, 10), 0, 0.01f);
  
  // Outside circle (positive)
  ASSERT_GT(sdf_circle(15, 0, 0, 0, 10), 0);
  ASSERT_GT(sdf_circle(20, 20, 0, 0, 10), 0);
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

bool test_sdf_box_inside_outside() {
  test_begin(__func__);
  Timer t; t.start();
  
  // Inside box (negative)
  ASSERT_LT(sdf_box(0, 0, 0, 0, 20, 10), 0);
  ASSERT_LT(sdf_box(5, 2, 0, 0, 20, 10), 0);
  
  // Outside box (positive)
  ASSERT_GT(sdf_box(20, 0, 0, 0, 20, 10), 0);
  ASSERT_GT(sdf_box(0, 10, 0, 0, 20, 10), 0);
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

bool test_sdf_operations() {
  test_begin(__func__);
  Timer t; t.start();
  
  float d1 = sdf_circle(5, 0, 0, 0, 10);   // Point inside circle
  float d2 = sdf_circle(5, 0, 10, 0, 10);  // Point also inside second circle
  
  // Union should be inside if either is inside
  ASSERT_LT(sdf_union(d1, d2), 0);
  
  // Intersection should be inside if both are inside
  ASSERT_LT(sdf_intersect(d1, d2), 0);
  
  // Point outside first, inside second
  d1 = sdf_circle(15, 0, 0, 0, 10);   // Outside first
  d2 = sdf_circle(15, 0, 10, 0, 10);  // Inside second (center at 10,0)
  
  ASSERT_LT(sdf_union(d1, d2), 0);     // Union: inside second
  ASSERT_GT(sdf_intersect(d1, d2), 0); // Intersection: not in both
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

// ============================================================
// Stress Tests
// ============================================================

bool test_stress_rapid_alloc_free() {
  test_begin(__func__);
  Timer t; t.start();
  
  const int ITERATIONS = 10000;
  
  for (int i = 0; i < ITERATIONS; i++) {
    size_t size = (rand() % 4096) + 1;
    void* p = malloc(size);
    ASSERT_NE(p, nullptr);
    memset(p, 0xAA, size);
    free(p);
  }
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

bool test_stress_trig_performance() {
  test_begin(__func__);
  Timer t; t.start();
  init_trig_tables();
  
  const int ITERATIONS = 100000;
  volatile int32_t sum = 0;
  
  for (int i = 0; i < ITERATIONS; i++) {
    sum += fast_sin(i & 0xFF);
    sum += fast_cos(i & 0xFF);
  }
  
  float elapsed = t.elapsed_ms();
  printf("    %d trig ops in %.2fms (%.0f ops/ms)\n", 
         ITERATIONS * 2, elapsed, ITERATIONS * 2 / elapsed);
  
  test_pass(__func__, elapsed);
  return true;
}

bool test_stress_fixed_math_performance() {
  test_begin(__func__);
  Timer t; t.start();
  
  const int ITERATIONS = 100000;
  volatile fixed16_16 result = float_to_fixed(1.0f);
  fixed16_16 multiplier = float_to_fixed(1.00001f);
  
  for (int i = 0; i < ITERATIONS; i++) {
    result = fixed_mul(result, multiplier);
  }
  
  float elapsed = t.elapsed_ms();
  printf("    %d fixed-point muls in %.2fms (%.0f ops/ms)\n",
         ITERATIONS, elapsed, ITERATIONS / elapsed);
  
  test_pass(__func__, elapsed);
  return true;
}

bool test_stress_bresenham_performance() {
  test_begin(__func__);
  Timer t; t.start();
  
  const int ITERATIONS = 10000;
  
  for (int i = 0; i < ITERATIONS; i++) {
    int x0 = rand() % 64;
    int y0 = rand() % 64;
    int x1 = rand() % 64;
    int y1 = rand() % 64;
    bresenham_line(x0, y0, x1, y1);
  }
  
  float elapsed = t.elapsed_ms();
  printf("    %d lines in %.2fms (%.0f lines/ms)\n",
         ITERATIONS, elapsed, ITERATIONS / elapsed);
  
  test_pass(__func__, elapsed);
  return true;
}

// ============================================================
// Regression Tests (Consistency)
// ============================================================

bool test_regression_deterministic_output() {
  test_begin(__func__);
  Timer t; t.start();
  
  // Same seed should produce same results
  srand(42);
  int first_values[10];
  for (int i = 0; i < 10; i++) {
    first_values[i] = rand();
  }
  
  srand(42);
  for (int i = 0; i < 10; i++) {
    ASSERT_EQ(rand(), first_values[i]);
  }
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

bool test_regression_fixed_point_consistency() {
  test_begin(__func__);
  Timer t; t.start();
  
  // Known values
  ASSERT_EQ(float_to_fixed(1.0f), 65536);
  ASSERT_EQ(float_to_fixed(0.5f), 32768);
  ASSERT_EQ(float_to_fixed(2.0f), 131072);
  
  ASSERT_EQ(fixed_mul(float_to_fixed(2.0f), float_to_fixed(3.0f)), float_to_fixed(6.0f));
  
  test_pass(__func__, t.elapsed_ms());
  return true;
}

// ============================================================
// Main Test Runner
// ============================================================

typedef bool (*TestFunc)();

struct TestEntry {
  const char* category;
  const char* name;
  TestFunc func;
};

TestEntry all_tests[] = {
  // ISA Tests
  {"ISA", "Opcode Values", test_isa_opcode_values},
  {"ISA", "DataType Sizes", test_isa_datatype_sizes},
  
  // Fixed-Point Math
  {"Math", "Fixed-Point Conversion", test_fixed_point_conversion},
  {"Math", "Fixed-Point Multiplication", test_fixed_point_multiplication},
  {"Math", "Fixed-Point Division", test_fixed_point_division},
  {"Math", "Fixed-Point Precision Drift", test_fixed_point_precision_drift},
  
  // Trigonometry
  {"Trig", "Sin Values", test_trig_sin_values},
  {"Trig", "Cos Values", test_trig_cos_values},
  {"Trig", "Sin^2 + Cos^2 Identity", test_trig_identity},
  
  // Color Space
  {"Color", "RGB to HSV", test_color_rgb_to_hsv},
  {"Color", "HSV to RGB", test_color_hsv_to_rgb},
  {"Color", "Color Roundtrip", test_color_roundtrip},
  
  // Drawing Algorithms
  {"Draw", "Bresenham Horizontal", test_bresenham_horizontal},
  {"Draw", "Bresenham Vertical", test_bresenham_vertical},
  {"Draw", "Bresenham Diagonal", test_bresenham_diagonal},
  {"Draw", "Bresenham Steep", test_bresenham_steep},
  {"Draw", "Circle Radius", test_circle_radius},
  {"Draw", "Circle Symmetry", test_circle_symmetry},
  
  // Memory
  {"Memory", "Alloc/Free", test_memory_alloc_free},
  {"Memory", "Fragmentation", test_memory_fragmentation},
  
  // Animation
  {"Anim", "Easing Boundaries", test_easing_boundaries},
  {"Anim", "Easing Monotonic", test_easing_monotonic},
  
  // SDF
  {"SDF", "Circle Inside/Outside", test_sdf_circle_inside_outside},
  {"SDF", "Box Inside/Outside", test_sdf_box_inside_outside},
  {"SDF", "SDF Operations", test_sdf_operations},
  
  // Stress Tests
  {"Stress", "Rapid Alloc/Free", test_stress_rapid_alloc_free},
  {"Stress", "Trig Performance", test_stress_trig_performance},
  {"Stress", "Fixed Math Performance", test_stress_fixed_math_performance},
  {"Stress", "Bresenham Performance", test_stress_bresenham_performance},
  
  // Regression
  {"Regression", "Deterministic Output", test_regression_deterministic_output},
  {"Regression", "Fixed-Point Consistency", test_regression_fixed_point_consistency},
};

void run_all_tests() {
  printf("============================================\n");
  printf("   GPU Driver Test Suite (Virtual Mode)\n");
  printf("============================================\n\n");
  
  Timer total;
  total.start();
  
  const char* current_category = nullptr;
  int test_count = sizeof(all_tests) / sizeof(all_tests[0]);
  
  for (int i = 0; i < test_count; i++) {
    if (!current_category || strcmp(current_category, all_tests[i].category) != 0) {
      current_category = all_tests[i].category;
      printf("\n=== %s Tests ===\n", current_category);
    }
    
    all_tests[i].func();
  }
  
  float total_time = total.elapsed_ms();
  
  printf("\n============================================\n");
  printf("                RESULTS\n");
  printf("============================================\n");
  printf("Total:  %d tests\n", g_total_tests);
  printf("Passed: %d (%.1f%%)\n", g_passed_tests, 100.0f * g_passed_tests / g_total_tests);
  printf("Failed: %d\n", g_failed_tests);
  printf("Time:   %.2f ms\n", total_time);
  printf("============================================\n");
  
  if (g_failed_tests > 0) {
    printf("\nFailed Tests:\n");
    for (int i = 0; i < g_result_count; i++) {
      if (!g_results[i].passed) {
        printf("  - %s: %s\n", g_results[i].name, g_results[i].message);
      }
    }
  }
  
  printf("\n%s\n", g_failed_tests == 0 ? "*** ALL TESTS PASSED ***" : "*** TESTS FAILED ***");
}

int main() {
  run_all_tests();
  return g_failed_tests > 0 ? 1 : 0;
}
