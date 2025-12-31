# ESP-IDF Component CMakeLists.txt
# Only include GPU.cpp for ESP-IDF builds

idf_component_register(
    SRCS "GPU.cpp"
    INCLUDE_DIRS 
        "." 
        "../include"
        "../ARCOS/ARCOS_library/include"
)

# Define platform macro for ARCOS library
target_compile_definitions(${COMPONENT_LIB} PRIVATE TARGET_ESP32_Wroom32S3_Module)
