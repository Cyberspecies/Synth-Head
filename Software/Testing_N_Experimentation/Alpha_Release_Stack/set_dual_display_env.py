"""
PlatformIO pre-build script to set environment variable for dual display test
"""
Import("env")
import os

# Set environment variable to select dual display test source
os.environ['DUAL_DISPLAY_TEST'] = '1'
print("Environment variable DUAL_DISPLAY_TEST set for CMake")
