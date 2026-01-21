#!/usr/bin/env python3
"""
Serial Test Runner for Scene Test Harness
Sends TEST:FULL command and captures output
"""
import serial
import time
import sys

def run_tests():
    port = 'COM6'
    baud = 115200
    
    print(f"Opening {port} at {baud} baud...")
    try:
        ser = serial.Serial(port, baud, timeout=0.5)
    except Exception as e:
        print(f"Failed to open port: {e}")
        return
    
    # Clear any buffered data
    ser.reset_input_buffer()
    time.sleep(0.5)
    
    # Send TEST:FULL command
    print("\n" + "="*60)
    print("Sending TEST:FULL command...")
    print("="*60 + "\n")
    
    ser.write(b'TEST:FULL\n')
    ser.flush()
    
    # Read output for a longer time to capture all tests
    # Tests take several seconds to run
    test_timeout = 120  # 2 minutes for comprehensive tests
    start_time = time.time()
    output = []
    test_started = False
    test_finished = False
    lines_since_last_test_output = 0
    
    print("Reading test output...\n")
    
    while time.time() - start_time < test_timeout:
        try:
            line = ser.readline()
            if line:
                decoded = line.decode('utf-8', errors='replace').strip()
                if decoded:
                    print(decoded)
                    output.append(decoded)
                    
                    # Check if we're in test output
                    if 'SCENE_TEST' in decoded or 'TEST SUITE' in decoded or 'PASSED' in decoded or 'FAILED' in decoded:
                        test_started = True
                        lines_since_last_test_output = 0
                    elif test_started:
                        lines_since_last_test_output += 1
                    
                    # Check for final summary
                    if 'GRAND TOTAL' in decoded or 'ALL TESTS COMPLETE' in decoded:
                        test_finished = True
                        # Read a few more lines to get the final summary
                        for _ in range(20):
                            line = ser.readline()
                            if line:
                                decoded = line.decode('utf-8', errors='replace').strip()
                                if decoded:
                                    print(decoded)
                                    output.append(decoded)
                        break
                    
                    # If we've seen no test output for a while after tests started, we're done
                    if test_started and lines_since_last_test_output > 50:
                        break
                        
        except KeyboardInterrupt:
            print("\nInterrupted by user")
            break
        except Exception as e:
            print(f"Read error: {e}")
            continue
    
    ser.close()
    
    print("\n" + "="*60)
    if test_finished:
        print("TEST EXECUTION COMPLETE")
    else:
        print("TEST OUTPUT CAPTURE FINISHED (timeout or no more test output)")
    print("="*60)
    
    # Save output to file
    with open('test_output.log', 'w') as f:
        f.write('\n'.join(output))
    print(f"\nOutput saved to test_output.log")

if __name__ == '__main__':
    run_tests()
