# ESP-IDF Component CMakeLists.txt
# Conditional source file selection based on build target

# CPU Build selections
if(CPU_PROGRAMMABLE_TEST_MODE)
    set(SOURCE_FILE "CPU_Programmable_Test.cpp")
    message(STATUS "Building CPU Programmable Test")
elseif(CPU_MAIN_MODE)
    set(SOURCE_FILES 
        "CPU_Main.cpp"
        "Lifecycle/LifecycleController.cpp"
        "Modes/BootMode.cpp"
        "Modes/CurrentMode.cpp"
        "Application/Core/ApplicationCore.cpp"
        "Drivers/GpsDriver.cpp"
        "Drivers/MicDriver.cpp"
        "Drivers/ImuDriver.cpp"
        "Drivers/FanDriver.cpp"
        "Drivers/BmeDriver.cpp"
        "SystemAPI/GPU/GpuDriver.cpp"
        "HAL/led_strip_encoder.c"
    )
    message(STATUS "Building CPU Main (Lifecycle Architecture + Dual-Core Application Layer)")
elseif(CPU_MODE_TOGGLE_MODE)
    set(SOURCE_FILE "CPU_ModeToggle.cpp")
    message(STATUS "Building CPU Mode Toggle Test")
elseif(CPU_POLYGON_DEMO_MODE)
    set(SOURCE_FILE "CPU_PolygonDemo.cpp")
    message(STATUS "Building CPU Polygon Demo")
elseif(CPU_GPU_MINIMAL_TEST_MODE)
    set(SOURCE_FILE "CPU_GpuMinimalTest.cpp")
    message(STATUS "Building CPU GPU Minimal Test")
elseif(CPU_SPRITE_DEMO_MODE)
    set(SOURCE_FILE "CPU_SpriteDemo.cpp")
    message(STATUS "Building CPU Sprite Demo")
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
elseif(CPU_GPS_TEST_MODE)
    set(SOURCE_FILE "CPU_GpsTest.cpp")
    message(STATUS "Building CPU GPS Test")
elseif(CPU_GPU_SPRITE_TEST_MODE)
    set(SOURCE_FILE "CPU_GpuSpriteTest.cpp")
    message(STATUS "Building CPU GPU Sprite Test")
elseif(CPU_SPRITE_DRIVER_TEST_MODE)
    set(SOURCE_FILE "CPU_SpriteDriverTest.cpp")
    message(STATUS "Building CPU Sprite Driver Test (Full Pipeline Debug)")
elseif(CPU_SDCARD_GPU_TEST_MODE)
    set(SOURCE_FILE "CPU_SdCardGpuTest.cpp")
    message(STATUS "Building CPU SD Card + GPU Minimal Test")
elseif(CPU_FAN_TEST_MODE)
    set(SOURCE_FILE "CPU_FanTest.cpp")
    message(STATUS "Building CPU Fan Test")
elseif(CPU_SPRITE_TEST_STEP1)
    set(SOURCE_FILE "CPU_SpriteTest_Step1.cpp")
    message(STATUS "Building CPU Sprite Test Step 1 (Minimal GPU Test)")
elseif(CPU_SPRITE_TEST_STEP2)
    set(SOURCE_FILE "CPU_SpriteTest_Step2.cpp")
    message(STATUS "Building CPU Sprite Test Step 2 (WiFi + RAM)")
elseif(CPU_SPRITE_DEBUG_TEST)
    set(SOURCE_FILE "CPU_SpriteDebugTest.cpp")
    message(STATUS "Building CPU Sprite Debug Test (ULTRA VERBOSE)")
elseif(CPU_WIFI_UART_TEST)
    set(SOURCE_FILE "CPU_WifiUartTest.cpp")
    message(STATUS "Building CPU WiFi+UART Test")
elseif(CPU_GPU_DRIVER_TEST)
    set(SOURCE_FILES
        "CPU_GpuDriverTest.cpp"
        "SystemAPI/GPU/GpuDriver.cpp"
    )
    message(STATUS "Building CPU GPU Driver Test")
elseif(CPU_WIFI_SPRITE_UPLOAD_TEST)
    set(SOURCE_FILES
        "CPU_WifiSpriteUploadTest.cpp"
        "SystemAPI/GPU/GpuDriver.cpp"
    )
    message(STATUS "Building CPU WiFi Sprite Upload Test")
elseif(CPU_SIMPLE_SPRITE_TEST)
    set(SOURCE_FILES
        "CPU_SimpleSpriteTest.cpp"
        "SystemAPI/GPU/GpuDriver.cpp"
    )
    message(STATUS "Building CPU Simple Sprite Test")
elseif(CPU_SDCARD_TEST)
    set(SOURCE_FILE "CPU_SdCardTest.cpp")
    message(STATUS "Building CPU SD Card Test")
elseif(CPU_WEB_CONFIG_TEST)
    set(SOURCE_FILES
        "CPU_WebConfigTest.cpp"
        "SystemAPI/GPU/GpuDriver.cpp"
        "Drivers/ImuDriver.cpp"
    )
    message(STATUS "Building CPU Web Config Test")
elseif(CPU_OLED_TEST)
    set(SOURCE_FILES
        "CPU_OledTest.cpp"
        "SystemAPI/GPU/GpuDriver.cpp"
    )
    message(STATUS "Building CPU OLED Test")
elseif(CPU_OLED_ORIENTATION_TEST)
    set(SOURCE_FILES
        "CPU_OledOrientationTest.cpp"
        "SystemAPI/GPU/GpuDriver.cpp"
    )
    message(STATUS "Building CPU OLED Orientation Test")
elseif(CPU_SCENE_COMPOSER_TEST)
    set(SOURCE_FILES
        "CPU_SceneComposerTest.cpp"
        "SystemAPI/GPU/GpuDriver.cpp"
    )
    message(STATUS "Building CPU Scene Composer Test")
elseif(CPU_OLEDUI_TEST)
    set(SOURCE_FILES
        "CPU_OledUITest.cpp"
        "SystemAPI/GPU/GpuDriver.cpp"
    )
    message(STATUS "Building CPU OledUI Framework Test")
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

# Handle both single file and multi-file builds
if(DEFINED SOURCE_FILES)
    set(SRCS_TO_BUILD ${SOURCE_FILES})
else()
    set(SRCS_TO_BUILD ${SOURCE_FILE})
endif()

idf_component_register(
    SRCS ${SRCS_TO_BUILD}
    INCLUDE_DIRS 
        "." 
        "../include"
        "../ARCOS/ARCOS_library/include"
)

# Define platform macro for ARCOS library
target_compile_definitions(${COMPONENT_LIB} PRIVATE TARGET_ESP32_Wroom32S3_Module)
