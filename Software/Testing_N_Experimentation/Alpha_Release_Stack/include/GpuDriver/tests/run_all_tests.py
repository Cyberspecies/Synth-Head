#!/usr/bin/env python3
"""
GPU Driver Complete Test Suite
Runs all virtual and hardware tests in sequence
"""

import sys
import os
import time
import subprocess
from pathlib import Path
from typing import Tuple

# ============================================================
# Configuration
# ============================================================

TEST_DIR = Path(__file__).parent
VIRTUAL_TESTS = [
    "run_virtual_tests.py",
    "run_extended_tests.py"
]
HARDWARE_TEST = "run_hardware_tests.py"

# ============================================================
# Test Execution
# ============================================================

def run_test_suite(script_name: str, extra_args: list = None) -> Tuple[bool, float]:
    """Run a test suite and return (success, duration_seconds)"""
    script_path = TEST_DIR / script_name
    
    if not script_path.exists():
        print(f"  ERROR: Test script not found: {script_path}")
        return False, 0.0
    
    cmd = [sys.executable, str(script_path)]
    if extra_args:
        cmd.extend(extra_args)
    
    start = time.perf_counter()
    
    try:
        result = subprocess.run(
            cmd,
            cwd=TEST_DIR,
            capture_output=False,  # Show output in real-time
            text=True
        )
        duration = time.perf_counter() - start
        return result.returncode == 0, duration
    except Exception as e:
        duration = time.perf_counter() - start
        print(f"  ERROR: Failed to run {script_name}: {e}")
        return False, duration

def main():
    print("=" * 70)
    print("        GPU DRIVER COMPLETE TEST SUITE")
    print("=" * 70)
    print(f"Python: {sys.version}")
    print(f"Working Directory: {TEST_DIR}")
    print()
    
    results = []
    total_start = time.perf_counter()
    
    # Virtual Tests
    print("=" * 70)
    print("                    VIRTUAL TESTS")
    print("=" * 70)
    
    for script in VIRTUAL_TESTS:
        print(f"\n>>> Running: {script}")
        print("-" * 50)
        success, duration = run_test_suite(script)
        results.append((script, success, duration))
        print(f"\n>>> {script}: {'PASSED' if success else 'FAILED'} ({duration:.2f}s)")
    
    # Hardware Tests
    print("\n" + "=" * 70)
    print("                   HARDWARE TESTS")
    print("=" * 70)
    
    # Check if hardware tests should be skipped
    skip_hw = "--skip-hw" in sys.argv
    
    if skip_hw:
        print("\n>>> Hardware tests skipped (--skip-hw)")
        results.append((HARDWARE_TEST, True, 0.0))  # Mark as passed when skipped
    else:
        print(f"\n>>> Running: {HARDWARE_TEST}")
        print("-" * 50)
        success, duration = run_test_suite(HARDWARE_TEST)
        results.append((HARDWARE_TEST, success, duration))
        print(f"\n>>> {HARDWARE_TEST}: {'PASSED' if success else 'FAILED'} ({duration:.2f}s)")
    
    # Summary
    total_duration = time.perf_counter() - total_start
    
    print("\n" + "=" * 70)
    print("                     SUMMARY")
    print("=" * 70)
    
    total_suites = len(results)
    passed_suites = sum(1 for _, success, _ in results if success)
    failed_suites = total_suites - passed_suites
    
    print(f"\nTest Suites Run:    {total_suites}")
    print(f"Test Suites Passed: {passed_suites}")
    print(f"Test Suites Failed: {failed_suites}")
    print(f"Total Time:         {total_duration:.2f}s")
    
    print("\nIndividual Results:")
    for script, success, duration in results:
        status = "✓ PASS" if success else "✗ FAIL"
        print(f"  {status}  {script:30s} ({duration:.2f}s)")
    
    print("\n" + "=" * 70)
    if failed_suites == 0:
        print("         *** ALL TEST SUITES PASSED ***")
    else:
        print("         *** SOME TEST SUITES FAILED ***")
    print("=" * 70)
    
    return 0 if failed_suites == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
