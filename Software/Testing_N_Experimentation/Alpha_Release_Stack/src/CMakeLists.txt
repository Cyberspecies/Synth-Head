# ESP-IDF Component CMakeLists.txt
# Conditional source file selection based on build target

# CPU Build selections
if(CPU_PROGRAMMABLE_TEST_MODE)
    set(SOURCE_FILE "CPU_Programmable_Test.cpp")
    message(STATUS "Building CPU Programmable Test")
elseif(CPU_MODE_TOGGLE_MODE)
    set(SOURCE_FILE "CPU_ModeToggle.cpp")
    message(STATUS "Building CPU Mode Toggle Test")
elseif(CPU_POLYGON_DEMO_MODE)
    set(SOURCE_FILE "CPU_PolygonDemo.cpp")
    message(STATUS "Building CPU Polygon Demo")
elseif(CPU_GPUCOMMANDS_TEST_MODE)
    set(SOURCE_FILE "CPU_GpuCommands_Test.cpp")
    message(STATUS "Building CPU GpuCommands Wrapper Test")
elseif(CPU_FRAMEWORK_DINO_TEST_MODE)
    set(SOURCE_FILE "CPU_FrameworkAPI_DinoTest.cpp")
    message(STATUS "Building CPU FrameworkAPI Dino Test")
elseif(CPU_FRAMEWORK_ASTEROIDS_TEST_MODE)
    set(SOURCE_FILE "CPU_FrameworkAPI_AsteroidsTest.cpp")
    message(STATUS "Building CPU FrameworkAPI Asteroids Test")
elseif(CPU_FRAMEWORK_SANDSIM_MODE)
    set(SOURCE_FILE "CPU_FrameworkAPI_SandSim.cpp")
    message(STATUS "Building CPU FrameworkAPI Sand Simulation")
elseif(CPU_FULL_COMMAND_TEST_MODE)
    set(SOURCE_FILE "CPU_Full_Command_Test.cpp")
    message(STATUS "Building CPU Full Command Test")
# GPU Build selections
elseif(BAUD_TEST_MODE)
    set(SOURCE_FILE "GPU_BaudTest.cpp")
    message(STATUS "Building GPU Baud Rate Test")
elseif(SIMPLE_UART_TEST_MODE)
    set(SOURCE_FILE "GPU_Simple_UART.cpp")
    message(STATUS "Building GPU Simple UART Test")
elseif(HAL_TEST_MODE)
    set(SOURCE_FILE "GPU_HAL_Test.cpp")
    message(STATUS "Building GPU HAL Test")
elseif(BASE_API_TEST_MODE)
    set(SOURCE_FILE "GPU_BaseAPI_Test.cpp")
    message(STATUS "Building GPU Base API Test")
elseif(GPU_DRIVER_TEST_MODE)
    set(SOURCE_FILE "GPU_GpuDriver_Test.cpp")
    message(STATUS "Building GPU Driver Integration Test")
elseif(GPU_ADVANCED_DEMO_MODE)
    set(SOURCE_FILE "GPU_Advanced_Demo.cpp")
    message(STATUS "Building GPU Advanced Demo (SDF Morphing)")
elseif(GPU_OPTIMIZED_DEMO_MODE)
    set(SOURCE_FILE "GPU_Advanced_Demo_Optimized.cpp")
    message(STATUS "Building GPU Optimized Demo (60+ FPS)")
elseif(GPU_COMMAND_RENDERER_MODE)
    set(SOURCE_FILE "GPU_CommandRenderer.cpp")
    message(STATUS "Building GPU Command Renderer (Dual Panel + OLED)")
elseif(GPU_DISPLAY_SLAVE_MODE)
    set(SOURCE_FILE "GPU_DisplaySlave.cpp")
    message(STATUS "Building GPU Display Slave (Pure Receiver)")
elseif(GPU_PROGRAMMABLE_MODE)
    set(SOURCE_FILE "GPU_Programmable.cpp")
    message(STATUS "Building GPU Programmable (Shader VM)")
else()
    set(SOURCE_FILE "GPU.cpp")
    message(STATUS "Building GPU Main Application")
endif()

idf_component_register(
    SRCS "${SOURCE_FILE}"
    INCLUDE_DIRS 
        "." 
        "../include"
        "../ARCOS/ARCOS_library/include"
)

# Define platform macro for ARCOS library
target_compile_definitions(${COMPONENT_LIB} PRIVATE TARGET_ESP32_Wroom32S3_Module)
