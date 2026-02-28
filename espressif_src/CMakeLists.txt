# Top-level CMakeLists.txt for Piano Pedal Controller
# ESP-IDF Framework

cmake_minimum_required(VERSION 3.16.0)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(piano_pedal_controller
    VERSION 1.1.0
    DESCRIPTION "Three-Pedal Piano Controller with Hall Sensors and BLE"
    LANGUAGES C
)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
