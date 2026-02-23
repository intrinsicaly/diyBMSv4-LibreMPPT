/**
 * @file test_main.cpp
 * @brief Unity test runner for diyBMS MPPT CAN bus and packet processor tests
 *
 * Add every test function here using RUN_TEST().
 *
 * Build and run:
 *   cd ESPController/test
 *   mkdir -p build && cd build
 *   cmake ..
 *   make
 *   ./run_tests
 */

#include "unity.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * Globals required by the production code under test.
 * Defined here so they are available to all linked translation units.
 * ------------------------------------------------------------------------- */

#include "defines.h"
#include "Rules.h"

/** Global settings (referenced via extern in mppt_canbus.h) */
diybms_eeprom_settings mysettings;

/** Global rules object (referenced via extern in mppt_canbus.h) */
Rules rules;

/* ---------------------------------------------------------------------------
 * Forward declarations – MPPT CAN bus tests
 * ------------------------------------------------------------------------- */

void test_mppt_device_registration(void);
void test_mppt_duplicate_registration(void);
void test_mppt_max_devices(void);
void test_mppt_device_discovery(void);
void test_mppt_telemetry_decode(void);
void test_mppt_invalid_telemetry(void);
void test_mppt_control_send(void);
void test_mppt_control_enable(void);
void test_mppt_timeout_handling(void);
void test_mppt_input_validation(void);
void test_mppt_canbus_send_failure(void);
void test_mppt_out_of_range_source_id(void);
void test_mppt_wrong_can_base(void);
void test_mppt_init_null_pointers(void);
void test_mppt_invalid_dlc(void);

/* setUp/tearDown for MPPT tests are defined in test_mppt_canbus.cpp */
void setUp(void);
void tearDown(void);

/* ---------------------------------------------------------------------------
 * Forward declarations – Packet processor tests
 * ------------------------------------------------------------------------- */

void test_packet_validation(void);
void test_packet_crc(void);
void test_packet_buffer_overflow(void);
void test_packet_address_range(void);
void test_packet_null_pointer(void);

/* ---------------------------------------------------------------------------
 * Main
 * ------------------------------------------------------------------------- */

int main(void)
{
    UNITY_BEGIN();

    /* ---- MPPT CAN Bus Manager Tests ---- */
    printf("\n=== MPPT CAN Bus Manager Tests ===\n");

    RUN_TEST(test_mppt_device_registration);
    RUN_TEST(test_mppt_duplicate_registration);
    RUN_TEST(test_mppt_max_devices);
    RUN_TEST(test_mppt_device_discovery);
    RUN_TEST(test_mppt_telemetry_decode);
    RUN_TEST(test_mppt_invalid_telemetry);
    RUN_TEST(test_mppt_control_send);
    RUN_TEST(test_mppt_control_enable);
    RUN_TEST(test_mppt_timeout_handling);
    RUN_TEST(test_mppt_input_validation);
    RUN_TEST(test_mppt_canbus_send_failure);
    RUN_TEST(test_mppt_out_of_range_source_id);
    RUN_TEST(test_mppt_wrong_can_base);
    RUN_TEST(test_mppt_init_null_pointers);
    RUN_TEST(test_mppt_invalid_dlc);

    /* ---- Packet Processing Tests ---- */
    printf("\n=== Packet Processing Tests ===\n");

    RUN_TEST(test_packet_validation);
    RUN_TEST(test_packet_crc);
    RUN_TEST(test_packet_buffer_overflow);
    RUN_TEST(test_packet_address_range);
    RUN_TEST(test_packet_null_pointer);

    return UNITY_END();
}
