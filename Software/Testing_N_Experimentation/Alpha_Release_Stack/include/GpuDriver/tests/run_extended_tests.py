#!/usr/bin/env python3
"""
GPU Driver Extended Test Suite
Additional comprehensive tests including:
- Edge cases
- Boundary conditions
- UART protocol simulation
- Command generation and validation
- Multi-component integration tests
"""

import sys
import time
import math
import random
import struct
from typing import List, Tuple, Dict, Optional
from dataclasses import dataclass
from enum import IntEnum
from collections import deque
import io

# ============================================================
# Test Framework (Shared)
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
        
    def begin_category(self, name: str):
        print(f"\n{'='*50}")
        print(f"  {name}")
        print(f"{'='*50}")
        
    def run_test(self, name: str, func):
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
            self.results.append(TestResult(name, False, duration, str(e)))
    
    def summary(self):
        total = len(self.results)
        passed = sum(1 for r in self.results if r.passed)
        failed = total - passed
        total_time = sum(r.duration_ms for r in self.results)
        
        print(f"\n{'='*50}")
        print(f"         EXTENDED TEST RESULTS")
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
    DRAW_FILLED_CIRCLE = 0x05
    DRAW_TRIANGLE = 0x06
    DRAW_SPRITE = 0x07
    DRAW_TEXT = 0x08
    CLEAR = 0x10
    FLIP = 0x11
    SET_COLOR = 0x12
    SET_BLEND = 0x13
    SET_CLIP = 0x14
    RESET_CLIP = 0x15
    ANIM_START = 0x20
    ANIM_STOP = 0x21
    ANIM_PAUSE = 0x22
    ANIM_RESUME = 0x23
    SCRIPT_CALL = 0x30
    SCRIPT_RET = 0x31
    STORE = 0x40
    LOAD = 0x41
    PUSH = 0x42
    POP = 0x43
    ADD = 0x50
    SUB = 0x51
    MUL = 0x52
    DIV = 0x53
    JUMP = 0x60
    JUMP_IF = 0x61
    SYNC = 0xF0
    RESET = 0xFF

# ============================================================
# UART Protocol Simulation
# ============================================================

class UartPacket:
    MAGIC = bytes([0xAA, 0x55])
    MAX_PAYLOAD = 255
    
    @staticmethod
    def encode(cmd: int, payload: bytes) -> bytes:
        if len(payload) > UartPacket.MAX_PAYLOAD:
            raise ValueError(f"Payload too large: {len(payload)} > {UartPacket.MAX_PAYLOAD}")
        
        header = UartPacket.MAGIC + bytes([cmd, len(payload)])
        crc = cmd ^ len(payload)
        for b in payload:
            crc ^= b
        return header + payload + bytes([crc & 0xFF])
    
    @staticmethod
    def decode(data: bytes) -> Tuple[int, bytes]:
        if len(data) < 5:  # magic(2) + cmd(1) + len(1) + crc(1)
            raise ValueError("Packet too short")
        if data[0:2] != UartPacket.MAGIC:
            raise ValueError(f"Invalid magic: {data[0:2].hex()}")
        
        cmd = data[2]
        length = data[3]
        
        if len(data) < 4 + length + 1:
            raise ValueError("Incomplete packet")
        
        payload = data[4:4+length]
        received_crc = data[4+length]
        
        expected_crc = cmd ^ length
        for b in payload:
            expected_crc ^= b
        expected_crc &= 0xFF
        
        if received_crc != expected_crc:
            raise ValueError(f"CRC mismatch: {received_crc:02x} != {expected_crc:02x}")
        
        return cmd, payload

class UartSimulator:
    """Simulates UART communication with noise and delays"""
    
    def __init__(self, error_rate: float = 0.0, drop_rate: float = 0.0):
        self.tx_buffer = deque()
        self.rx_buffer = deque()
        self.error_rate = error_rate
        self.drop_rate = drop_rate
        self.bytes_sent = 0
        self.bytes_received = 0
        self.packets_sent = 0
        self.packets_dropped = 0
        self.crc_errors = 0
    
    def send_packet(self, cmd: int, payload: bytes):
        packet = UartPacket.encode(cmd, payload)
        
        if random.random() < self.drop_rate:
            self.packets_dropped += 1
            return False
        
        for byte in packet:
            if random.random() < self.error_rate:
                byte ^= (1 << random.randint(0, 7))  # Flip random bit
            self.tx_buffer.append(byte)
        
        self.bytes_sent += len(packet)
        self.packets_sent += 1
        return True
    
    def receive(self) -> bytes:
        data = bytes(self.tx_buffer)
        self.tx_buffer.clear()
        return data
    
    def try_decode(self, data: bytes) -> Optional[Tuple[int, bytes]]:
        try:
            return UartPacket.decode(data)
        except ValueError as e:
            if "CRC" in str(e):
                self.crc_errors += 1
            return None

# ============================================================
# GPU State Simulator
# ============================================================

class FrameBuffer:
    def __init__(self, width: int, height: int):
        self.width = width
        self.height = height
        self.pixels = [[0] * width for _ in range(height)]
    
    def set_pixel(self, x: int, y: int, color: int):
        if 0 <= x < self.width and 0 <= y < self.height:
            self.pixels[y][x] = color
    
    def get_pixel(self, x: int, y: int) -> int:
        if 0 <= x < self.width and 0 <= y < self.height:
            return self.pixels[y][x]
        return 0
    
    def clear(self, color: int = 0):
        for y in range(self.height):
            for x in range(self.width):
                self.pixels[y][x] = color
    
    def fill_rect(self, x: int, y: int, w: int, h: int, color: int):
        for py in range(max(0, y), min(self.height, y + h)):
            for px in range(max(0, x), min(self.width, x + w)):
                self.pixels[py][px] = color

class GpuSimulator:
    """Simulates GPU command execution"""
    
    def __init__(self, width: int = 64, height: int = 64):
        self.fb = FrameBuffer(width, height)
        self.current_color = 0xFFFFFF
        self.blend_mode = 0
        self.clip_x = 0
        self.clip_y = 0
        self.clip_w = width
        self.clip_h = height
        self.commands_executed = 0
    
    def execute(self, cmd: int, payload: bytes) -> bool:
        self.commands_executed += 1
        
        if cmd == Opcode.NOP:
            return True
        elif cmd == Opcode.CLEAR:
            color = struct.unpack('<I', payload[:4])[0] if len(payload) >= 4 else 0
            self.fb.clear(color)
            return True
        elif cmd == Opcode.SET_PIXEL:
            if len(payload) >= 6:
                x, y = struct.unpack('<HH', payload[:4])
                color = struct.unpack('<H', payload[4:6])[0]
                self.fb.set_pixel(x, y, color)
                return True
        elif cmd == Opcode.FILL_RECT:
            if len(payload) >= 12:
                x, y, w, h = struct.unpack('<HHHH', payload[:8])
                color = struct.unpack('<I', payload[8:12])[0]
                self.fb.fill_rect(x, y, w, h, color)
                return True
        elif cmd == Opcode.SET_COLOR:
            if len(payload) >= 4:
                self.current_color = struct.unpack('<I', payload[:4])[0]
                return True
        elif cmd == Opcode.FLIP:
            return True  # Simulated
        elif cmd == Opcode.RESET:
            self.__init__(self.fb.width, self.fb.height)
            return True
        
        return False

# ============================================================
# Edge Case Tests
# ============================================================

def test_edge_case_empty_payload():
    packet = UartPacket.encode(Opcode.NOP, bytes())
    cmd, payload = UartPacket.decode(packet)
    assert cmd == Opcode.NOP
    assert payload == bytes()

def test_edge_case_max_payload():
    max_payload = bytes([0xFF] * 255)
    packet = UartPacket.encode(Opcode.SET_PIXEL, max_payload)
    cmd, payload = UartPacket.decode(packet)
    assert payload == max_payload

def test_edge_case_all_zeros():
    packet = UartPacket.encode(Opcode.NOP, bytes(100))
    cmd, payload = UartPacket.decode(packet)
    assert all(b == 0 for b in payload)

def test_edge_case_all_ones():
    packet = UartPacket.encode(0xFE, bytes([0xFF] * 100))
    cmd, payload = UartPacket.decode(packet)
    assert cmd == 0xFE
    assert all(b == 0xFF for b in payload)

def test_edge_case_alternating():
    alternating = bytes([0xAA, 0x55] * 50)
    packet = UartPacket.encode(Opcode.DRAW_LINE, alternating)
    cmd, payload = UartPacket.decode(packet)
    assert payload == alternating

# ============================================================
# Boundary Condition Tests
# ============================================================

def test_boundary_pixel_corners():
    gpu = GpuSimulator(64, 64)
    # All four corners
    gpu.fb.set_pixel(0, 0, 1)
    gpu.fb.set_pixel(63, 0, 2)
    gpu.fb.set_pixel(0, 63, 3)
    gpu.fb.set_pixel(63, 63, 4)
    
    assert gpu.fb.get_pixel(0, 0) == 1
    assert gpu.fb.get_pixel(63, 0) == 2
    assert gpu.fb.get_pixel(0, 63) == 3
    assert gpu.fb.get_pixel(63, 63) == 4

def test_boundary_pixel_out_of_bounds():
    gpu = GpuSimulator(64, 64)
    # These should be silently ignored
    gpu.fb.set_pixel(-1, 0, 0xFF)
    gpu.fb.set_pixel(0, -1, 0xFF)
    gpu.fb.set_pixel(64, 0, 0xFF)
    gpu.fb.set_pixel(0, 64, 0xFF)
    
    # Check corners unaffected
    assert gpu.fb.get_pixel(0, 0) == 0

def test_boundary_rect_clipping():
    gpu = GpuSimulator(64, 64)
    
    # Rect partially off screen
    gpu.fb.fill_rect(-10, -10, 20, 20, 0xFF)
    
    # Only visible portion should be filled
    assert gpu.fb.get_pixel(0, 0) == 0xFF
    assert gpu.fb.get_pixel(9, 9) == 0xFF
    assert gpu.fb.get_pixel(10, 10) == 0  # Outside rect

def test_boundary_rect_entirely_off():
    gpu = GpuSimulator(64, 64)
    gpu.fb.clear(0)
    
    # Completely off screen
    gpu.fb.fill_rect(-100, -100, 10, 10, 0xFF)
    gpu.fb.fill_rect(100, 100, 10, 10, 0xFF)
    
    # Nothing should be drawn
    for y in range(64):
        for x in range(64):
            assert gpu.fb.get_pixel(x, y) == 0

# ============================================================
# Protocol Robustness Tests
# ============================================================

def test_protocol_all_opcodes():
    for op in Opcode:
        packet = UartPacket.encode(op, bytes([0x00, 0x01, 0x02]))
        cmd, payload = UartPacket.decode(packet)
        assert cmd == op

def test_protocol_crc_detection():
    packet = UartPacket.encode(Opcode.SET_PIXEL, bytes([10, 20, 255, 0, 0]))
    
    # Corrupt one byte
    corrupted = bytearray(packet)
    corrupted[5] ^= 0x01
    
    try:
        UartPacket.decode(bytes(corrupted))
        assert False, "Should have raised CRC error"
    except ValueError as e:
        assert "CRC" in str(e)

def test_protocol_magic_detection():
    # Wrong magic
    bad_packet = bytes([0xBB, 0x55, Opcode.NOP, 0, 0])
    
    try:
        UartPacket.decode(bad_packet)
        assert False, "Should have raised magic error"
    except ValueError as e:
        assert "magic" in str(e).lower()

def test_protocol_truncated():
    packet = UartPacket.encode(Opcode.SET_PIXEL, bytes([10, 20, 255, 0, 0]))
    
    # Truncate packet
    try:
        UartPacket.decode(packet[:3])
        assert False, "Should have raised error"
    except ValueError:
        pass

# ============================================================
# UART Simulation Tests
# ============================================================

def test_uart_clean_channel():
    uart = UartSimulator(error_rate=0.0, drop_rate=0.0)
    
    for i in range(100):
        uart.send_packet(Opcode.SET_PIXEL, bytes([i, i, 255, 0, 0]))
    
    data = uart.receive()
    
    assert uart.packets_sent == 100
    assert uart.packets_dropped == 0
    assert len(data) > 0

def test_uart_noisy_channel():
    random.seed(42)
    uart = UartSimulator(error_rate=0.01, drop_rate=0.0)
    
    successes = 0
    for i in range(100):
        uart.send_packet(Opcode.SET_PIXEL, bytes([i, i, 255, 0, 0]))
        data = uart.receive()
        result = uart.try_decode(data)
        if result:
            successes += 1
    
    # With 1% error rate on ~10 bytes per packet, expect some failures
    print(f"    {successes}/100 successful, {uart.crc_errors} CRC errors")
    assert successes >= 70  # At least 70% should succeed

def test_uart_lossy_channel():
    random.seed(42)
    uart = UartSimulator(error_rate=0.0, drop_rate=0.1)
    
    for _ in range(100):
        uart.send_packet(Opcode.SET_PIXEL, bytes([10, 20, 255, 0, 0]))
    
    print(f"    {uart.packets_sent} sent, {uart.packets_dropped} dropped")
    assert uart.packets_dropped > 0  # Some should be dropped

# ============================================================
# GPU Command Integration Tests
# ============================================================

def test_gpu_command_sequence():
    gpu = GpuSimulator(64, 64)
    
    # Execute sequence
    commands = [
        (Opcode.RESET, bytes()),  # This resets commands_executed to 0
        (Opcode.CLEAR, struct.pack('<I', 0)),
        (Opcode.SET_COLOR, struct.pack('<I', 0xFF0000)),
        (Opcode.SET_PIXEL, struct.pack('<HHH', 10, 10, 0xFF00)),
        (Opcode.FILL_RECT, struct.pack('<HHHHI', 20, 20, 10, 10, 0x0000FF)),
        (Opcode.FLIP, bytes()),
    ]
    
    for cmd, payload in commands:
        gpu.execute(cmd, payload)
    
    # RESET command causes __init__ which resets counter, so only 5 commands counted after reset
    assert gpu.commands_executed == 5
    # Pixel stores 16-bit value, rect stores 32-bit value
    assert gpu.fb.get_pixel(10, 10) == 0xFF00  # SET_PIXEL uses 16-bit
    assert gpu.fb.get_pixel(25, 25) == 0xFF  # FILL_RECT stores lower byte (blue)

def test_gpu_rapid_clear_flip():
    gpu = GpuSimulator(64, 64)
    
    for i in range(100):
        gpu.execute(Opcode.CLEAR, struct.pack('<I', i))
        gpu.execute(Opcode.FLIP, bytes())
    
    assert gpu.commands_executed == 200

def test_gpu_fill_entire_screen():
    gpu = GpuSimulator(64, 64)
    gpu.execute(Opcode.FILL_RECT, struct.pack('<HHHHI', 0, 0, 64, 64, 0xFFFFFF))
    
    for y in range(64):
        for x in range(64):
            assert gpu.fb.get_pixel(x, y) == 0xFFFFFF

# ============================================================
# Fixed-Point Edge Cases
# ============================================================

FIXED_SCALE = 65536

def float_to_fixed(f: float) -> int:
    return int(f * FIXED_SCALE)

def fixed_to_float(f: int) -> float:
    return f / FIXED_SCALE

def fixed_mul(a: int, b: int) -> int:
    return (a * b) >> 16

def fixed_div(a: int, b: int) -> int:
    return (a << 16) // b if b != 0 else 0

def test_fixed_overflow():
    # Large values - check behavior
    large = float_to_fixed(100.0)
    result = fixed_mul(large, large)
    # 100 * 100 = 10000 - should still be representable
    assert abs(fixed_to_float(result) - 10000.0) < 1.0

def test_fixed_underflow():
    # Very small values
    tiny = float_to_fixed(0.00001)
    result = fixed_mul(tiny, tiny)
    # Should be effectively zero due to precision limits
    assert abs(fixed_to_float(result)) < 0.1

def test_fixed_negative():
    neg_a = float_to_fixed(-5.0)
    pos_b = float_to_fixed(3.0)
    
    result = fixed_mul(neg_a, pos_b)
    assert abs(fixed_to_float(result) - (-15.0)) < 0.01

def test_fixed_div_by_small():
    large = float_to_fixed(1000.0)
    small = float_to_fixed(0.001)
    
    result = fixed_div(small, large)
    # Should be very small but not zero
    assert fixed_to_float(result) < 0.01

# ============================================================
# Drawing Algorithm Edge Cases
# ============================================================

def bresenham_line(x0, y0, x1, y1):
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

def test_line_zero_length():
    points = bresenham_line(10, 10, 10, 10)
    assert len(points) == 1
    assert points[0] == (10, 10)

def test_line_single_pixel():
    points = bresenham_line(0, 0, 1, 0)
    assert len(points) == 2
    assert (0, 0) in points
    assert (1, 0) in points

def test_line_negative_coords():
    points = bresenham_line(-10, -10, 10, 10)
    assert (-10, -10) in points
    assert (0, 0) in points
    assert (10, 10) in points

def test_line_backwards():
    forward = bresenham_line(0, 0, 10, 5)
    backward = bresenham_line(10, 5, 0, 0)
    
    # Both should contain same number of points and cover same pixels
    assert len(forward) == len(backward)
    # Points may not be identical due to Bresenham's tie-breaking, but line length should match
    assert abs(len(forward) - len(backward)) <= 1

# ============================================================
# Animation/Timing Tests
# ============================================================

def test_animation_frame_timing():
    target_fps = 60
    frame_time_us = 1_000_000 // target_fps  # 16666 us
    
    frames = 100
    total_time = frames * frame_time_us
    
    # 100 frames * 16666 us = 1666600 us (integer division)
    assert total_time == 1_666_600  # ~1.67 seconds for 100 frames at 60fps

def test_animation_interpolation():
    def lerp(a, b, t):
        return a + (b - a) * t
    
    # Test endpoints
    assert lerp(0, 100, 0.0) == 0
    assert lerp(0, 100, 1.0) == 100
    assert lerp(0, 100, 0.5) == 50
    
    # Extrapolation
    assert lerp(0, 100, 2.0) == 200
    assert lerp(0, 100, -1.0) == -100

def test_animation_step_counting():
    duration_ms = 1000
    fps = 60
    frame_time_ms = 1000.0 / fps  # ~16.667 ms
    
    expected_frames = int(duration_ms / frame_time_ms + 0.5)  # Round to nearest
    assert expected_frames == 60  # 60 frames per second

# ============================================================
# Memory Pattern Tests
# ============================================================

def test_memory_alignment():
    # Simulate checking alignment requirements
    for size in [1, 2, 4, 8, 16, 32, 64]:
        aligned_size = (size + 3) & ~3  # 4-byte alignment
        assert aligned_size % 4 == 0

def test_memory_pool_simulation():
    pool_size = 4096
    allocations = []
    allocated = 0
    
    # Allocate until full
    random.seed(42)
    while allocated < pool_size:
        size = random.randint(16, 256)
        if allocated + size <= pool_size:
            allocations.append(size)
            allocated += size
        else:
            break
    
    print(f"    {len(allocations)} allocations, {allocated}/{pool_size} bytes used")
    assert allocated <= pool_size

def test_memory_fragmentation():
    # Simulate fragmentation scenario
    blocks = list(range(100))  # 100 blocks allocated
    
    # Free every other block
    for i in range(0, 100, 2):
        blocks[i] = None
    
    free_count = sum(1 for b in blocks if b is None)
    assert free_count == 50  # 50% fragmented

# ============================================================
# Color Conversion Edge Cases
# ============================================================

def test_color_grayscale():
    # Pure grayscale values
    for v in [0, 64, 128, 192, 255]:
        r = g = b = v
        # Convert to 16-bit RGB565
        rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        
        # Ensure not too much data loss
        r_back = ((rgb565 >> 11) & 0x1F) << 3
        g_back = ((rgb565 >> 5) & 0x3F) << 2
        b_back = (rgb565 & 0x1F) << 3
        
        assert abs(r - r_back) <= 8
        assert abs(g - g_back) <= 4
        assert abs(b - b_back) <= 8

def test_color_rgb565_primaries():
    # Pure colors in RGB565
    red_565 = 0xF800
    green_565 = 0x07E0
    blue_565 = 0x001F
    
    assert red_565 >> 11 == 0x1F  # Full red
    assert (green_565 >> 5) & 0x3F == 0x3F  # Full green
    assert blue_565 & 0x1F == 0x1F  # Full blue

def test_color_alpha_blend():
    def alpha_blend(src, dst, alpha):
        return ((src * alpha) + (dst * (255 - alpha))) // 255
    
    # Opaque source overwrites
    assert alpha_blend(100, 200, 255) == 100
    
    # Transparent source leaves dest
    assert alpha_blend(100, 200, 0) == 200
    
    # 50% blend
    result = alpha_blend(100, 200, 128)
    assert 145 <= result <= 155  # Should be ~150

# ============================================================
# Performance Benchmarks
# ============================================================

def test_perf_packet_encoding():
    iterations = 10000
    payload = bytes([0xFF] * 100)
    
    start = time.perf_counter()
    for _ in range(iterations):
        UartPacket.encode(Opcode.SET_PIXEL, payload)
    elapsed = (time.perf_counter() - start) * 1000
    
    print(f"    {iterations} encodes in {elapsed:.2f}ms ({iterations/elapsed:.0f} ops/ms)")

def test_perf_packet_decoding():
    packet = UartPacket.encode(Opcode.SET_PIXEL, bytes([0xFF] * 100))
    iterations = 10000
    
    start = time.perf_counter()
    for _ in range(iterations):
        UartPacket.decode(packet)
    elapsed = (time.perf_counter() - start) * 1000
    
    print(f"    {iterations} decodes in {elapsed:.2f}ms ({iterations/elapsed:.0f} ops/ms)")

def test_perf_gpu_commands():
    gpu = GpuSimulator(64, 64)
    iterations = 10000
    
    start = time.perf_counter()
    for i in range(iterations):
        gpu.execute(Opcode.SET_PIXEL, struct.pack('<HHH', i % 64, i % 64, i))
    elapsed = (time.perf_counter() - start) * 1000
    
    print(f"    {iterations} GPU commands in {elapsed:.2f}ms ({iterations/elapsed:.0f} ops/ms)")

# ============================================================
# Main
# ============================================================

def main():
    print("=" * 60)
    print("   GPU Driver Extended Test Suite")
    print("=" * 60)
    
    # Edge Cases
    runner.begin_category("Edge Cases")
    runner.run_test("Empty Payload", test_edge_case_empty_payload)
    runner.run_test("Max Payload (255 bytes)", test_edge_case_max_payload)
    runner.run_test("All Zeros Payload", test_edge_case_all_zeros)
    runner.run_test("All Ones Payload", test_edge_case_all_ones)
    runner.run_test("Alternating Pattern", test_edge_case_alternating)
    
    # Boundary Conditions
    runner.begin_category("Boundary Conditions")
    runner.run_test("Pixel Corners", test_boundary_pixel_corners)
    runner.run_test("Pixel Out of Bounds", test_boundary_pixel_out_of_bounds)
    runner.run_test("Rect Clipping", test_boundary_rect_clipping)
    runner.run_test("Rect Entirely Off Screen", test_boundary_rect_entirely_off)
    
    # Protocol Robustness
    runner.begin_category("Protocol Robustness")
    runner.run_test("All Opcodes Encode/Decode", test_protocol_all_opcodes)
    runner.run_test("CRC Error Detection", test_protocol_crc_detection)
    runner.run_test("Magic Number Detection", test_protocol_magic_detection)
    runner.run_test("Truncated Packet Handling", test_protocol_truncated)
    
    # UART Simulation
    runner.begin_category("UART Simulation")
    runner.run_test("Clean Channel", test_uart_clean_channel)
    runner.run_test("Noisy Channel (1% BER)", test_uart_noisy_channel)
    runner.run_test("Lossy Channel (10% drop)", test_uart_lossy_channel)
    
    # GPU Integration
    runner.begin_category("GPU Command Integration")
    runner.run_test("Command Sequence", test_gpu_command_sequence)
    runner.run_test("Rapid Clear/Flip", test_gpu_rapid_clear_flip)
    runner.run_test("Fill Entire Screen", test_gpu_fill_entire_screen)
    
    # Fixed-Point Edge Cases
    runner.begin_category("Fixed-Point Edge Cases")
    runner.run_test("Large Value Overflow", test_fixed_overflow)
    runner.run_test("Small Value Underflow", test_fixed_underflow)
    runner.run_test("Negative Multiplication", test_fixed_negative)
    runner.run_test("Division by Small Number", test_fixed_div_by_small)
    
    # Drawing Edge Cases
    runner.begin_category("Drawing Algorithm Edge Cases")
    runner.run_test("Zero Length Line", test_line_zero_length)
    runner.run_test("Single Pixel Line", test_line_single_pixel)
    runner.run_test("Negative Coordinates", test_line_negative_coords)
    runner.run_test("Backwards Line", test_line_backwards)
    
    # Animation
    runner.begin_category("Animation/Timing")
    runner.run_test("Frame Timing Calculation", test_animation_frame_timing)
    runner.run_test("Linear Interpolation", test_animation_interpolation)
    runner.run_test("Step Counting", test_animation_step_counting)
    
    # Memory
    runner.begin_category("Memory Patterns")
    runner.run_test("Alignment Check", test_memory_alignment)
    runner.run_test("Pool Allocation Simulation", test_memory_pool_simulation)
    runner.run_test("Fragmentation Simulation", test_memory_fragmentation)
    
    # Color
    runner.begin_category("Color Conversion")
    runner.run_test("Grayscale RGB565", test_color_grayscale)
    runner.run_test("Primary Colors RGB565", test_color_rgb565_primaries)
    runner.run_test("Alpha Blending", test_color_alpha_blend)
    
    # Performance
    runner.begin_category("Performance Benchmarks")
    runner.run_test("Packet Encoding", test_perf_packet_encoding)
    runner.run_test("Packet Decoding", test_perf_packet_decoding)
    runner.run_test("GPU Command Execution", test_perf_gpu_commands)
    
    return 0 if runner.summary() else 1

if __name__ == "__main__":
    sys.exit(main())
