cmake_minimum_required(VERSION 3.14)
project(diybms_tests C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(ENABLE_COVERAGE "Build with gcov coverage instrumentation" OFF)

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------
set(COMMON_FLAGS "-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS}")

if(ENABLE_COVERAGE)
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   --coverage")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
endif()

# ---------------------------------------------------------------------------
# Unity via FetchContent
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(unity)

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
set(SRC_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/../src)
set(INC_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/../include)
set(LIB_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/../lib)
set(MOCKS_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/mocks)

# ---------------------------------------------------------------------------
# Include directories
# Mocks must appear FIRST so they shadow any real ESP32/Arduino headers.
# ---------------------------------------------------------------------------
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}  # test_globals.h and other test headers
    ${MOCKS_DIR}                 # mock Arduino.h, esp_log.h, esp_timer.h, ...
    ${MOCKS_DIR}/driver          # mock driver/twai.h, driver/uart.h
    ${MOCKS_DIR}/freertos        # mock freertos/FreeRTOS.h, freertos/semphr.h
    ${INC_DIR}                   # production headers (defines.h, mppt_canbus.h, ...)
    ${SRC_DIR}                   # production source headers (mppt_config.h, ...)
    ${LIB_DIR}/crc16             # CRC16 library header
)

# ---------------------------------------------------------------------------
# Mock sources
# ---------------------------------------------------------------------------
set(MOCK_SOURCES
    ${MOCKS_DIR}/mock_hal.cpp
    ${MOCKS_DIR}/mock_canbus.cpp
    ${MOCKS_DIR}/mock_rules.cpp
)

# ---------------------------------------------------------------------------
# Port sources (native test implementation of mppt_port.h)
# ---------------------------------------------------------------------------
set(PORT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ports/mppt_port_native.cpp
)

# ---------------------------------------------------------------------------
# Production sources under test
# ---------------------------------------------------------------------------
set(SUT_SOURCES
    ${SRC_DIR}/mppt_canbus.cpp
    ${SRC_DIR}/PacketReceiveProcessor.cpp
    ${LIB_DIR}/crc16/crc16.cpp
)

# ---------------------------------------------------------------------------
# Test sources
# ---------------------------------------------------------------------------
set(TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_globals.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mppt_canbus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_packet_processor.cpp
)

# ---------------------------------------------------------------------------
# Test executable
# ---------------------------------------------------------------------------
add_executable(run_tests
    ${MOCK_SOURCES}
    ${PORT_SOURCES}
    ${SUT_SOURCES}
    ${TEST_SOURCES}
)

target_link_libraries(run_tests unity m)

# Expose test-only hooks (e.g. sendWithRetry/calculateBackoffDelay) via
# #ifdef DIYBMS_MPPT_TESTING guards in production headers.
target_compile_definitions(run_tests PRIVATE DIYBMS_MPPT_TESTING)

if(ENABLE_COVERAGE)
    target_link_options(run_tests PRIVATE --coverage)
endif()

# ---------------------------------------------------------------------------
# CTest integration
# ---------------------------------------------------------------------------
enable_testing()
add_test(NAME unit_tests COMMAND run_tests)

