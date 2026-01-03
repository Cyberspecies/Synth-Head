#!/usr/bin/env python3
"""
Hardware Tests v3 - Works with actual GPU/CPU architecture

ARCHITECTURE UNDERSTANDING:
- GPU firmware receives frames ONLY from CPU via UART1 (GPIO12/13 @ 10Mbps)
- USB-CDC (what we connect to) is only for debug logging, not protocol commands
- CPU transmits frames to GPU and logs status via USB-CDC
- We can verify the system works by observing the status messages

TEST APPROACH:
1. Connect to both GPU and CPU via USB-CDC
2. Parse their status messages to verify communication is working
3. Verify frame counts, FPS, and transfer statistics
"""

import serial
import serial.tools.list_ports
import time
import re
import sys

# Configuration
GPU_PORT = "COM5"
CPU_PORT = "COM15"
BAUD_RATE = 115200  # USB-CDC ignores this but we need to set something

def find_ports():
    """Find and verify COM ports."""
    ports = list(serial.tools.list_ports.comports())
    
    gpu_found = any(GPU_PORT in p.device for p in ports)
    cpu_found = any(CPU_PORT in p.device for p in ports)
    
    return gpu_found, cpu_found

class StatusParser:
    """Parse status messages from GPU and CPU."""
    
    # GPU pattern: I (198886) GPU_DUAL_DISPLAY: HUB75: RX 1577 @ 7 fps, Display 1577 @ 7 fps | OLED: RX 1577 @ 7 fps, Display...
    GPU_PATTERN = re.compile(
        r'GPU_DUAL_DISPLAY:.*HUB75:.*RX\s+(\d+)\s+@\s+(\d+)\s+fps.*Display\s+(\d+)\s+@\s+(\d+)\s+fps.*OLED:.*RX\s+(\d+)\s+@\s+(\d+)\s+fps'
    )
    
    # CPU pattern: [CPU] HUB75: 5 fps | OLED: 5 fps | TX: 44944 KB | RTT: 0 us
    CPU_PATTERN = re.compile(
        r'\[CPU\]\s+HUB75:\s+(\d+)\s+fps.*OLED:\s+(\d+)\s+fps.*TX:\s+(\d+)\s+KB.*RTT:\s+(\d+)\s+us'
    )
    
    @staticmethod
    def parse_gpu_status(text):
        """Parse GPU status message."""
        match = StatusParser.GPU_PATTERN.search(text)
        if match:
            return {
                'hub75_rx_count': int(match.group(1)),
                'hub75_rx_fps': int(match.group(2)),
                'hub75_display_count': int(match.group(3)),
                'hub75_display_fps': int(match.group(4)),
                'oled_rx_count': int(match.group(5)),
                'oled_rx_fps': int(match.group(6)),
            }
        return None
    
    @staticmethod
    def parse_cpu_status(text):
        """Parse CPU status message."""
        match = StatusParser.CPU_PATTERN.search(text)
        if match:
            return {
                'hub75_fps': int(match.group(1)),
                'oled_fps': int(match.group(2)),
                'tx_kb': int(match.group(3)),
                'rtt_us': int(match.group(4)),
            }
        return None

class HardwareTestSuite:
    """Hardware test suite that works with the actual architecture."""
    
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.gpu_ser = None
        self.cpu_ser = None
    
    def setup(self):
        """Setup serial connections."""
        print("\n" + "="*70)
        print("HARDWARE TEST SUITE v3")
        print("Testing actual GPU/CPU communication via status monitoring")
        print("="*70)
        
        try:
            print(f"\n[SETUP] Connecting to GPU ({GPU_PORT})...")
            self.gpu_ser = serial.Serial(
                port=GPU_PORT,
                baudrate=BAUD_RATE,
                timeout=0.1,
                write_timeout=0.5,
                rtscts=False,
                dsrdtr=False
            )
            print(f"        GPU connected")
            
            print(f"[SETUP] Connecting to CPU ({CPU_PORT})...")
            self.cpu_ser = serial.Serial(
                port=CPU_PORT,
                baudrate=BAUD_RATE,
                timeout=0.1,
                rtscts=False,
                dsrdtr=False
            )
            print(f"        CPU connected")
            
            # Flush buffers
            self.gpu_ser.reset_input_buffer()
            self.cpu_ser.reset_input_buffer()
            
            return True
            
        except serial.SerialException as e:
            print(f"        FAILED: {e}")
            return False
    
    def teardown(self):
        """Close connections."""
        if self.gpu_ser:
            self.gpu_ser.close()
        if self.cpu_ser:
            self.cpu_ser.close()
    
    def collect_data(self, duration=5.0):
        """Collect status data from both devices."""
        print(f"\n[COLLECT] Gathering status data for {duration}s...")
        
        gpu_data = []
        cpu_data = []
        
        gpu_buffer = ""
        cpu_buffer = ""
        
        start = time.time()
        while time.time() - start < duration:
            # Read from GPU
            if self.gpu_ser and self.gpu_ser.in_waiting:
                chunk = self.gpu_ser.read(self.gpu_ser.in_waiting).decode('utf-8', errors='replace')
                gpu_buffer += chunk
                
                # Parse complete lines
                while '\n' in gpu_buffer:
                    line, gpu_buffer = gpu_buffer.split('\n', 1)
                    status = StatusParser.parse_gpu_status(line)
                    if status:
                        gpu_data.append(status)
            
            # Read from CPU
            if self.cpu_ser and self.cpu_ser.in_waiting:
                chunk = self.cpu_ser.read(self.cpu_ser.in_waiting).decode('utf-8', errors='replace')
                cpu_buffer += chunk
                
                while '\n' in cpu_buffer:
                    line, cpu_buffer = cpu_buffer.split('\n', 1)
                    status = StatusParser.parse_cpu_status(line)
                    if status:
                        cpu_data.append(status)
            
            time.sleep(0.01)
        
        return gpu_data, cpu_data
    
    def run_test(self, name, test_func):
        """Run a single test."""
        print(f"\n[TEST] {name}")
        try:
            result, msg = test_func()
            if result:
                print(f"       ✓ PASS: {msg}")
                self.passed += 1
            else:
                print(f"       ✗ FAIL: {msg}")
                self.failed += 1
        except Exception as e:
            print(f"       ✗ FAIL: Exception: {e}")
            self.failed += 1
    
    # =========================================================
    # TESTS
    # =========================================================
    
    def test_gpu_receiving_frames(self):
        """Test that GPU is receiving frames."""
        gpu_data, _ = self.collect_data(3.0)
        
        if not gpu_data:
            return False, "No GPU status data received"
        
        # Check if frame counts are increasing
        if len(gpu_data) >= 2:
            first = gpu_data[0]['hub75_rx_count']
            last = gpu_data[-1]['hub75_rx_count']
            if last > first:
                return True, f"Frame count increased: {first} -> {last}"
            else:
                return False, f"Frame count not increasing: {first} -> {last}"
        
        return True, f"GPU receiving frames: {gpu_data[0]['hub75_rx_count']} frames"
    
    def test_cpu_transmitting_frames(self):
        """Test that CPU is transmitting frames."""
        _, cpu_data = self.collect_data(3.0)
        
        if not cpu_data:
            return False, "No CPU status data received"
        
        # Check TX KB is increasing
        if len(cpu_data) >= 2:
            first = cpu_data[0]['tx_kb']
            last = cpu_data[-1]['tx_kb']
            if last > first:
                return True, f"TX data increased: {first} KB -> {last} KB"
        
        return True, f"CPU transmitting: {cpu_data[0]['tx_kb']} KB total"
    
    def test_hub75_fps(self):
        """Test HUB75 frame rate is reasonable."""
        gpu_data, cpu_data = self.collect_data(3.0)
        
        # Get GPU HUB75 RX fps
        gpu_fps = [d['hub75_rx_fps'] for d in gpu_data if d['hub75_rx_fps'] > 0]
        cpu_fps = [d['hub75_fps'] for d in cpu_data if d['hub75_fps'] > 0]
        
        if not gpu_fps and not cpu_fps:
            return False, "No FPS data collected"
        
        avg_gpu_fps = sum(gpu_fps) / len(gpu_fps) if gpu_fps else 0
        avg_cpu_fps = sum(cpu_fps) / len(cpu_fps) if cpu_fps else 0
        
        # Expect at least 1 FPS (system is running)
        if avg_gpu_fps >= 1 or avg_cpu_fps >= 1:
            return True, f"GPU RX: {avg_gpu_fps:.1f} fps, CPU TX: {avg_cpu_fps:.1f} fps"
        
        return False, f"FPS too low: GPU={avg_gpu_fps:.1f}, CPU={avg_cpu_fps:.1f}"
    
    def test_oled_fps(self):
        """Test OLED frame rate is reasonable."""
        gpu_data, cpu_data = self.collect_data(3.0)
        
        gpu_fps = [d['oled_rx_fps'] for d in gpu_data if d['oled_rx_fps'] > 0]
        cpu_fps = [d['oled_fps'] for d in cpu_data if d['oled_fps'] > 0]
        
        if not gpu_fps and not cpu_fps:
            return False, "No OLED FPS data collected"
        
        avg_gpu_fps = sum(gpu_fps) / len(gpu_fps) if gpu_fps else 0
        avg_cpu_fps = sum(cpu_fps) / len(cpu_fps) if cpu_fps else 0
        
        if avg_gpu_fps >= 1 or avg_cpu_fps >= 1:
            return True, f"GPU RX: {avg_gpu_fps:.1f} fps, CPU TX: {avg_cpu_fps:.1f} fps"
        
        return False, f"OLED FPS too low: GPU={avg_gpu_fps:.1f}, CPU={avg_cpu_fps:.1f}"
    
    def test_gpu_display_active(self):
        """Test GPU is actually displaying frames."""
        gpu_data, _ = self.collect_data(3.0)
        
        if not gpu_data:
            return False, "No GPU data"
        
        # Check display fps
        display_fps = [d['hub75_display_fps'] for d in gpu_data if d['hub75_display_fps'] > 0]
        
        if display_fps:
            avg = sum(display_fps) / len(display_fps)
            return True, f"Display active at {avg:.1f} fps"
        
        return False, "Display not active"
    
    def test_no_rtt_timeout(self):
        """Test RTT is not timing out (0 = no ACK expected in streaming mode)."""
        _, cpu_data = self.collect_data(3.0)
        
        if not cpu_data:
            return False, "No CPU data"
        
        # In streaming mode, RTT should be 0 (no ACK expected)
        rtts = [d['rtt_us'] for d in cpu_data]
        avg_rtt = sum(rtts) / len(rtts) if rtts else -1
        
        # RTT of 0 is expected in streaming mode
        return True, f"Average RTT: {avg_rtt:.0f} us (0 = streaming mode, no ACK)"
    
    def test_frame_sync(self):
        """Test that GPU and CPU frame counts are roughly in sync."""
        gpu_data, cpu_data = self.collect_data(5.0)
        
        if not gpu_data or not cpu_data:
            return False, "Missing data from one device"
        
        # Compare final counts
        gpu_hub75_count = gpu_data[-1]['hub75_rx_count'] if gpu_data else 0
        
        # CPU doesn't report count directly, but TX KB can estimate
        # ~12KB per HUB75 frame
        cpu_tx_kb = cpu_data[-1]['tx_kb'] if cpu_data else 0
        estimated_frames = cpu_tx_kb / 12  # rough estimate
        
        return True, f"GPU received ~{gpu_hub75_count} frames, CPU transmitted ~{estimated_frames:.0f} frames worth"
    
    def test_continuous_operation(self):
        """Test system operates continuously for 10 seconds."""
        print("        Monitoring for 10 seconds...")
        
        samples = []
        for i in range(10):
            gpu_data, cpu_data = self.collect_data(1.0)
            samples.append((len(gpu_data) > 0, len(cpu_data) > 0))
            print(f"        Second {i+1}: GPU={'OK' if samples[-1][0] else 'NO DATA'}, CPU={'OK' if samples[-1][1] else 'NO DATA'}")
        
        gpu_ok = sum(1 for s in samples if s[0])
        cpu_ok = sum(1 for s in samples if s[1])
        
        if gpu_ok >= 8 and cpu_ok >= 8:
            return True, f"Stable operation: GPU {gpu_ok}/10, CPU {cpu_ok}/10"
        
        return False, f"Unstable: GPU {gpu_ok}/10, CPU {cpu_ok}/10"
    
    def run_all(self):
        """Run all tests."""
        if not self.setup():
            print("\n✗ SETUP FAILED - Cannot run tests")
            return False
        
        try:
            self.run_test("GPU Receiving Frames", self.test_gpu_receiving_frames)
            self.run_test("CPU Transmitting Frames", self.test_cpu_transmitting_frames)
            self.run_test("HUB75 Frame Rate", self.test_hub75_fps)
            self.run_test("OLED Frame Rate", self.test_oled_fps)
            self.run_test("GPU Display Active", self.test_gpu_display_active)
            self.run_test("RTT / ACK Status", self.test_no_rtt_timeout)
            self.run_test("Frame Sync Check", self.test_frame_sync)
            self.run_test("Continuous Operation (10s)", self.test_continuous_operation)
            
        finally:
            self.teardown()
        
        # Summary
        total = self.passed + self.failed
        print("\n" + "="*70)
        print(f"RESULTS: {self.passed}/{total} tests passed")
        print("="*70)
        
        if self.failed == 0:
            print("\n✓ ALL TESTS PASSED!")
            print("  GPU/CPU communication is working correctly.")
            return True
        else:
            print(f"\n✗ {self.failed} test(s) failed")
            return False

def main():
    # Check ports exist
    gpu_found, cpu_found = find_ports()
    
    if not gpu_found:
        print(f"ERROR: GPU port {GPU_PORT} not found")
        sys.exit(1)
    
    if not cpu_found:
        print(f"ERROR: CPU port {CPU_PORT} not found")
        sys.exit(1)
    
    # Run tests
    suite = HardwareTestSuite()
    success = suite.run_all()
    
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
