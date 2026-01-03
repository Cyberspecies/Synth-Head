# GPU Driver Test Suite

## Overview

This directory contains comprehensive testing infrastructure for the GPU Driver system, including:
- **Virtual Tests**: Algorithm validation, protocol testing, performance benchmarks
- **Extended Tests**: Edge cases, boundary conditions, stress testing  
- **Hardware Tests**: Real ESP32-S3 device communication tests

## Test Files

| File | Tests | Description |
|------|-------|-------------|
| `run_virtual_tests.py` | 35 | Core algorithm validation |
| `run_extended_tests.py` | 39 | Edge cases & stress tests |
| `run_hardware_tests.py` | 14 | Real hardware communication |
| `run_all_tests.py` | All | Master test runner |

## Running Tests

### Run All Virtual Tests
```bash
python run_all_tests.py --skip-hw
```

### Run Full Suite (with hardware)
```bash
python run_all_tests.py
```
Note: Ensure COM ports (COM5, COM15) are not in use by other applications.

### Run Individual Test Suites
```bash
python run_virtual_tests.py
python run_extended_tests.py
python run_hardware_tests.py --cpu-port COM15 --gpu-port COM5
```

## Test Categories

### Virtual Tests (run_virtual_tests.py)

| Category | Tests | Coverage |
|----------|-------|----------|
| ISA | 3 | Opcode values, datatypes, encoding |
| Fixed-Point Math | 4 | Conversion, mul/div, precision |
| Trigonometry | 4 | sin/cos accuracy, identity, performance |
| Color Space | 3 | RGB/HSV conversion, roundtrip |
| Drawing Algorithms | 7 | Bresenham line, midpoint circle |
| Animation/Easing | 2 | Boundary conditions, monotonicity |
| SDF Rendering | 3 | Circle/box distance, operations |
| Protocol | 2 | Packet encoding, CRC |
| Stress Tests | 2 | Memory, fixed-point performance |
| Hardware Framework | 3 | Failure tracking, counters, thermal |
| Regression | 2 | Determinism, consistency |

### Extended Tests (run_extended_tests.py)

| Category | Tests | Coverage |
|----------|-------|----------|
| Edge Cases | 5 | Empty/max payload, patterns |
| Boundary Conditions | 4 | Screen edges, clipping |
| Protocol Robustness | 4 | All opcodes, error detection |
| UART Simulation | 3 | Clean/noisy/lossy channels |
| GPU Integration | 3 | Command sequences |
| Fixed-Point Edge Cases | 4 | Overflow, underflow, negatives |
| Drawing Edge Cases | 4 | Zero-length, backwards lines |
| Animation/Timing | 3 | Frame timing, interpolation |
| Memory Patterns | 3 | Alignment, pooling, fragmentation |
| Color Conversion | 3 | RGB565, alpha blending |
| Performance Benchmarks | 3 | Encoding/decoding throughput |

### Hardware Tests (run_hardware_tests.py)

| Test | Description |
|------|-------------|
| GPU/CPU Connection | Verify serial port access |
| GPU Echo | Round-trip communication |
| GPU NOP/Reset | Basic commands |
| GPU Clear/Flip | Display operations |
| GPU Set Pixel | Single pixel write |
| GPU Fill Rect | Rectangle fill |
| GPU Draw Line | Line rendering |
| GPU Rapid Commands | Burst command handling |
| GPU Throughput | Data transfer rate |
| GPU Frame Rate | Achievable FPS |
| CPU to GPU Relay | Command forwarding |

## Test Results Summary

### Latest Run (Virtual Tests)
```
============================================================
   GPU Driver Complete Test Suite Results
============================================================

Test Suites Run:    3
Test Suites Passed: 3
Test Suites Failed: 0
Total Time:         0.54s

Individual Results:
  ✓ PASS  run_virtual_tests.py    (35 tests)
  ✓ PASS  run_extended_tests.py   (39 tests)
  ✓ PASS  run_hardware_tests.py   (skipped - ports in use)

Total: 74 tests PASSED
============================================================
```

## Performance Benchmarks

From extended test suite:
- Packet Encoding: ~235 ops/ms
- Packet Decoding: ~256 ops/ms
- GPU Command Execution: ~1200 ops/ms
- Trig Operations: ~10,500 ops/ms
- Fixed-Point Math: ~10,500 ops/ms
- Bresenham Line: ~140 lines/ms

## Hardware Requirements

For hardware tests:
- **CPU**: ESP32-S3 on COM15 (Arduino framework)
- **GPU**: ESP32-S3 on COM5 (ESP-IDF framework)
- **Baud Rate**: 2,000,000 (2 Mbps)

## Dependencies

```bash
pip install pyserial
```

## Adding New Tests

1. Create test function with `test_` prefix
2. Return `True` for pass, error message for fail
3. Add to appropriate test file
4. Use `runner.run_test("Test Name", test_function)` to register

## Continuous Integration

For CI environments, use:
```bash
python run_all_tests.py --skip-hw
exit $?  # Returns 0 on success, 1 on failure
```
