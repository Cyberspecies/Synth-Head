#!/usr/bin/env python3
"""
Debug Hardware Connection - Verbose diagnostic output
"""

import serial
import serial.tools.list_ports
import time
import struct

print("=" * 60)
print("  GPU DRIVER HARDWARE DEBUGGER")
print("=" * 60)

# Step 1: List all ports
print("\n[STEP 1] Scanning serial ports...")
ports = list(serial.tools.list_ports.comports())
for p in ports:
    print(f"  Found: {p.device} - {p.description} (hwid: {p.hwid})")

if not ports:
    print("  ERROR: No serial ports found!")
    exit(1)

# Step 2: Try to connect to GPU (COM5)
GPU_PORT = "COM5"
CPU_PORT = "COM15"

print(f"\n[STEP 2] Attempting to connect to GPU on {GPU_PORT}...")

# Try different baud rates
BAUD_RATES = [10000000, 2000000, 1000000, 921600, 115200]

for baud in BAUD_RATES:
    print(f"\n  Trying {baud} baud...", end=" ")
    try:
        ser = serial.Serial(
            port=GPU_PORT,
            baudrate=baud,
            timeout=0.1,
            write_timeout=0.1
        )
        print(f"CONNECTED!")
        
        # Flush any pending data
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        
        # Check what's in the buffer
        time.sleep(0.05)
        pending = ser.in_waiting
        print(f"    Bytes in RX buffer: {pending}")
        if pending > 0:
            data = ser.read(pending)
            print(f"    Data (hex): {data.hex()}")
            print(f"    Data (ascii): {data}")
        
        # Try sending a ping using the ACTUAL protocol from UartProtocol.hpp
        # Sync: 0xAA 0x55 0xCC
        # Version: 0x01
        # MsgType: 0x01 (PING)
        # SeqNum: 0x0001
        # Length: 0x0000 (no payload)
        # Checksum: calculated
        
        print("\n    Sending PING packet...")
        
        # Build packet
        sync = bytes([0xAA, 0x55, 0xCC])
        version = 0x01
        msg_type = 0x01  # PING
        seq_num = 1
        length = 0
        
        # Header without checksum
        header = struct.pack('<3sBBHH', sync, version, msg_type, seq_num, length)
        
        # Calculate checksum (XOR of all header bytes except sync)
        checksum = 0
        for b in header[3:]:  # Skip sync bytes
            checksum ^= b
        
        packet = header + bytes([checksum])
        
        print(f"    Packet (hex): {packet.hex()}")
        print(f"    Packet breakdown:")
        print(f"      Sync:     {sync.hex()}")
        print(f"      Version:  {version:02x}")
        print(f"      MsgType:  {msg_type:02x} (PING)")
        print(f"      SeqNum:   {seq_num:04x}")
        print(f"      Length:   {length:04x}")
        print(f"      Checksum: {checksum:02x}")
        
        # Send it
        bytes_sent = ser.write(packet)
        print(f"    Bytes sent: {bytes_sent}")
        
        # Wait for response
        print("    Waiting for response...")
        time.sleep(0.2)
        
        pending = ser.in_waiting
        print(f"    Bytes in RX buffer after wait: {pending}")
        
        if pending > 0:
            response = ser.read(pending)
            print(f"    Response (hex): {response.hex()}")
            print(f"    Response length: {len(response)}")
            
            # Try to parse it
            if len(response) >= 3:
                if response[0:3] == bytes([0xAA, 0x55, 0xCC]):
                    print("    *** VALID SYNC DETECTED! ***")
                    if len(response) >= 9:
                        ver = response[3]
                        mtype = response[4]
                        seq = struct.unpack('<H', response[5:7])[0]
                        plen = struct.unpack('<H', response[7:9])[0]
                        print(f"    Parsed: ver={ver}, type={mtype}, seq={seq}, len={plen}")
                        
                        # Type 0x02 = PONG
                        if mtype == 0x02:
                            print("    *** GOT PONG RESPONSE! ***")
                else:
                    print(f"    Sync bytes don't match: {response[0:3].hex()}")
        else:
            print("    No response received")
        
        # Try sending raw bytes to see if anything echoes
        print("\n    Sending raw test bytes [0x00, 0x01, 0x02, 0x03]...")
        ser.write(bytes([0x00, 0x01, 0x02, 0x03]))
        time.sleep(0.1)
        pending = ser.in_waiting
        print(f"    Response bytes: {pending}")
        if pending > 0:
            print(f"    Response: {ser.read(pending).hex()}")
        
        # Try simple protocol ping (old style)
        print("\n    Sending simple [0xAA, 0x55, 0x06, 0x00] ping...")
        ser.write(bytes([0xAA, 0x55, 0x06, 0x00]))  # PING with 0 length
        time.sleep(0.1)
        pending = ser.in_waiting
        print(f"    Response bytes: {pending}")
        if pending > 0:
            print(f"    Response: {ser.read(pending).hex()}")
        
        ser.close()
        print(f"\n  Closed connection to {GPU_PORT}")
        
    except serial.SerialException as e:
        print(f"FAILED: {e}")
    except Exception as e:
        print(f"ERROR: {e}")

# Step 3: Try CPU port
print(f"\n[STEP 3] Attempting to connect to CPU on {CPU_PORT}...")

try:
    ser = serial.Serial(
        port=CPU_PORT,
        baudrate=2000000,
        timeout=0.1
    )
    print("  CONNECTED!")
    
    # Check buffer
    time.sleep(0.05)
    pending = ser.in_waiting
    print(f"  Bytes in RX buffer: {pending}")
    if pending > 0:
        data = ser.read(pending)
        print(f"  Data (hex): {data.hex()}")
    
    ser.close()
    print(f"  Closed connection to {CPU_PORT}")
    
except serial.SerialException as e:
    print(f"  FAILED: {e}")

print("\n" + "=" * 60)
print("  DEBUG COMPLETE")
print("=" * 60)
