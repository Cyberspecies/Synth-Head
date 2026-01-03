#!/usr/bin/env python3
"""
Hardware Debug Script v2.0 for GPU Driver Tests
Enhanced diagnostics to understand communication issues.

Key insight: ESP32-S3 has built-in USB-CDC which appears as a serial port
but doesn't actually use traditional UART baud rates - the USB-CDC interface
ignores baud rate settings and runs at USB 2.0 Full Speed (12 Mbps).

The 10 Mbps in the firmware is for the ACTUAL UART between CPU<->GPU,
not for USB-CDC to PC!
"""

import serial
import serial.tools.list_ports
import time
import struct
import sys

# Protocol constants from UartProtocol.hpp
SYNC_BYTE_1 = 0xAA
SYNC_BYTE_2 = 0x55
SYNC_BYTE_3 = 0xCC

class MsgType:
    PING = 0x01
    PONG = 0x02
    ACK = 0x03
    NACK = 0x04
    STATUS = 0x05
    HUB75_FRAME = 0x10
    HUB75_FRAG = 0x11
    OLED_FRAME = 0x12
    OLED_FRAG = 0x13

def hexdump(data, prefix=""):
    """Print hex dump of data."""
    if not data:
        print(f"{prefix}(empty)")
        return
    hex_str = ' '.join(f'{b:02x}' for b in data)
    print(f"{prefix}{hex_str}")
    # Also show ASCII if printable
    ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in data)
    print(f"{prefix}ASCII: {ascii_str}")

def build_packet(msg_type, seq_num=1, payload=b''):
    """Build a protocol packet."""
    header = bytes([
        SYNC_BYTE_1, SYNC_BYTE_2, SYNC_BYTE_3,  # Sync
        0x01,  # Version
        msg_type,  # Message type
    ])
    header += struct.pack('<H', seq_num)  # Sequence number (little endian)
    header += struct.pack('<H', len(payload))  # Payload length
    
    # Checksum: XOR of all bytes
    packet = header + payload
    checksum = 0
    for b in packet:
        checksum ^= b
    
    return packet + bytes([checksum])

def get_msg_name(msg_type):
    """Get human-readable name for message type."""
    names = {
        0x01: "PING",
        0x02: "PONG",
        0x03: "ACK",
        0x04: "NACK",
        0x05: "STATUS",
        0x10: "HUB75_FRAME",
        0x11: "HUB75_FRAG",
        0x12: "OLED_FRAME",
        0x13: "OLED_FRAG",
    }
    return names.get(msg_type, f"UNKNOWN(0x{msg_type:02x})")

def scan_ports():
    """Scan and display available serial ports with detailed info."""
    print("\n" + "="*70)
    print("[STEP 1] SCANNING SERIAL PORTS")
    print("="*70)
    ports = list(serial.tools.list_ports.comports())
    
    if not ports:
        print("  ERROR: No serial ports found!")
        return None, None
    
    gpu_port = None
    cpu_port = None
    
    for p in ports:
        print(f"\n  PORT: {p.device}")
        print(f"    Description:  {p.description}")
        print(f"    HWID:         {p.hwid}")
        if p.vid:
            print(f"    VID:PID:      {p.vid:04X}:{p.pid:04X}")
        print(f"    Serial#:      {p.serial_number}")
        print(f"    Manufacturer: {p.manufacturer}")
        print(f"    Product:      {p.product}")
        
        # Identify based on port number
        if "COM5" in p.device:
            gpu_port = p.device
            print(f"    >> IDENTIFIED AS: GPU")
        elif "COM15" in p.device:
            cpu_port = p.device
            print(f"    >> IDENTIFIED AS: CPU")
    
    return gpu_port, cpu_port

def test_port_detailed(port, name):
    """Test a serial port with very detailed diagnostics."""
    print(f"\n" + "="*70)
    print(f"[STEP 2] TESTING {name} ({port})")
    print("="*70)
    
    # ESP32-S3 USB-CDC doesn't care about baud rate, use 115200
    # (The 10Mbps is for physical UART between CPU<->GPU, not USB to PC)
    baud = 115200
    
    try:
        print(f"\n  [2.1] Opening port at {baud} baud...")
        print(f"        Note: ESP32-S3 USB-CDC ignores baud rate setting")
        
        ser = serial.Serial(
            port=port,
            baudrate=baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=2.0,
            write_timeout=2.0,
            rtscts=False,
            dsrdtr=False
        )
        
        print(f"        SUCCESS - Port opened")
        print(f"        Settings: {ser.baudrate} baud, 8N1")
        
        # Don't toggle DTR/RTS - they can reset ESP32!
        print(f"\n  [2.2] Control signals (NOT toggling - can reset ESP32):")
        print(f"        DTR: {ser.dtr}")
        print(f"        RTS: {ser.rts}")
        
        # Flush and wait
        print(f"\n  [2.3] Flushing buffers and waiting 2s for boot data...")
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        
        # Listen for any boot messages or spontaneous output
        all_data = b''
        for i in range(20):  # 20 x 100ms = 2 seconds
            time.sleep(0.1)
            waiting = ser.in_waiting
            if waiting > 0:
                chunk = ser.read(waiting)
                all_data += chunk
                print(f"        [{i*0.1:.1f}s] Received {len(chunk)} bytes (total: {len(all_data)})")
        
        if all_data:
            print(f"\n  [2.4] DATA RECEIVED PASSIVELY ({len(all_data)} bytes):")
            hexdump(all_data[:100], "        ")
            if len(all_data) > 100:
                print(f"        ... ({len(all_data) - 100} more bytes)")
        else:
            print(f"\n  [2.4] No passive data received (device silent)")
        
        # Clear buffer before sending
        ser.reset_input_buffer()
        
        # TEST A: Send PING
        print(f"\n  [2.5] Sending PING command...")
        ping = build_packet(MsgType.PING)
        print(f"        Building packet:")
        print(f"          Sync:     AA 55 CC")
        print(f"          Version:  01")
        print(f"          MsgType:  01 (PING)")
        print(f"          SeqNum:   01 00 (1)")
        print(f"          Length:   00 00 (0)")
        print(f"          Checksum: {ping[-1]:02x}")
        hexdump(ping, "        Full packet: ")
        
        bytes_written = ser.write(ping)
        ser.flush()
        print(f"\n        Written {bytes_written} bytes")
        
        print(f"\n        Waiting for PONG response (3s)...")
        response = b''
        for i in range(30):
            time.sleep(0.1)
            waiting = ser.in_waiting
            if waiting > 0:
                chunk = ser.read(waiting)
                response += chunk
                print(f"        [{i*0.1:.1f}s] Got {len(chunk)} bytes!")
                hexdump(chunk, "              ")
        
        if response:
            print(f"\n        RESPONSE ({len(response)} bytes):")
            hexdump(response, "        ")
            
            # Try to parse
            if len(response) >= 5 and response[0:3] == bytes([0xAA, 0x55, 0xCC]):
                print(f"\n        PARSED:")
                print(f"          Sync: VALID")
                print(f"          MsgType: {get_msg_name(response[4])}")
        else:
            print(f"\n        NO RESPONSE to PING")
        
        # TEST B: Send plain text (maybe firmware echoes?)
        print(f"\n  [2.6] Sending plain text 'HELLO\\r\\n'...")
        ser.reset_input_buffer()
        ser.write(b'HELLO\r\n')
        ser.flush()
        
        time.sleep(0.5)
        waiting = ser.in_waiting
        if waiting > 0:
            echo = ser.read(waiting)
            print(f"        Received {len(echo)} bytes:")
            hexdump(echo, "        ")
        else:
            print(f"        No echo response")
        
        # TEST C: Send newline (trigger any AT-style response?)
        print(f"\n  [2.7] Sending just '\\r\\n'...")
        ser.reset_input_buffer()
        ser.write(b'\r\n')
        ser.flush()
        
        time.sleep(0.5)
        waiting = ser.in_waiting
        if waiting > 0:
            data = ser.read(waiting)
            print(f"        Received {len(data)} bytes:")
            hexdump(data, "        ")
        else:
            print(f"        No response")
        
        ser.close()
        print(f"\n  [2.8] Port closed successfully")
        return True
        
    except serial.SerialException as e:
        print(f"\n  SERIAL ERROR: {e}")
        return False
    except Exception as e:
        print(f"\n  ERROR: {type(e).__name__}: {e}")
        import traceback
        traceback.print_exc()
        return False

def print_analysis():
    """Print analysis and suggestions."""
    print(f"\n" + "="*70)
    print("[INFO] ANALYSIS AND SUGGESTIONS")
    print("="*70)
    print("""
    FIRMWARE ARCHITECTURE:
    
    The GPU firmware (GPU.cpp) is designed for:
    - UART1 communication with CPU: GPIO12(TX), GPIO13(RX) @ 10 Mbps
    - The protocol expects CPU to send HUB75/OLED frames
    - It does NOT expect PC test commands!
    
    WHAT'S HAPPENING:
    
    1. When you connect USB to ESP32-S3, you get USB-CDC console
    2. USB-CDC is separate from UART1 (which talks to CPU)
    3. The GPU firmware probably sends ESP_LOG output to USB-CDC
    4. But it processes protocol commands only from UART1 (CPU)
    
    SOLUTIONS:
    
    Option A: MODIFY GPU FIRMWARE
      - Add protocol handling on USB-CDC for testing
      - Make GpuUartHandler also listen on USB
      
    Option B: USE CPU AS BRIDGE  
      - Connect PC to CPU via USB
      - CPU forwards commands to GPU via UART
      - This is how it's meant to work in production
      
    Option C: DIRECT UART TESTING
      - Use USB-UART adapter connected to GPIO12/13
      - Bypass USB-CDC entirely
      - Need level shifter if using 3.3V logic
    
    RECOMMENDED NEXT STEPS:
    
    1. Check GPU serial monitor - do you see ESP_LOG boot messages?
    2. If yes, USB-CDC works but firmware doesn't handle test protocol
    3. Look at GpuUartHandler.hpp - it only reads from UART1, not USB-CDC
    """)

def main():
    print("\n" + "="*70)
    print("  GPU DRIVER HARDWARE DEBUGGER v2.0")
    print("  Enhanced Serial Port Diagnostics")
    print("="*70)
    
    gpu_port, cpu_port = scan_ports()
    
    if gpu_port:
        test_port_detailed(gpu_port, "GPU")
    else:
        print("\n  WARNING: GPU port (COM5) not found")
    
    if cpu_port:
        test_port_detailed(cpu_port, "CPU")
    else:
        print("\n  WARNING: CPU port (COM15) not found")
    
    print_analysis()
    
    print("\n" + "="*70)
    print("  DEBUG COMPLETE")
    print("="*70 + "\n")

if __name__ == "__main__":
    main()
