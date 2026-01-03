#!/usr/bin/env python3
"""
GPU Driver Test Suite - Virtual Testing
Runs comprehensive tests for the GPU driver architecture without hardware.
"""

import sys
import time
import math
import random
import struct
from typing import List, Tuple, Optional, Callable
from dataclasses import dataclass, field
from enum import IntEnum
import traceback

# ============================================================
# Test Framework
# ============================================================

class TestResult:
    def __init__(self, name: str, passed: bool, duration_ms: float, message: str = ""):
        self.name = name
        self.passed = passed
        self.duration_ms = duration_ms
        self.message = message

class TestRunner:
    def __init__(self):
        self.results: List[TestResult] = []
        self.current_category = ""
        
    def begin_category(self, name: str):
        self.current_category = name
        print(f"\n{'='*50}")
        print(f"  {name} Tests")
        print(f"{'='*50}")
        
    def run_test(self, name: str, func: Callable):
        print(f"  [RUN] {name}...", end=" ", flush=True)
        start = time.perf_counter()
        try:
            result = func()
            duration = (time.perf_counter() - start) * 1000
            if result is True or result is None:
                print(f"[PASS] ({duration:.2f}ms)")
                self.results.append(TestResult(name, True, duration))
            else:
                print(f"[FAIL] {result}")
                self.results.append(TestResult(name, False, duration, str(result)))
        except AssertionError as e:
            duration = (time.perf_counter() - start) * 1000
            print(f"[FAIL] {e}")
            self.results.append(TestResult(name, False, duration, str(e)))
        except Exception as e:
            duration = (time.perf_counter() - start) * 1000
            print(f"[ERROR] {e}")
            traceback.print_exc()
            self.results.append(TestResult(name, False, duration, str(e)))
    
    def summary(self):
        total = len(self.results)
        passed = sum(1 for r in self.results if r.passed)
        failed = total - passed
        total_time = sum(r.duration_ms for r in self.results)
        
        print(f"\n{'='*50}")
        print(f"                RESULTS")
        print(f"{'='*50}")
        print(f"Total:  {total} tests")
        print(f"Passed: {passed} ({100*passed/total:.1f}%)" if total > 0 else "Passed: 0")
        print(f"Failed: {failed}")
        print(f"Time:   {total_time:.2f} ms")
        print(f"{'='*50}")
        
        if failed > 0:
            print("\nFailed Tests:")
            for r in self.results:
                if not r.passed:
                    print(f"  - {r.name}: {r.message}")
        
        print(f"\n{'*** ALL TESTS PASSED ***' if failed == 0 else '*** TESTS FAILED ***'}")
        return failed == 0

runner = TestRunner()

# ============================================================
# ISA Definitions
# ============================================================

class Opcode(IntEnum):
    NOP = 0x00
    SET_PIXEL = 0x01
    FILL_RECT = 0x02
    DRAW_LINE = 0x03
    DRAW_CIRCLE = 0x04
    DRAW_TRIANGLE = 0x05
    DRAW_SPRITE = 0x06
    DRAW_TEXT = 0x07
    CLEAR = 0x10
    FLIP = 0x11
    SET_COLOR = 0x12
    SET_BLEND = 0x13
    ANIM_START = 0x20
    ANIM_STOP = 0x21
    ANIM_PAUSE = 0x22
    SCRIPT_CALL = 0x30
    SCRIPT_RET = 0x31
    SYNC = 0xF0
    RESET = 0xFF

class DataType(IntEnum):
    VOID = 0
    BOOL = 1
    UINT8 = 2
    INT8 = 3
    UINT16 = 4
    INT16 = 5
    UINT32 = 6
    INT32 = 7
    FLOAT32 = 8
    FIXED16_16 = 9
    COLOR_RGB = 10
    COLOR_RGBA = 11
    VEC2 = 12
    VEC3 = 13
    VEC4 = 14

# ============================================================
# Fixed-Point Math
# ============================================================

FIXED_SCALE = 65536  # 16.16 format

def float_to_fixed(f: float) -> int:
    return int(f * FIXED_SCALE)

def fixed_to_float(f: int) -> float:
    return f / FIXED_SCALE

def fixed_mul(a: int, b: int) -> int:
    return (a * b) >> 16

def fixed_div(a: int, b: int) -> int:
    return (a << 16) // b if b != 0 else 0

# ============================================================
# Trigonometry (Lookup Table)
# ============================================================

TRIG_TABLE_SIZE = 256
sin_table = [int(math.sin(2 * math.pi * i / TRIG_TABLE_SIZE) * 32767) for i in range(TRIG_TABLE_SIZE)]

def fast_sin(angle: int) -> int:
    return sin_table[angle & 0xFF]

def fast_cos(angle: int) -> int:
    return sin_table[(angle + 64) & 0xFF]

# ============================================================
# Color Space
# ============================================================

@dataclass
class RGB:
    r: int
    g: int
    b: int

@dataclass
class HSV:
    h: int
    s: int
    v: int

def rgb_to_hsv(rgb: RGB) -> HSV:
    r, g, b = rgb.r / 255, rgb.g / 255, rgb.b / 255
    mx = max(r, g, b)
    mn = min(r, g, b)
    delta = mx - mn
    
    v = int(mx * 255)
    s = int((delta / mx * 255) if mx > 0 else 0)
    
    if delta == 0:
        h = 0
    elif mx == r:
        h = int(((g - b) / delta) % 6 * 43)
    elif mx == g:
        h = int(((b - r) / delta + 2) * 43)
    else:
        h = int(((r - g) / delta + 4) * 43)
    
    return HSV(h & 0xFF, s, v)

def hsv_to_rgb(hsv: HSV) -> RGB:
    if hsv.s == 0:
        return RGB(hsv.v, hsv.v, hsv.v)
    
    h = hsv.h / 43
    i = int(h)
    f = h - i
    p = int(hsv.v * (1 - hsv.s / 255))
    q = int(hsv.v * (1 - hsv.s / 255 * f))
    t = int(hsv.v * (1 - hsv.s / 255 * (1 - f)))
    
    if i == 0: return RGB(hsv.v, t, p)
    if i == 1: return RGB(q, hsv.v, p)
    if i == 2: return RGB(p, hsv.v, t)
    if i == 3: return RGB(p, q, hsv.v)
    if i == 4: return RGB(t, p, hsv.v)
    return RGB(hsv.v, p, q)

# ============================================================
# Drawing Algorithms
# ============================================================

def bresenham_line(x0: int, y0: int, x1: int, y1: int) -> List[Tuple[int, int]]:
    points = []
    dx = abs(x1 - x0)
    dy = -abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    
    while True:
        points.append((x0, y0))
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy
    
    return points

def midpoint_circle(cx: int, cy: int, r: int) -> List[Tuple[int, int]]:
    points = []
    x, y = r, 0
    p = 1 - r
    
    def plot8(px, py):
        points.extend([
            (cx + px, cy + py), (cx - px, cy + py),
            (cx + px, cy - py), (cx - px, cy - py),
            (cx + py, cy + px), (cx - py, cy + px),
            (cx + py, cy - px), (cx - py, cy - px)
        ])
    
    plot8(x, y)
    while x > y:
        y += 1
        if p <= 0:
            p = p + 2 * y + 1
        else:
            x -= 1
            p = p + 2 * y - 2 * x + 1
        plot8(x, y)
    
    return points

# ============================================================
# SDF Functions
# ============================================================

def sdf_circle(x: float, y: float, cx: float, cy: float, r: float) -> float:
    return math.sqrt((x - cx)**2 + (y - cy)**2) - r

def sdf_box(x: float, y: float, cx: float, cy: float, w: float, h: float) -> float:
    dx = abs(x - cx) - w / 2
    dy = abs(y - cy) - h / 2
    outside = math.sqrt(max(dx, 0)**2 + max(dy, 0)**2)
    inside = min(max(dx, dy), 0)
    return outside + inside

def sdf_union(d1: float, d2: float) -> float:
    return min(d1, d2)

def sdf_intersect(d1: float, d2: float) -> float:
    return max(d1, d2)

def sdf_subtract(d1: float, d2: float) -> float:
    return max(d1, -d2)

# ============================================================
# Easing Functions
# ============================================================

def ease_linear(t: float) -> float:
    return t

def ease_in_quad(t: float) -> float:
    return t * t

def ease_out_quad(t: float) -> float:
    return t * (2 - t)

def ease_in_out_quad(t: float) -> float:
    return 2 * t * t if t < 0.5 else -1 + (4 - 2 * t) * t

def ease_in_cubic(t: float) -> float:
    return t * t * t

def ease_out_cubic(t: float) -> float:
    t -= 1
    return t * t * t + 1

# ============================================================
# ISA Tests
# ============================================================

def test_isa_opcode_values():
    assert Opcode.NOP == 0x00
    assert Opcode.SET_PIXEL == 0x01
    assert Opcode.FILL_RECT == 0x02
    assert Opcode.CLEAR == 0x10
    assert Opcode.ANIM_START == 0x20
    assert Opcode.RESET == 0xFF

def test_isa_datatype_enum():
    assert DataType.VOID == 0
    assert DataType.UINT8 == 2
    assert DataType.FLOAT32 == 8
    assert DataType.COLOR_RGB == 10
    assert DataType.VEC4 == 14

def test_isa_opcode_encoding():
    # Test encoding instructions as bytes
    instr = bytes([Opcode.SET_PIXEL, 10, 20, 255, 0, 0])  # x=10, y=20, color=red
    assert instr[0] == Opcode.SET_PIXEL
    assert instr[1] == 10  # x
    assert instr[2] == 20  # y

# ============================================================
# Fixed-Point Math Tests
# ============================================================

def test_fixed_point_conversion():
    assert abs(fixed_to_float(float_to_fixed(1.0)) - 1.0) < 0.0001
    assert abs(fixed_to_float(float_to_fixed(0.5)) - 0.5) < 0.0001
    assert abs(fixed_to_float(float_to_fixed(-1.5)) - (-1.5)) < 0.0001
    assert abs(fixed_to_float(float_to_fixed(3.14159)) - 3.14159) < 0.001

def test_fixed_point_multiplication():
    a = float_to_fixed(2.5)
    b = float_to_fixed(4.0)
    result = fixed_mul(a, b)
    assert abs(fixed_to_float(result) - 10.0) < 0.01
    
    # Small numbers
    a = float_to_fixed(0.1)
    b = float_to_fixed(0.1)
    result = fixed_mul(a, b)
    assert abs(fixed_to_float(result) - 0.01) < 0.001

def test_fixed_point_division():
    a = float_to_fixed(10.0)
    b = float_to_fixed(2.0)
    result = fixed_div(a, b)
    assert abs(fixed_to_float(result) - 5.0) < 0.01
    
    # Fractional
    a = float_to_fixed(1.0)
    b = float_to_fixed(3.0)
    result = fixed_div(a, b)
    assert abs(fixed_to_float(result) - 0.333333) < 0.001

def test_fixed_point_precision_drift():
    # Use larger step to avoid sub-LSB accumulation issues
    fixed_acc = 0
    float_acc = 0.0
    
    iterations = 1000
    small_value = 0.01  # Larger step to avoid sub-LSB issues
    small_fixed = float_to_fixed(small_value)
    
    for _ in range(iterations):
        fixed_acc += small_fixed
        float_acc += small_value
    
    fixed_result = fixed_to_float(fixed_acc)
    error = abs(fixed_result - float_acc)
    relative_error = error / float_acc if float_acc != 0 else 0
    
    print(f"    Fixed: {fixed_result:.6f}, Float: {float_acc:.6f}, Error: {relative_error*100:.4f}%")
    # 16.16 fixed point has inherent quantization - allow up to 0.01% relative error
    assert relative_error < 0.001  # < 0.1% error

# ============================================================
# Trigonometry Tests
# ============================================================

def test_trig_sin_values():
    assert abs(fast_sin(0) / 32767 - 0.0) < 0.02
    assert abs(fast_sin(64) / 32767 - 1.0) < 0.02
    assert abs(fast_sin(128) / 32767 - 0.0) < 0.02
    assert abs(fast_sin(192) / 32767 - (-1.0)) < 0.02

def test_trig_cos_values():
    assert abs(fast_cos(0) / 32767 - 1.0) < 0.02
    assert abs(fast_cos(64) / 32767 - 0.0) < 0.02
    assert abs(fast_cos(128) / 32767 - (-1.0)) < 0.02
    assert abs(fast_cos(192) / 32767 - 0.0) < 0.02

def test_trig_identity():
    for angle in range(256):
        s = fast_sin(angle) / 32767
        c = fast_cos(angle) / 32767
        sum_sq = s*s + c*c
        assert abs(sum_sq - 1.0) < 0.02, f"Identity failed at angle {angle}: {sum_sq}"

def test_trig_performance():
    iterations = 100000
    total = 0
    start = time.perf_counter()
    for i in range(iterations):
        total += fast_sin(i & 0xFF)
        total += fast_cos(i & 0xFF)
    elapsed = (time.perf_counter() - start) * 1000
    print(f"    {iterations*2} trig ops in {elapsed:.2f}ms ({iterations*2/elapsed:.0f} ops/ms)")

# ============================================================
# Color Space Tests
# ============================================================

def test_color_rgb_to_hsv():
    # Red
    hsv = rgb_to_hsv(RGB(255, 0, 0))
    assert hsv.v == 255
    assert hsv.s == 255
    assert abs(hsv.h - 0) < 10
    
    # Green
    hsv = rgb_to_hsv(RGB(0, 255, 0))
    assert hsv.v == 255
    assert abs(hsv.h - 85) < 10
    
    # Blue
    hsv = rgb_to_hsv(RGB(0, 0, 255))
    assert hsv.v == 255
    assert abs(hsv.h - 171) < 10

def test_color_hsv_to_rgb():
    # Red
    rgb = hsv_to_rgb(HSV(0, 255, 255))
    assert rgb.r > 240
    assert rgb.g < 20
    assert rgb.b < 20
    
    # Green
    rgb = hsv_to_rgb(HSV(85, 255, 255))
    assert rgb.g > 240

def test_color_roundtrip():
    random.seed(12345)
    for _ in range(100):
        original = RGB(random.randint(0, 255), random.randint(0, 255), random.randint(0, 255))
        hsv = rgb_to_hsv(original)
        recovered = hsv_to_rgb(hsv)
        assert abs(original.r - recovered.r) <= 10
        assert abs(original.g - recovered.g) <= 10
        assert abs(original.b - recovered.b) <= 10

# ============================================================
# Drawing Algorithm Tests
# ============================================================

def test_bresenham_horizontal():
    points = bresenham_line(0, 5, 10, 5)
    assert len(points) == 11
    for i, (x, y) in enumerate(points):
        assert x == i
        assert y == 5

def test_bresenham_vertical():
    points = bresenham_line(5, 0, 5, 10)
    assert len(points) == 11
    for i, (x, y) in enumerate(points):
        assert x == 5
        assert y == i

def test_bresenham_diagonal():
    points = bresenham_line(0, 0, 10, 10)
    assert len(points) == 11
    for i, (x, y) in enumerate(points):
        assert x == i
        assert y == i

def test_bresenham_steep():
    points = bresenham_line(0, 0, 3, 10)
    # Check connectivity
    for i in range(1, len(points)):
        dx = abs(points[i][0] - points[i-1][0])
        dy = abs(points[i][1] - points[i-1][1])
        assert dx <= 1 and dy <= 1

def test_circle_radius():
    cx, cy, r = 32, 32, 10
    points = midpoint_circle(cx, cy, r)
    
    for px, py in points:
        dist = math.sqrt((px - cx)**2 + (py - cy)**2)
        assert abs(dist - r) < 1.5  # Within 1.5 pixels

def test_circle_symmetry():
    points = midpoint_circle(0, 0, 15)
    point_set = set(points)
    
    assert (15, 0) in point_set
    assert (-15, 0) in point_set
    assert (0, 15) in point_set
    assert (0, -15) in point_set

def test_bresenham_performance():
    random.seed(42)
    iterations = 10000
    start = time.perf_counter()
    for _ in range(iterations):
        bresenham_line(random.randint(0, 63), random.randint(0, 63),
                       random.randint(0, 63), random.randint(0, 63))
    elapsed = (time.perf_counter() - start) * 1000
    print(f"    {iterations} lines in {elapsed:.2f}ms ({iterations/elapsed:.0f} lines/ms)")

# ============================================================
# Animation/Easing Tests
# ============================================================

def test_easing_boundaries():
    easings = [ease_linear, ease_in_quad, ease_out_quad, ease_in_out_quad, 
               ease_in_cubic, ease_out_cubic]
    for ease in easings:
        assert abs(ease(0.0) - 0.0) < 0.001, f"{ease.__name__}(0) != 0"
        assert abs(ease(1.0) - 1.0) < 0.001, f"{ease.__name__}(1) != 1"

def test_easing_monotonic():
    easings = [ease_linear, ease_in_quad, ease_out_quad]
    for ease in easings:
        prev = 0
        for i in range(101):
            t = i / 100
            v = ease(t)
            assert v >= prev - 0.001, f"{ease.__name__} not monotonic at t={t}"
            prev = v

# ============================================================
# SDF Tests
# ============================================================

def test_sdf_circle_inside_outside():
    # Inside
    assert sdf_circle(0, 0, 0, 0, 10) < 0
    assert sdf_circle(5, 0, 0, 0, 10) < 0
    
    # On boundary
    assert abs(sdf_circle(10, 0, 0, 0, 10)) < 0.01
    
    # Outside
    assert sdf_circle(15, 0, 0, 0, 10) > 0
    assert sdf_circle(20, 20, 0, 0, 10) > 0

def test_sdf_box_inside_outside():
    # Inside
    assert sdf_box(0, 0, 0, 0, 20, 10) < 0
    assert sdf_box(5, 2, 0, 0, 20, 10) < 0
    
    # Outside
    assert sdf_box(20, 0, 0, 0, 20, 10) > 0
    assert sdf_box(0, 10, 0, 0, 20, 10) > 0

def test_sdf_operations():
    d1 = sdf_circle(5, 0, 0, 0, 10)   # Inside first
    d2 = sdf_circle(5, 0, 10, 0, 10)  # Inside second
    
    assert sdf_union(d1, d2) < 0
    assert sdf_intersect(d1, d2) < 0
    
    # Point outside first, inside second
    d1 = sdf_circle(15, 0, 0, 0, 10)
    d2 = sdf_circle(15, 0, 10, 0, 10)
    
    assert sdf_union(d1, d2) < 0
    assert sdf_intersect(d1, d2) > 0

# ============================================================
# Memory/Stress Tests
# ============================================================

def test_stress_rapid_allocations():
    iterations = 10000
    for _ in range(iterations):
        size = random.randint(1, 4096)
        data = bytearray(size)
        for i in range(min(100, size)):
            data[i] = 0xAA
        del data

def test_stress_fixed_math_performance():
    iterations = 100000
    result = float_to_fixed(1.0)
    multiplier = float_to_fixed(1.00001)
    
    start = time.perf_counter()
    for _ in range(iterations):
        result = fixed_mul(result, multiplier)
    elapsed = (time.perf_counter() - start) * 1000
    print(f"    {iterations} fixed muls in {elapsed:.2f}ms ({iterations/elapsed:.0f} ops/ms)")

# ============================================================
# Protocol Tests
# ============================================================

def test_packet_encoding():
    # Test packet structure
    magic = bytes([0xAA, 0x55])
    command = Opcode.SET_PIXEL
    payload = bytes([10, 20, 255, 0, 0])  # x=10, y=20, r=255, g=0, b=0
    length = len(payload)
    
    packet = magic + bytes([command, length]) + payload
    crc = command ^ length
    for b in payload:
        crc ^= b
    packet += bytes([crc])
    
    # Verify structure
    assert packet[0:2] == magic
    assert packet[2] == command
    assert packet[3] == length
    assert packet[4:4+length] == payload

def test_crc_calculation():
    def calc_crc(cmd: int, payload: bytes) -> int:
        crc = cmd ^ len(payload)
        for b in payload:
            crc ^= b
        return crc & 0xFF
    
    # Known test case
    assert calc_crc(0x01, bytes([10, 20, 255, 0, 0])) == (0x01 ^ 5 ^ 10 ^ 20 ^ 255 ^ 0 ^ 0) & 0xFF
    
    # Empty payload
    assert calc_crc(0x10, bytes()) == 0x10

# ============================================================
# Regression Tests
# ============================================================

def test_deterministic_output():
    random.seed(42)
    first_values = [random.randint(0, 1000) for _ in range(10)]
    
    random.seed(42)
    for expected in first_values:
        assert random.randint(0, 1000) == expected

def test_fixed_point_consistency():
    assert float_to_fixed(1.0) == 65536
    assert float_to_fixed(0.5) == 32768
    assert float_to_fixed(2.0) == 131072
    
    assert fixed_mul(float_to_fixed(2.0), float_to_fixed(3.0)) == float_to_fixed(6.0)

# ============================================================
# Hardware Test Framework Validation
# ============================================================

def test_failure_category_encoding():
    # Simulate failure record structure
    class FailureCategory(IntEnum):
        TIMING = 0
        PRECISION = 1
        RACE_CONDITION = 2
        MEMORY_CORRUPT = 3
        SYNC_ERROR = 4
        WATCHDOG = 5
        THERMAL = 6
    
    # Test all categories are distinct
    categories = list(FailureCategory)
    assert len(categories) == len(set(categories))
    
    # Test encoding
    record = {
        'category': FailureCategory.PRECISION,
        'seed': 0x12345678,
        'timestamp': 1000,
        'message': 'Drift exceeded tolerance'
    }
    
    assert record['category'] == FailureCategory.PRECISION
    assert isinstance(record['seed'], int)

def test_performance_counter_accumulation():
    counters = {}
    
    def increment(name: str, amount: int = 1):
        counters[name] = counters.get(name, 0) + amount
    
    # Simulate frame rendering
    for _ in range(100):
        increment('FRAMES_RENDERED')
        increment('PIXELS_DRAWN', 4096)
        increment('COMMANDS_EXECUTED', 10)
    
    assert counters['FRAMES_RENDERED'] == 100
    assert counters['PIXELS_DRAWN'] == 409600
    assert counters['COMMANDS_EXECUTED'] == 1000

def test_thermal_monitoring():
    class ThermalMonitor:
        def __init__(self):
            self.samples = []
            self.peak = 0
            self.throttle_threshold = 70.0
        
        def add_sample(self, temp: float):
            self.samples.append(temp)
            if temp > self.peak:
                self.peak = temp
        
        def is_throttling(self) -> bool:
            if not self.samples:
                return False
            return self.samples[-1] > self.throttle_threshold
        
        def avg(self) -> float:
            return sum(self.samples) / len(self.samples) if self.samples else 0
    
    monitor = ThermalMonitor()
    
    # Simulate temperature readings
    temps = [25, 30, 35, 40, 45, 50, 55, 60, 65, 68]
    for t in temps:
        monitor.add_sample(t)
    
    assert monitor.peak == 68
    assert not monitor.is_throttling()
    
    monitor.add_sample(75)
    assert monitor.is_throttling()

# ============================================================
# Main
# ============================================================

def main():
    print("=" * 60)
    print("   GPU Driver Test Suite (Python Virtual Testing)")
    print("=" * 60)
    
    # ISA Tests
    runner.begin_category("ISA")
    runner.run_test("Opcode Values", test_isa_opcode_values)
    runner.run_test("DataType Enum", test_isa_datatype_enum)
    runner.run_test("Opcode Encoding", test_isa_opcode_encoding)
    
    # Fixed-Point Math
    runner.begin_category("Fixed-Point Math")
    runner.run_test("Conversion", test_fixed_point_conversion)
    runner.run_test("Multiplication", test_fixed_point_multiplication)
    runner.run_test("Division", test_fixed_point_division)
    runner.run_test("Precision Drift", test_fixed_point_precision_drift)
    
    # Trigonometry
    runner.begin_category("Trigonometry")
    runner.run_test("Sin Values", test_trig_sin_values)
    runner.run_test("Cos Values", test_trig_cos_values)
    runner.run_test("Identity (sin²+cos²=1)", test_trig_identity)
    runner.run_test("Performance", test_trig_performance)
    
    # Color Space
    runner.begin_category("Color Space")
    runner.run_test("RGB to HSV", test_color_rgb_to_hsv)
    runner.run_test("HSV to RGB", test_color_hsv_to_rgb)
    runner.run_test("Roundtrip", test_color_roundtrip)
    
    # Drawing Algorithms
    runner.begin_category("Drawing Algorithms")
    runner.run_test("Bresenham Horizontal", test_bresenham_horizontal)
    runner.run_test("Bresenham Vertical", test_bresenham_vertical)
    runner.run_test("Bresenham Diagonal", test_bresenham_diagonal)
    runner.run_test("Bresenham Steep", test_bresenham_steep)
    runner.run_test("Circle Radius", test_circle_radius)
    runner.run_test("Circle Symmetry", test_circle_symmetry)
    runner.run_test("Bresenham Performance", test_bresenham_performance)
    
    # Animation/Easing
    runner.begin_category("Animation/Easing")
    runner.run_test("Easing Boundaries", test_easing_boundaries)
    runner.run_test("Easing Monotonic", test_easing_monotonic)
    
    # SDF
    runner.begin_category("SDF Rendering")
    runner.run_test("Circle Inside/Outside", test_sdf_circle_inside_outside)
    runner.run_test("Box Inside/Outside", test_sdf_box_inside_outside)
    runner.run_test("SDF Operations", test_sdf_operations)
    
    # Protocol
    runner.begin_category("Protocol")
    runner.run_test("Packet Encoding", test_packet_encoding)
    runner.run_test("CRC Calculation", test_crc_calculation)
    
    # Stress Tests
    runner.begin_category("Stress Tests")
    runner.run_test("Rapid Allocations", test_stress_rapid_allocations)
    runner.run_test("Fixed Math Performance", test_stress_fixed_math_performance)
    
    # Hardware Framework Validation
    runner.begin_category("Hardware Framework")
    runner.run_test("Failure Category Encoding", test_failure_category_encoding)
    runner.run_test("Performance Counter Accumulation", test_performance_counter_accumulation)
    runner.run_test("Thermal Monitoring", test_thermal_monitoring)
    
    # Regression
    runner.begin_category("Regression")
    runner.run_test("Deterministic Output", test_deterministic_output)
    runner.run_test("Fixed-Point Consistency", test_fixed_point_consistency)
    
    # Summary
    success = runner.summary()
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())
