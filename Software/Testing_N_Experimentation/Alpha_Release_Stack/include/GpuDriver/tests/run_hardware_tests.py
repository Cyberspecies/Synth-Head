#!/usr/bin/env python3
"""
GPU Driver Hardware Test Suite
Tests real hardware communication between CPU (COM15) and GPU (COM5)
"""

import sys
import time
import serial
import struct
from typing import List, Tuple, Optional
from dataclasses import dataclass
from enum import IntEnum
import argparse

# ============================================================
# Configuration
# ============================================================

DEFAULT_CPU_PORT = "COM15"
DEFAULT_GPU_PORT = "COM5"
BAUD_RATE = 2000000  # 2 Mbps

# ============================================================
# ISA Definitions
# ============================================================

class Opcode(IntEnum):
    NOP = 0x00
    SET_PIXEL = 0x01
    FILL_RECT = 0x02
    DRAW_LINE = 0x03
    DRAW_CIRCLE = 0x04
    CLEAR = 0x10
    FLIP = 0x11
    SET_COLOR = 0x12
    SYNC = 0xF0
    RESET = 0xFF
    
    # Test commands
    TEST_ECHO = 0xE0
    TEST_PERF = 0xE1
    TEST_MEM = 0xE2

# ============================================================
# Protocol
# ============================================================

class UartPacket:
    MAGIC = bytes([0xAA, 0x55])
    MAX_PAYLOAD = 255
    
    @staticmethod
    def encode(cmd: int, payload: bytes) -> bytes:
        if len(payload) > UartPacket.MAX_PAYLOAD:
            raise ValueError(f"Payload too large: {len(payload)}")
        
        header = UartPacket.MAGIC + bytes([cmd, len(payload)])
        crc = cmd ^ len(payload)
        for b in payload:
            crc ^= b
        return header + payload + bytes([crc & 0xFF])
    
    @staticmethod
    def decode(data: bytes) -> Tuple[int, bytes]:
        if len(data) < 5:
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
            raise ValueError(f"CRC mismatch")
        
        return cmd, payload

# ============================================================
# Hardware Interface
# ============================================================

class SerialInterface:
    def __init__(self, port: str, baud: int = BAUD_RATE, timeout: float = 1.0):
        self.port = port
        self.baud = baud
        self.timeout = timeout
        self.ser: Optional[serial.Serial] = None
        self.connected = False
        
    def connect(self) -> bool:
        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baud,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=self.timeout
            )
            self.connected = True
            print(f"  Connected to {self.port} @ {self.baud} baud")
            return True
        except serial.SerialException as e:
            print(f"  Failed to connect to {self.port}: {e}")
            return False
    
    def disconnect(self):
        if self.ser:
            self.ser.close()
            self.connected = False
    
    def send_packet(self, cmd: int, payload: bytes) -> bool:
        if not self.connected:
            return False
        try:
            packet = UartPacket.encode(cmd, payload)
            self.ser.write(packet)
            return True
        except Exception as e:
            print(f"    Send error: {e}")
            return False
    
    def receive_packet(self, timeout: float = 1.0) -> Optional[Tuple[int, bytes]]:
        if not self.connected:
            return None
        
        self.ser.timeout = timeout
        
        # Look for magic bytes
        magic_found = False
        start_time = time.time()
        
        while time.time() - start_time < timeout:
            byte = self.ser.read(1)
            if not byte:
                continue
            
            if byte[0] == 0xAA:
                next_byte = self.ser.read(1)
                if next_byte and next_byte[0] == 0x55:
                    magic_found = True
                    break
        
        if not magic_found:
            return None
        
        # Read header
        header = self.ser.read(2)
        if len(header) < 2:
            return None
        
        cmd = header[0]
        length = header[1]
        
        # Read payload + CRC
        data = self.ser.read(length + 1)
        if len(data) < length + 1:
            return None
        
        payload = data[:length]
        received_crc = data[length]
        
        # Verify CRC
        expected_crc = cmd ^ length
        for b in payload:
            expected_crc ^= b
        expected_crc &= 0xFF
        
        if received_crc != expected_crc:
            return None
        
        return cmd, payload
    
    def flush(self):
        if self.ser:
            self.ser.flushInput()
            self.ser.flushOutput()

# ============================================================
# Test Framework
# ============================================================

@dataclass
class TestResult:
    name: str
    passed: bool
    duration_ms: float
    message: str = ""

class HardwareTestRunner:
    def __init__(self, cpu_port: str = DEFAULT_CPU_PORT, gpu_port: str = DEFAULT_GPU_PORT):
        self.cpu = SerialInterface(cpu_port)
        self.gpu = SerialInterface(gpu_port)
        self.results: List[TestResult] = []
        
    def setup(self) -> bool:
        print("\n[SETUP] Connecting to hardware...")
        
        cpu_ok = self.cpu.connect()
        gpu_ok = self.gpu.connect()
        
        if not cpu_ok and not gpu_ok:
            print("  WARNING: No hardware connected - running in simulation mode")
            return False
        
        # Flush buffers
        if cpu_ok:
            self.cpu.flush()
        if gpu_ok:
            self.gpu.flush()
        
        # Small delay for hardware initialization
        time.sleep(0.1)
        
        return cpu_ok or gpu_ok
    
    def teardown(self):
        print("\n[TEARDOWN] Disconnecting...")
        self.cpu.disconnect()
        self.gpu.disconnect()
    
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
        except Exception as e:
            duration = (time.perf_counter() - start) * 1000
            print(f"[ERROR] {e}")
            self.results.append(TestResult(name, False, duration, str(e)))
    
    def summary(self) -> bool:
        total = len(self.results)
        passed = sum(1 for r in self.results if r.passed)
        failed = total - passed
        total_time = sum(r.duration_ms for r in self.results)
        
        print(f"\n{'='*50}")
        print(f"       HARDWARE TEST RESULTS")
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
        
        return failed == 0

# ============================================================
# Hardware Tests
# ============================================================

def create_hardware_tests(runner: HardwareTestRunner):
    """Create hardware test functions"""
    
    def test_gpu_connection():
        if not runner.gpu.connected:
            return "GPU not connected"
        return True
    
    def test_cpu_connection():
        if not runner.cpu.connected:
            return "CPU not connected"
        return True
    
    def test_gpu_echo():
        if not runner.gpu.connected:
            return "GPU not connected"
        
        test_data = bytes([0x12, 0x34, 0x56, 0x78])
        runner.gpu.flush()
        runner.gpu.send_packet(Opcode.TEST_ECHO, test_data)
        
        response = runner.gpu.receive_packet(timeout=0.5)
        if response is None:
            return "No response (echo command may not be implemented)"
        
        cmd, payload = response
        if cmd != Opcode.TEST_ECHO:
            return f"Wrong response command: {cmd:02X}"
        if payload != test_data:
            return f"Echo mismatch: got {payload.hex()}, expected {test_data.hex()}"
        return True
    
    def test_gpu_nop():
        if not runner.gpu.connected:
            return "GPU not connected"
        
        success = runner.gpu.send_packet(Opcode.NOP, bytes())
        if not success:
            return "Failed to send NOP"
        return True
    
    def test_gpu_reset():
        if not runner.gpu.connected:
            return "GPU not connected"
        
        success = runner.gpu.send_packet(Opcode.RESET, bytes())
        if not success:
            return "Failed to send RESET"
        time.sleep(0.1)  # Wait for reset
        return True
    
    def test_gpu_clear():
        if not runner.gpu.connected:
            return "GPU not connected"
        
        # Clear to black
        payload = struct.pack('<I', 0x000000)
        success = runner.gpu.send_packet(Opcode.CLEAR, payload)
        if not success:
            return "Failed to send CLEAR"
        return True
    
    def test_gpu_set_pixel():
        if not runner.gpu.connected:
            return "GPU not connected"
        
        # Set pixel at (32, 32) to white
        payload = struct.pack('<HHH', 32, 32, 0xFFFF)
        success = runner.gpu.send_packet(Opcode.SET_PIXEL, payload)
        if not success:
            return "Failed to send SET_PIXEL"
        return True
    
    def test_gpu_fill_rect():
        if not runner.gpu.connected:
            return "GPU not connected"
        
        # Fill 10x10 rect at (10, 10) with red
        payload = struct.pack('<HHHHI', 10, 10, 10, 10, 0xFF0000)
        success = runner.gpu.send_packet(Opcode.FILL_RECT, payload)
        if not success:
            return "Failed to send FILL_RECT"
        return True
    
    def test_gpu_draw_line():
        if not runner.gpu.connected:
            return "GPU not connected"
        
        # Draw line from (0,0) to (63,63)
        payload = struct.pack('<HHHHI', 0, 0, 63, 63, 0x00FF00)
        success = runner.gpu.send_packet(Opcode.DRAW_LINE, payload)
        if not success:
            return "Failed to send DRAW_LINE"
        return True
    
    def test_gpu_flip():
        if not runner.gpu.connected:
            return "GPU not connected"
        
        success = runner.gpu.send_packet(Opcode.FLIP, bytes())
        if not success:
            return "Failed to send FLIP"
        return True
    
    def test_gpu_rapid_commands():
        if not runner.gpu.connected:
            return "GPU not connected"
        
        # Send many commands rapidly
        start = time.perf_counter()
        count = 100
        
        for i in range(count):
            payload = struct.pack('<HHH', i % 64, i % 64, i)
            runner.gpu.send_packet(Opcode.SET_PIXEL, payload)
        
        elapsed = (time.perf_counter() - start) * 1000
        rate = count / (elapsed / 1000) if elapsed > 0 else 0
        print(f" ({count} cmds in {elapsed:.1f}ms = {rate:.0f} cmd/s)", end="")
        return True
    
    def test_gpu_throughput():
        if not runner.gpu.connected:
            return "GPU not connected"
        
        # Measure throughput
        runner.gpu.flush()
        
        start = time.perf_counter()
        total_bytes = 0
        iterations = 50
        
        for _ in range(iterations):
            # Send FILL_RECT (largest common command)
            payload = struct.pack('<HHHHI', 0, 0, 64, 64, 0xFFFFFF)
            packet = UartPacket.encode(Opcode.FILL_RECT, payload)
            runner.gpu.ser.write(packet)
            total_bytes += len(packet)
        
        elapsed = time.perf_counter() - start
        throughput = total_bytes / elapsed / 1024 if elapsed > 0 else 0
        print(f" ({throughput:.1f} KB/s)", end="")
        return True
    
    def test_gpu_frame_rate():
        if not runner.gpu.connected:
            return "GPU not connected"
        
        # Measure achievable frame rate
        start = time.perf_counter()
        frames = 60
        
        for _ in range(frames):
            # Simulate minimal frame: clear + flip
            runner.gpu.send_packet(Opcode.CLEAR, struct.pack('<I', 0))
            runner.gpu.send_packet(Opcode.FLIP, bytes())
        
        elapsed = time.perf_counter() - start
        fps = frames / elapsed if elapsed > 0 else 0
        print(f" ({fps:.1f} FPS)", end="")
        
        if fps < 30:
            return f"Frame rate too low: {fps:.1f} FPS (target: 60)"
        return True
    
    def test_cpu_to_gpu_relay():
        """Test CPU forwarding commands to GPU"""
        if not runner.cpu.connected:
            return "CPU not connected"
        
        # Send command via CPU (it should relay to GPU)
        runner.cpu.send_packet(Opcode.CLEAR, struct.pack('<I', 0xFF0000))
        time.sleep(0.05)
        runner.cpu.send_packet(Opcode.FLIP, bytes())
        return True
    
    return [
        ("GPU Connection", test_gpu_connection),
        ("CPU Connection", test_cpu_connection),
        ("GPU Echo", test_gpu_echo),
        ("GPU NOP", test_gpu_nop),
        ("GPU Reset", test_gpu_reset),
        ("GPU Clear", test_gpu_clear),
        ("GPU Set Pixel", test_gpu_set_pixel),
        ("GPU Fill Rect", test_gpu_fill_rect),
        ("GPU Draw Line", test_gpu_draw_line),
        ("GPU Flip", test_gpu_flip),
        ("GPU Rapid Commands", test_gpu_rapid_commands),
        ("GPU Throughput", test_gpu_throughput),
        ("GPU Frame Rate", test_gpu_frame_rate),
        ("CPU to GPU Relay", test_cpu_to_gpu_relay),
    ]

# ============================================================
# Main
# ============================================================

def main():
    parser = argparse.ArgumentParser(description="GPU Driver Hardware Test Suite")
    parser.add_argument("--cpu-port", default=DEFAULT_CPU_PORT, help=f"CPU serial port (default: {DEFAULT_CPU_PORT})")
    parser.add_argument("--gpu-port", default=DEFAULT_GPU_PORT, help=f"GPU serial port (default: {DEFAULT_GPU_PORT})")
    parser.add_argument("--baud", type=int, default=BAUD_RATE, help=f"Baud rate (default: {BAUD_RATE})")
    parser.add_argument("--skip-hw", action="store_true", help="Skip actual hardware tests")
    args = parser.parse_args()
    
    print("=" * 60)
    print("   GPU Driver Hardware Test Suite")
    print("=" * 60)
    print(f"CPU Port: {args.cpu_port}")
    print(f"GPU Port: {args.gpu_port}")
    print(f"Baud Rate: {args.baud}")
    
    if args.skip_hw:
        print("\n*** Hardware tests skipped (--skip-hw) ***")
        return 0
    
    runner = HardwareTestRunner(args.cpu_port, args.gpu_port)
    
    # Try to setup hardware
    hw_available = runner.setup()
    
    if not hw_available:
        print("\n*** No hardware available - tests will show connection failures ***")
    
    # Create and run tests
    print("\n[TESTS] Running hardware tests...")
    tests = create_hardware_tests(runner)
    
    for name, func in tests:
        runner.run_test(name, func)
    
    # Cleanup
    runner.teardown()
    
    # Summary
    success = runner.summary()
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())
