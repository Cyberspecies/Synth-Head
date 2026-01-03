#!/usr/bin/env python3
"""
GPU Driver Hardware Test Suite v2
Tests real hardware communication using the ACTUAL UartProtocol from UartProtocol.hpp

Protocol:
- 3-byte sync: 0xAA 0x55 0xCC
- PacketHeader: sync1, sync2, sync3, msg_type, payload_len(2), frame_num(2), frag_index, frag_total
- Payload (variable)
- PacketFooter: checksum(2), end_byte(0x55)

Supported MsgTypes:
- PING (0x01), PONG (0x02), ACK (0x03), NACK (0x04), STATUS (0x05)
- HUB75_FRAME (0x10), HUB75_FRAG (0x11), OLED_FRAME (0x12), OLED_FRAG (0x13)

Baud Rate: 10,000,000 (10 Mbps)
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
# Protocol Constants (from UartProtocol.hpp)
# ============================================================

SYNC_BYTE_1 = 0xAA
SYNC_BYTE_2 = 0x55
SYNC_BYTE_3 = 0xCC
END_BYTE = 0x55

# Correct baud rate from protocol
BAUD_RATE = 10000000  # 10 Mbps

# Display sizes
HUB75_WIDTH = 128
HUB75_HEIGHT = 32
HUB75_RGB_SIZE = HUB75_WIDTH * HUB75_HEIGHT * 3  # 12,288 bytes

OLED_WIDTH = 128
OLED_HEIGHT = 128
OLED_MONO_SIZE = (OLED_WIDTH * OLED_HEIGHT) // 8  # 2,048 bytes

# Fragment configuration
FRAGMENT_SIZE = 1024
HUB75_FRAGMENT_COUNT = 12
OLED_FRAGMENT_COUNT = 2

# Port configuration
DEFAULT_CPU_PORT = "COM15"
DEFAULT_GPU_PORT = "COM5"

# ============================================================
# Message Types (from UartProtocol.hpp)
# ============================================================

class MsgType(IntEnum):
    # Control messages
    PING = 0x01
    PONG = 0x02
    ACK = 0x03
    NACK = 0x04
    STATUS = 0x05
    FRAME_REQUEST = 0x06
    RESEND_FRAG = 0x07
    
    # Display frames
    HUB75_FRAME = 0x10
    HUB75_FRAG = 0x11
    OLED_FRAME = 0x12
    OLED_FRAG = 0x13
    
    # Settings
    SET_FPS = 0x20
    SET_BRIGHTNESS = 0x21
    
    # Diagnostics
    STATS_REQUEST = 0x30
    STATS_RESPONSE = 0x31

# ============================================================
# Packet Structures
# ============================================================

class PacketHeader:
    """10-byte packet header"""
    SIZE = 10
    
    def __init__(self, msg_type: int, payload_len: int = 0, 
                 frame_num: int = 0, frag_index: int = 0, frag_total: int = 1):
        self.sync1 = SYNC_BYTE_1
        self.sync2 = SYNC_BYTE_2
        self.sync3 = SYNC_BYTE_3
        self.msg_type = msg_type
        self.payload_len = payload_len
        self.frame_num = frame_num
        self.frag_index = frag_index
        self.frag_total = frag_total
    
    def pack(self) -> bytes:
        return struct.pack('<BBBHHHBB', 
                           self.sync1, self.sync2, self.sync3,
                           self.msg_type, self.payload_len, self.frame_num,
                           self.frag_index, self.frag_total)
    
    @staticmethod
    def unpack(data: bytes) -> 'PacketHeader':
        if len(data) < PacketHeader.SIZE:
            raise ValueError("Not enough data for header")
        s1, s2, s3, msg_type, payload_len, frame_num, frag_idx, frag_total = struct.unpack('<BBBHHHBB', data[:10])
        hdr = PacketHeader(msg_type, payload_len, frame_num, frag_idx, frag_total)
        hdr.sync1 = s1
        hdr.sync2 = s2
        hdr.sync3 = s3
        return hdr

class PacketFooter:
    """3-byte packet footer"""
    SIZE = 3
    
    def __init__(self, checksum: int = 0):
        self.checksum = checksum
        self.end_byte = END_BYTE
    
    def pack(self) -> bytes:
        return struct.pack('<HB', self.checksum, self.end_byte)
    
    @staticmethod
    def unpack(data: bytes) -> 'PacketFooter':
        if len(data) < PacketFooter.SIZE:
            raise ValueError("Not enough data for footer")
        checksum, end_byte = struct.unpack('<HB', data[:3])
        ftr = PacketFooter(checksum)
        ftr.end_byte = end_byte
        return ftr

# ============================================================
# Utility Functions
# ============================================================

def calc_checksum(data: bytes) -> int:
    """Simple sum checksum"""
    total = 0
    for b in data:
        total += b
    return total & 0xFFFF

def build_packet(msg_type: int, payload: bytes = b'', 
                 frame_num: int = 0, frag_index: int = 0, frag_total: int = 1) -> bytes:
    """Build a complete packet with header, payload, and footer"""
    header = PacketHeader(msg_type, len(payload), frame_num, frag_index, frag_total)
    header_bytes = header.pack()
    
    checksum = calc_checksum(header_bytes) + calc_checksum(payload)
    footer = PacketFooter(checksum & 0xFFFF)
    
    return header_bytes + payload + footer.pack()

def build_ping_packet(timestamp_us: int = None) -> bytes:
    """Build a PING packet"""
    if timestamp_us is None:
        timestamp_us = int(time.time() * 1000000) & 0xFFFFFFFF
    payload = struct.pack('<I', timestamp_us)
    return build_packet(MsgType.PING, payload)

def build_hub75_frame_packet(rgb_data: bytes, frame_num: int = 0) -> bytes:
    """Build a HUB75 full frame packet (legacy mode)"""
    if len(rgb_data) != HUB75_RGB_SIZE:
        raise ValueError(f"Invalid HUB75 data size: {len(rgb_data)}, expected {HUB75_RGB_SIZE}")
    return build_packet(MsgType.HUB75_FRAME, rgb_data, frame_num)

def build_hub75_fragment_packets(rgb_data: bytes, frame_num: int = 0) -> List[bytes]:
    """Build HUB75 fragment packets"""
    if len(rgb_data) != HUB75_RGB_SIZE:
        raise ValueError(f"Invalid HUB75 data size: {len(rgb_data)}, expected {HUB75_RGB_SIZE}")
    
    packets = []
    for i in range(HUB75_FRAGMENT_COUNT):
        offset = i * FRAGMENT_SIZE
        frag_data = rgb_data[offset:offset + FRAGMENT_SIZE]
        packet = build_packet(MsgType.HUB75_FRAG, frag_data, frame_num, i, HUB75_FRAGMENT_COUNT)
        packets.append(packet)
    return packets

def build_oled_fragment_packets(mono_data: bytes, frame_num: int = 0) -> List[bytes]:
    """Build OLED fragment packets"""
    if len(mono_data) != OLED_MONO_SIZE:
        raise ValueError(f"Invalid OLED data size: {len(mono_data)}, expected {OLED_MONO_SIZE}")
    
    packets = []
    for i in range(OLED_FRAGMENT_COUNT):
        offset = i * FRAGMENT_SIZE
        frag_data = mono_data[offset:offset + FRAGMENT_SIZE]
        packet = build_packet(MsgType.OLED_FRAG, frag_data, frame_num, i, OLED_FRAGMENT_COUNT)
        packets.append(packet)
    return packets

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
            print(f"  Connected to {self.port} @ {self.baud:,} baud ({self.baud/1000000:.1f} Mbps)")
            return True
        except serial.SerialException as e:
            print(f"  Failed to connect to {self.port}: {e}")
            return False
    
    def disconnect(self):
        if self.ser:
            self.ser.close()
            self.connected = False
    
    def send_raw(self, data: bytes) -> bool:
        """Send raw bytes"""
        if not self.connected:
            return False
        try:
            self.ser.write(data)
            self.ser.flush()
            return True
        except Exception as e:
            print(f"    Send error: {e}")
            return False
    
    def send_packet(self, packet: bytes) -> bool:
        """Send a pre-built packet"""
        return self.send_raw(packet)
    
    def receive_packet(self, timeout: float = 1.0) -> Optional[Tuple[PacketHeader, bytes, PacketFooter]]:
        """Receive a complete packet"""
        if not self.connected:
            return None
        
        self.ser.timeout = timeout
        start_time = time.time()
        
        # Look for 3-byte sync pattern
        sync_found = False
        while time.time() - start_time < timeout:
            byte1 = self.ser.read(1)
            if not byte1:
                continue
            
            if byte1[0] == SYNC_BYTE_1:
                byte2 = self.ser.read(1)
                if byte2 and byte2[0] == SYNC_BYTE_2:
                    byte3 = self.ser.read(1)
                    if byte3 and byte3[0] == SYNC_BYTE_3:
                        sync_found = True
                        break
        
        if not sync_found:
            return None
        
        # Read rest of header (already have sync bytes)
        remaining_header = self.ser.read(PacketHeader.SIZE - 3)
        if len(remaining_header) < PacketHeader.SIZE - 3:
            return None
        
        header_data = bytes([SYNC_BYTE_1, SYNC_BYTE_2, SYNC_BYTE_3]) + remaining_header
        header = PacketHeader.unpack(header_data)
        
        # Read payload
        payload = b''
        if header.payload_len > 0:
            payload = self.ser.read(header.payload_len)
            if len(payload) < header.payload_len:
                return None
        
        # Read footer
        footer_data = self.ser.read(PacketFooter.SIZE)
        if len(footer_data) < PacketFooter.SIZE:
            return None
        footer = PacketFooter.unpack(footer_data)
        
        # Verify checksum
        expected_checksum = calc_checksum(header_data) + calc_checksum(payload)
        expected_checksum &= 0xFFFF
        
        if footer.checksum != expected_checksum:
            print(f"    Checksum mismatch: got {footer.checksum:04X}, expected {expected_checksum:04X}")
            return None
        
        return header, payload, footer
    
    def flush(self):
        if self.ser:
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()

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
    def __init__(self, gpu_port: str = DEFAULT_GPU_PORT, cpu_port: str = DEFAULT_CPU_PORT):
        self.gpu = SerialInterface(gpu_port)
        self.cpu = SerialInterface(cpu_port)
        self.results: List[TestResult] = []
        
    def setup(self) -> bool:
        print("\n[SETUP] Connecting to hardware...")
        print(f"  Protocol: 3-byte sync (0xAA 0x55 0xCC)")
        print(f"  Baud Rate: {BAUD_RATE:,} ({BAUD_RATE/1000000:.1f} Mbps)")
        
        gpu_ok = self.gpu.connect()
        cpu_ok = self.cpu.connect()
        
        if not gpu_ok and not cpu_ok:
            print("  WARNING: No hardware connected")
            return False
        
        # Flush buffers
        if gpu_ok:
            self.gpu.flush()
        if cpu_ok:
            self.cpu.flush()
        
        time.sleep(0.1)
        return gpu_ok or cpu_ok
    
    def teardown(self):
        print("\n[TEARDOWN] Disconnecting...")
        self.gpu.disconnect()
        self.cpu.disconnect()
    
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
        
        print(f"\n{'='*60}")
        print(f"       HARDWARE TEST RESULTS (Protocol v2)")
        print(f"{'='*60}")
        print(f"Total:  {total} tests")
        print(f"Passed: {passed} ({100*passed/total:.1f}%)" if total > 0 else "Passed: 0")
        print(f"Failed: {failed}")
        print(f"Time:   {total_time:.2f} ms")
        print(f"{'='*60}")
        
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
        """Test that GPU serial port is accessible"""
        if not runner.gpu.connected:
            return "GPU not connected"
        return True
    
    def test_cpu_connection():
        """Test that CPU serial port is accessible"""
        if not runner.cpu.connected:
            return "CPU not connected"
        return True
    
    def test_gpu_ping_pong():
        """Test PING/PONG round-trip (the correct echo mechanism)"""
        if not runner.gpu.connected:
            return "GPU not connected"
        
        runner.gpu.flush()
        
        # Send PING
        timestamp = int(time.time() * 1000000) & 0xFFFFFFFF
        ping_packet = build_ping_packet(timestamp)
        
        if not runner.gpu.send_packet(ping_packet):
            return "Failed to send PING"
        
        # Wait for PONG
        response = runner.gpu.receive_packet(timeout=1.0)
        if response is None:
            return "No PONG response (GPU may need PING support in firmware)"
        
        header, payload, footer = response
        
        if header.msg_type != MsgType.PONG:
            return f"Wrong response type: 0x{header.msg_type:02X} (expected PONG 0x02)"
        
        if len(payload) >= 4:
            rx_timestamp = struct.unpack('<I', payload[:4])[0]
            # PONG should echo back our timestamp
            if rx_timestamp != timestamp:
                return f"Timestamp mismatch: got {rx_timestamp}, sent {timestamp}"
        
        return True
    
    def test_packet_integrity():
        """Test that packets maintain integrity through checksum"""
        if not runner.gpu.connected:
            return "GPU not connected"
        
        # Build a test packet (PING) and verify structure
        timestamp = 0x12345678
        packet = build_ping_packet(timestamp)
        
        # Verify packet structure
        if len(packet) != PacketHeader.SIZE + 4 + PacketFooter.SIZE:  # 4 = timestamp size
            return f"Invalid packet size: {len(packet)}"
        
        # Verify sync bytes
        if packet[0:3] != bytes([SYNC_BYTE_1, SYNC_BYTE_2, SYNC_BYTE_3]):
            return "Invalid sync bytes"
        
        # Verify checksum calculation
        header = PacketHeader.unpack(packet[:PacketHeader.SIZE])
        payload = packet[PacketHeader.SIZE:PacketHeader.SIZE + header.payload_len]
        footer = PacketFooter.unpack(packet[-PacketFooter.SIZE:])
        
        expected_checksum = calc_checksum(packet[:PacketHeader.SIZE]) + calc_checksum(payload)
        expected_checksum &= 0xFFFF
        
        if footer.checksum != expected_checksum:
            return f"Checksum calculation error: {footer.checksum:04X} vs {expected_checksum:04X}"
        
        return True
    
    def test_hub75_frame_generation():
        """Test HUB75 frame packet generation"""
        # Generate test pattern (gradient)
        rgb_data = bytearray(HUB75_RGB_SIZE)
        for y in range(HUB75_HEIGHT):
            for x in range(HUB75_WIDTH):
                idx = (y * HUB75_WIDTH + x) * 3
                rgb_data[idx + 0] = x * 2      # R
                rgb_data[idx + 1] = y * 8      # G
                rgb_data[idx + 2] = (x + y) % 256  # B
        
        # Build fragmented packets
        packets = build_hub75_fragment_packets(bytes(rgb_data), frame_num=1)
        
        if len(packets) != HUB75_FRAGMENT_COUNT:
            return f"Wrong fragment count: {len(packets)}, expected {HUB75_FRAGMENT_COUNT}"
        
        # Verify each fragment
        total_payload = 0
        for i, packet in enumerate(packets):
            header = PacketHeader.unpack(packet[:PacketHeader.SIZE])
            
            if header.msg_type != MsgType.HUB75_FRAG:
                return f"Fragment {i}: wrong type 0x{header.msg_type:02X}"
            
            if header.frag_index != i:
                return f"Fragment {i}: wrong index {header.frag_index}"
            
            if header.frag_total != HUB75_FRAGMENT_COUNT:
                return f"Fragment {i}: wrong total {header.frag_total}"
            
            total_payload += header.payload_len
        
        if total_payload != HUB75_RGB_SIZE:
            return f"Total payload mismatch: {total_payload}, expected {HUB75_RGB_SIZE}"
        
        return True
    
    def test_oled_frame_generation():
        """Test OLED frame packet generation"""
        # Generate test pattern (checkerboard)
        mono_data = bytearray(OLED_MONO_SIZE)
        for i in range(OLED_MONO_SIZE):
            mono_data[i] = 0xAA if (i % 2) == 0 else 0x55
        
        # Build fragmented packets
        packets = build_oled_fragment_packets(bytes(mono_data), frame_num=1)
        
        if len(packets) != OLED_FRAGMENT_COUNT:
            return f"Wrong fragment count: {len(packets)}, expected {OLED_FRAGMENT_COUNT}"
        
        return True
    
    def test_send_hub75_frame():
        """Test sending a HUB75 frame to GPU"""
        if not runner.gpu.connected:
            return "GPU not connected"
        
        runner.gpu.flush()
        
        # Generate red frame
        rgb_data = bytearray(HUB75_RGB_SIZE)
        for i in range(0, HUB75_RGB_SIZE, 3):
            rgb_data[i + 0] = 255  # R
            rgb_data[i + 1] = 0    # G
            rgb_data[i + 2] = 0    # B
        
        # Send all fragments
        packets = build_hub75_fragment_packets(bytes(rgb_data), frame_num=1)
        
        for i, packet in enumerate(packets):
            if not runner.gpu.send_packet(packet):
                return f"Failed to send fragment {i}"
            time.sleep(0.001)  # Small delay between fragments
        
        # GPU should display the frame (no response expected in streaming mode)
        time.sleep(0.1)
        
        return True
    
    def test_send_oled_frame():
        """Test sending an OLED frame to GPU"""
        if not runner.gpu.connected:
            return "GPU not connected"
        
        runner.gpu.flush()
        
        # Generate checkerboard pattern
        mono_data = bytearray(OLED_MONO_SIZE)
        for y in range(OLED_HEIGHT):
            for x in range(OLED_WIDTH):
                byte_idx = (y * OLED_WIDTH + x) // 8
                bit_idx = 7 - (x % 8)  # MSB first
                if (x // 8 + y // 8) % 2 == 0:
                    mono_data[byte_idx] |= (1 << bit_idx)
        
        # Send all fragments
        packets = build_oled_fragment_packets(bytes(mono_data), frame_num=1)
        
        for i, packet in enumerate(packets):
            if not runner.gpu.send_packet(packet):
                return f"Failed to send fragment {i}"
            time.sleep(0.001)
        
        time.sleep(0.1)
        return True
    
    def test_frame_sequence():
        """Test sending multiple frames in sequence"""
        if not runner.gpu.connected:
            return "GPU not connected"
        
        runner.gpu.flush()
        
        # Send 5 frames with increasing brightness
        for frame_num in range(5):
            brightness = (frame_num + 1) * 50  # 50, 100, 150, 200, 250
            
            rgb_data = bytearray(HUB75_RGB_SIZE)
            for i in range(0, HUB75_RGB_SIZE, 3):
                rgb_data[i + 0] = brightness  # R
                rgb_data[i + 1] = brightness  # G
                rgb_data[i + 2] = brightness  # B
            
            packets = build_hub75_fragment_packets(bytes(rgb_data), frame_num=frame_num)
            
            for packet in packets:
                runner.gpu.send_packet(packet)
                time.sleep(0.0005)  # 0.5ms between fragments
            
            time.sleep(0.016)  # ~60fps spacing
        
        return True
    
    def test_throughput():
        """Test frame throughput (frames per second)"""
        if not runner.gpu.connected:
            return "GPU not connected"
        
        runner.gpu.flush()
        
        # Generate test frame
        rgb_data = bytes([128] * HUB75_RGB_SIZE)
        
        # Send 60 frames as fast as possible
        num_frames = 60
        start_time = time.perf_counter()
        
        for frame_num in range(num_frames):
            packets = build_hub75_fragment_packets(rgb_data, frame_num=frame_num)
            for packet in packets:
                runner.gpu.send_packet(packet)
        
        elapsed = time.perf_counter() - start_time
        fps = num_frames / elapsed
        
        print(f"[INFO] {fps:.1f} fps", end=" ")
        
        if fps < 30:
            return f"Low throughput: {fps:.1f} fps (target: 60 fps)"
        
        return True
    
    def test_color_accuracy():
        """Test that specific color values are encoded correctly"""
        # Test specific RGB values
        test_colors = [
            (255, 0, 0),     # Pure red
            (0, 255, 0),     # Pure green
            (0, 0, 255),     # Pure blue
            (255, 255, 255), # White
            (0, 0, 0),       # Black
            (128, 128, 128), # Gray
        ]
        
        for r, g, b in test_colors:
            rgb_data = bytearray(HUB75_RGB_SIZE)
            for i in range(0, HUB75_RGB_SIZE, 3):
                rgb_data[i + 0] = r
                rgb_data[i + 1] = g
                rgb_data[i + 2] = b
            
            packets = build_hub75_fragment_packets(bytes(rgb_data), frame_num=0)
            
            # Verify first fragment contains correct color
            first_frag = packets[0]
            header = PacketHeader.unpack(first_frag[:PacketHeader.SIZE])
            payload_start = PacketHeader.SIZE
            payload = first_frag[payload_start:payload_start + header.payload_len]
            
            if payload[0] != r or payload[1] != g or payload[2] != b:
                return f"Color ({r},{g},{b}) encoded as ({payload[0]},{payload[1]},{payload[2]})"
        
        return True
    
    # Return test functions in order
    return [
        ("GPU Connection", test_gpu_connection),
        ("CPU Connection", test_cpu_connection),
        ("Packet Integrity", test_packet_integrity),
        ("HUB75 Frame Generation", test_hub75_frame_generation),
        ("OLED Frame Generation", test_oled_frame_generation),
        ("Color Accuracy", test_color_accuracy),
        ("GPU PING/PONG", test_gpu_ping_pong),
        ("Send HUB75 Frame", test_send_hub75_frame),
        ("Send OLED Frame", test_send_oled_frame),
        ("Frame Sequence", test_frame_sequence),
        ("Throughput (60 frames)", test_throughput),
    ]

# ============================================================
# Main Entry Point
# ============================================================

def main():
    parser = argparse.ArgumentParser(description='GPU Hardware Test Suite v2 (Correct Protocol)')
    parser.add_argument('--gpu', default=DEFAULT_GPU_PORT, help=f'GPU serial port (default: {DEFAULT_GPU_PORT})')
    parser.add_argument('--cpu', default=DEFAULT_CPU_PORT, help=f'CPU serial port (default: {DEFAULT_CPU_PORT})')
    parser.add_argument('--baud', type=int, default=BAUD_RATE, help=f'Baud rate (default: {BAUD_RATE})')
    args = parser.parse_args()
    
    baud_rate = args.baud
    
    print("=" * 60)
    print("    GPU DRIVER HARDWARE TEST SUITE v2")
    print("    Using Correct UartProtocol (3-byte sync)")
    print("=" * 60)
    print(f"  GPU Port: {args.gpu}")
    print(f"  CPU Port: {args.cpu}")
    print(f"  Baud Rate: {baud_rate:,} ({baud_rate/1000000:.1f} Mbps)")
    print(f"  Protocol: 0xAA 0x55 0xCC sync, 10-byte header")
    
    runner = HardwareTestRunner(gpu_port=args.gpu, cpu_port=args.cpu)
    runner.gpu.baud = baud_rate
    runner.cpu.baud = baud_rate
    
    if not runner.setup():
        print("\n[ERROR] Hardware setup failed!")
        print("Check that:")
        print("  1. GPU (ESP32-S3) is connected and powered")
        print("  2. Correct serial ports are specified")
        print("  3. No other program is using the ports")
        return 1
    
    try:
        print("\n[TESTS] Running hardware tests...")
        tests = create_hardware_tests(runner)
        
        for name, func in tests:
            runner.run_test(name, func)
        
        all_passed = runner.summary()
        return 0 if all_passed else 1
        
    finally:
        runner.teardown()

if __name__ == "__main__":
    sys.exit(main())
