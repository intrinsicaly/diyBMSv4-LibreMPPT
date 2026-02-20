cmake_minimum_required(VERSION 3.16)
project(diyBMS_Tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Unity test framework (cloned by CI or developer before running cmake)
# ---------------------------------------------------------------------------
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unity/src/unity.c")
    message(FATAL_ERROR
        "Unity framework not found. Please run:\n"
        "  cd ESPController/test && git clone https://github.com/ThrowTheSwitch/Unity.git unity\n"
        "before running cmake.")
endif()

add_subdirectory(unity)

# ---------------------------------------------------------------------------
# Include paths – mocks MUST come before project headers so that our
# platform-stub headers shadow the real ESP-IDF / Arduino headers.
# ---------------------------------------------------------------------------
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks           # Platform mocks (first!)
    ${CMAKE_CURRENT_SOURCE_DIR}/../include      # Project headers
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/crc16    # CRC16 library
    ${CMAKE_CURRENT_SOURCE_DIR}/unity/src       # Unity framework
)

# ---------------------------------------------------------------------------
# Test executable
# ---------------------------------------------------------------------------
add_executable(run_tests
    # Test runner and test suites
    test_main.cpp
    test_mppt_canbus.cpp
    test_packet_processor.cpp
    test_canbus.cpp

    # Production source files under test
    ../src/mppt_canbus.cpp
    ../src/PacketReceiveProcessor.cpp
    ../lib/crc16/crc16.cpp

    # Mock implementations
    mocks/mock_hal.cpp
    mocks/mock_canbus.cpp
    mocks/mock_rules.cpp
)

target_link_libraries(run_tests PRIVATE unity)

target_compile_definitions(run_tests PRIVATE
    UNIT_TEST=1
)

target_compile_options(run_tests PRIVATE
    -Wall
    -Wno-unused-parameter
    -Wno-unused-variable
)

# Optional: enable coverage instrumentation (activated by -DENABLE_COVERAGE=ON)
option(ENABLE_COVERAGE "Build with gcov coverage instrumentation" OFF)
if(ENABLE_COVERAGE)
    target_compile_options(run_tests PRIVATE --coverage -O0 -g)
    target_link_options(run_tests PRIVATE --coverage)
endif()

# ---------------------------------------------------------------------------
# CTest integration
# ---------------------------------------------------------------------------
enable_testing()
add_test(NAME AllTests COMMAND run_tests)
