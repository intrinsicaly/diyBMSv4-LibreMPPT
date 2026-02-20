# diyBMS MPPT Unit Tests

This directory contains the unit test suite for the diyBMS MPPT CAN bus
integration and packet processing modules, built with the
[Unity](https://github.com/ThrowTheSwitch/Unity) C test framework.

---

## Directory Structure

```
test/
├── mocks/
│   ├── Arduino.h              Mock Arduino runtime
│   ├── EmbeddedFiles_Defines.h  Stub (not needed in tests)
│   ├── EmbeddedFiles_Integrity.h Stub
│   ├── esp_log.h              Mock ESP logging macros
│   ├── esp_timer.h            Mock esp_timer_get_time()
│   ├── driver/
│   │   ├── twai.h             Mock TWAI / CAN driver types
│   │   └── uart.h             Mock UART driver types
│   ├── freertos/
│   │   ├── FreeRTOS.h         Mock FreeRTOS types
│   │   └── semphr.h           Mock semaphore + task functions
│   ├── mock_hal.h / .cpp      MockHAL – controllable timer & semaphore stubs
│   └── mock_canbus.h / .cpp   MockCANBus – recordable CAN transmit stub
├── unity/                     Unity framework (cloned at build time)
├── test_mppt_canbus.cpp       Tests for MPPTManager
├── test_packet_processor.cpp  Tests for PacketReceiveProcessor
├── test_main.cpp              Test runner (entry point)
├── CMakeLists.txt             Native build configuration
└── README.md                  This file
```

---

## Prerequisites

| Tool         | Version tested |
|--------------|---------------|
| gcc / g++    | 11 or later    |
| cmake        | 3.10 or later  |
| lcov         | any recent     |
| git          | any recent     |

On Debian/Ubuntu:

```bash
sudo apt-get install build-essential cmake lcov git
```

---

## Build and Run

```bash
cd ESPController/test

# 1 – Clone the Unity framework (once)
git clone --depth 1 https://github.com/ThrowTheSwitch/Unity.git unity

# 2 – Configure
mkdir -p build && cd build
cmake ..

# 3 – Build
make

# 4 – Run
./run_tests
```

Expected output:

```
=== MPPT CAN Bus Manager Tests ===
test_mppt_device_registration:PASS
test_mppt_duplicate_registration:PASS
...

=== Packet Processing Tests ===
test_packet_validation:PASS
...

-----------------------
20 Tests 0 Failures 0 Ignored
OK
```

---

## Coverage Report

```bash
# From the build directory:
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
# Open coverage_html/index.html in a browser
```

---

## Test Suites

### MPPT CAN Bus Tests (`test_mppt_canbus.cpp`)

| Test | What it verifies |
|------|-----------------|
| `test_mppt_device_registration` | Single device registered on first message |
| `test_mppt_duplicate_registration` | Duplicate CAN source does not create two entries |
| `test_mppt_max_devices` | Device list capped at `MAX_MPPT_DEVICES` |
| `test_mppt_device_discovery` | Discovery broadcast uses correct CAN ID |
| `test_mppt_telemetry_decode` | CBOR int16 temperature decoded correctly |
| `test_mppt_invalid_telemetry` | Invalid CBOR payload ignored, device stays online |
| `test_mppt_control_send` | `sendControl(false)` produces CBOR false frame |
| `test_mppt_control_enable` | `sendControl(true)` produces CBOR true frame |
| `test_mppt_timeout_handling` | Device marked `MPPT_TIMEOUT` after inactivity |
| `test_mppt_input_validation` | NULL message pointer handled safely |
| `test_mppt_canbus_send_failure` | CAN transmit failure handled without crash |
| `test_mppt_out_of_range_source_id` | IDs outside `[MPPT_ID_MIN, MPPT_ID_MAX]` ignored |
| `test_mppt_wrong_can_base` | Non-pub/sub CAN base IDs ignored |
| `test_mppt_init_null_pointers` | NULL settings/rules pointer handled safely |
| `test_mppt_invalid_dlc` | DLC > 8 rejected |

### Packet Processor Tests (`test_packet_processor.cpp`)

| Test | What it verifies |
|------|-----------------|
| `test_packet_validation` | Valid packet with correct CRC accepted |
| `test_packet_crc` | Packet with corrupted CRC rejected |
| `test_packet_buffer_overflow` | `start_address >= max` rejected |
| `test_packet_address_range` | `start_address > end_address` rejected |
| `test_packet_null_pointer` | NULL packet pointer handled safely |

---

## Mock Framework

### MockHAL

```cpp
MockHAL &hal = MockHAL::instance();
hal.reset();                    // call in setUp()
hal.setTime(1000000LL);         // set simulated µs clock
hal.advanceTime(500000LL);      // advance by 500 ms
hal.mutex_should_fail_take = true; // force semaphore failure
```

### MockCANBus

```cpp
MockCANBus &bus = MockCANBus::instance();
bus.reset();                        // call in setUp()
bus.should_fail_transmit = true;    // simulate CAN transmit error

int count = bus.getSentCount();
const SentCANMessage *m = bus.getLastSent();
// m->identifier, m->data[], m->length
```
