cmake_minimum_required(VERSION 3.10)
project(diybms_tests C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------
set(COMMON_FLAGS "-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${COMMON_FLAGS} --coverage")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS} --coverage")

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
set(SRC_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/../src)
set(INC_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/../include)
set(LIB_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/../lib)
set(MOCKS_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/mocks)
set(UNITY_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/unity)

# ---------------------------------------------------------------------------
# Include directories
# Mocks must appear FIRST so they shadow any real ESP32/Arduino headers.
# ---------------------------------------------------------------------------
include_directories(
    ${MOCKS_DIR}           # mock Arduino.h, esp_log.h, esp_timer.h, ...
    ${MOCKS_DIR}/driver    # mock driver/twai.h, driver/uart.h
    ${MOCKS_DIR}/freertos  # mock freertos/FreeRTOS.h, freertos/semphr.h
    ${INC_DIR}             # production headers (defines.h, mppt_canbus.h, ...)
    ${LIB_DIR}/crc16       # CRC16 library header
    ${UNITY_DIR}/src       # Unity framework
)

# ---------------------------------------------------------------------------
# Unity framework source
# ---------------------------------------------------------------------------
set(UNITY_SRC ${UNITY_DIR}/src/unity.c)

# ---------------------------------------------------------------------------
# Mock sources
# ---------------------------------------------------------------------------
set(MOCK_SOURCES
    ${MOCKS_DIR}/mock_hal.cpp
    ${MOCKS_DIR}/mock_canbus.cpp
)

# ---------------------------------------------------------------------------
# Production sources under test
# ---------------------------------------------------------------------------
set(SUT_SOURCES
    ${SRC_DIR}/mppt_canbus.cpp
    ${SRC_DIR}/PacketReceiveProcessor.cpp
    ${SRC_DIR}/Rules.cpp
    ${LIB_DIR}/crc16/crc16.cpp
)

# ---------------------------------------------------------------------------
# Test sources
# ---------------------------------------------------------------------------
set(TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mppt_canbus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_packet_processor.cpp
)

# ---------------------------------------------------------------------------
# Test executable
# ---------------------------------------------------------------------------
add_executable(run_tests
    ${UNITY_SRC}
    ${MOCK_SOURCES}
    ${SUT_SOURCES}
    ${TEST_SOURCES}
)

# Link coverage and standard math library
target_link_options(run_tests PRIVATE --coverage)
target_link_libraries(run_tests m)

# ---------------------------------------------------------------------------
# CTest integration
# ---------------------------------------------------------------------------
enable_testing()
add_test(NAME unit_tests COMMAND run_tests)
