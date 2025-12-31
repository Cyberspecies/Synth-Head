# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

# Select source file based on build configuration
# Priority: Environment variable > Build flags > Default
if(DEFINED ENV{MAIN_SOURCE_FILE})
    set(MAIN_SRCS "$ENV{MAIN_SOURCE_FILE}")
    message(STATUS "Building from environment: ${MAIN_SRCS}")
elseif(DEFINED MAIN_SOURCE_FILE)
    set(MAIN_SRCS "${MAIN_SOURCE_FILE}")
    message(STATUS "Building from CMake arg: ${MAIN_SRCS}")
elseif(DEFINED DUAL_DISPLAY_TEST)
    set(MAIN_SRCS "DualDisplayTest.cpp")
    message(STATUS "Building DUAL_DISPLAY_TEST")
elseif(DEFINED HUB75_TEST)
    set(MAIN_SRCS "HUB75Test.cpp")
    message(STATUS "Building HUB75_TEST")
elseif(DEFINED OLED_TEST)
    set(MAIN_SRCS "OLEDTest.cpp")
    message(STATUS "Building OLED_TEST")
else()
    set(MAIN_SRCS "GPU.cpp")
    message(STATUS "Building GPU default (GPU.cpp with managers)")
endif()

idf_component_register(SRCS ${MAIN_SRCS})
