# diyBMS Unit Tests

## Overview

This directory contains host-side unit tests for the diyBMS ESP32 controller
firmware.  Tests run on a standard Linux host (no embedded hardware required)
using the [Unity](https://github.com/ThrowTheSwitch/Unity) C test framework and
a set of lightweight mock headers that replace ESP-IDF / Arduino / FreeRTOS
platform APIs.

## Directory Structure

```
test/
├── CMakeLists.txt              – CMake build configuration
├── README.md                   – This file
├── test_main.cpp               – Unity test runner (setUp/tearDown + main)
├── test_mppt_canbus.cpp        – MPPTManager unit tests
├── test_packet_processor.cpp   – PacketReceiveProcessor unit tests
├── test_canbus.cpp             – CAN bus mock / send function tests
└── mocks/
    ├── Arduino.h               – Arduino platform types, millis(), logging
    ├── esp_timer.h             – esp_timer_get_time() mock
    ├── driver/
    │   ├── twai.h              – TWAI (CAN bus) message types
    │   └── uart.h              – UART type definitions
    ├── freertos/
    │   ├── FreeRTOS.h          – FreeRTOS basic types and macros
    │   ├── semphr.h            – Semaphore functions
    │   └── task.h              – Task / notification functions
    ├── EmbeddedFiles_Defines.h – Empty placeholder (generated in real build)
    ├── EmbeddedFiles_Integrity.h – Empty placeholder
    ├── mock_hal.h / .cpp       – Hardware abstraction layer mock
    ├── mock_canbus.h / .cpp    – CAN bus mock and send_ext_canbus_message()
    ├── mock_settings.h         – make_test_settings() helper
    └── mock_rules.cpp          – Minimal Rules class stub for linking
```

## Setup

### Prerequisites

```bash
# Ubuntu / Debian
sudo apt-get install -y cmake build-essential lcov
```

### Clone Unity Framework

Unity is not bundled in the repository.  Clone it before building:

```bash
cd ESPController/test
git clone https://github.com/ThrowTheSwitch/Unity.git unity
```

### Build

```bash
cd ESPController/test
cmake -B build
cmake --build build --parallel
```

### Run Tests

```bash
./build/run_tests
```

### Generate Coverage Report

Build with coverage enabled then generate an HTML report:

```bash
cmake -B build -DENABLE_COVERAGE=ON
cmake --build build --parallel
./build/run_tests
cd build
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/test/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

Open `build/coverage_html/index.html` in your browser.

## Writing New Tests

1. Add your test function to one of the existing `test_*.cpp` files (or create
   a new one and add it to `CMakeLists.txt`).
2. Declare the function in `test_main.cpp` and add a `RUN_TEST()` call inside
   `main()`.
3. Use Unity assertion macros:

   | Macro | Description |
   |---|---|
   | `TEST_ASSERT_TRUE(cond)` | Fails if `cond` is false |
   | `TEST_ASSERT_FALSE(cond)` | Fails if `cond` is true |
   | `TEST_ASSERT_EQUAL(exp, act)` | Integer equality |
   | `TEST_ASSERT_EQUAL_UINT8(exp, act)` | uint8_t equality |
   | `TEST_ASSERT_EQUAL_UINT16(exp, act)` | uint16_t equality |
   | `TEST_ASSERT_EQUAL_UINT32(exp, act)` | uint32_t equality |
   | `TEST_ASSERT_EQUAL_HEX8(exp, act)` | uint8_t, printed in hex |
   | `TEST_ASSERT_EQUAL_INT16(exp, act)` | int16_t equality |
   | `TEST_ASSERT_NOT_NULL(ptr)` | Fails if pointer is NULL |
   | `TEST_ASSERT_GREATER_THAN(thresh, val)` | val > thresh |

4. Control mock state from your test:
   - `mock_esp_timer_value` – set the value returned by `esp_timer_get_time()`
   - `mock_millis_value` – set the value returned by `millis()`
   - `g_mock_canbus->should_fail_transmit` – simulate CAN send failure
   - `g_mock_canbus->transmitted_messages` – inspect sent CAN frames
   - `g_mock_semphr_fail` – make `xSemaphoreTake()` always return `pdFALSE`
   - `g_mock_hal->can_mutex_should_fail` – simulate HAL mutex timeout

## Continuous Integration

Tests run automatically via GitHub Actions (`.github/workflows/test.yml`) on:

- Every push to `main` / `develop`
- Every pull request targeting `main` / `develop`

The workflow installs dependencies, clones Unity, builds the test binary, runs
it, and (optionally) uploads a coverage report to Codecov.

Static analysis runs separately via `.github/workflows/static-analysis.yml`
using `cppcheck`.
