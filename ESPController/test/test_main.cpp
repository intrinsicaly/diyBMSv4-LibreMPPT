/*
 * Unity test runner for diyBMS
 *
 * setUp() and tearDown() are called by Unity automatically before/after
 * every test.  They create and destroy the global mock objects shared
 * across all test suites.
 */

#include "unity.h"
#include "mock_hal.h"
#include "mock_canbus.h"
#include "defines.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

/* ---- Global symbols required by production code ---- */

/* Cell module info array (PacketReceiveProcessor.cpp) */
CellModuleInfo cmi[maximum_controller_cell_modules];

/* Snapshot task handle (PacketReceiveProcessor.cpp) */
TaskHandle_t voltageandstatussnapshot_task_handle = nullptr;

/* Settings and rules instances (mppt_canbus.h extern declarations) */
#include "defines.h"
#include "Rules.h"
diybms_eeprom_settings mysettings;
Rules rules;

/* Mock global state (global timer and millis counter) */
int64_t  mock_esp_timer_value = 0;
uint32_t mock_millis_value    = 0;
bool     g_mock_semphr_fail   = false;

/* ---- Unity setUp / tearDown ---- */

void setUp(void)
{
    /* Reset controllable globals */
    mock_esp_timer_value = 0;
    mock_millis_value    = 0;
    g_mock_semphr_fail   = false;

    /* Reset cell module info */
    memset(cmi, 0, sizeof(cmi));

    /* Fresh mock objects; their constructors set g_mock_hal / g_mock_canbus */
    static MockHAL    *hal    = nullptr;
    static MockCANBus *canbus = nullptr;

    delete canbus; canbus = nullptr;
    delete hal;    hal    = nullptr;

    hal    = new MockHAL();
    canbus = new MockCANBus();
}

void tearDown(void)
{
    /* Mocks are cleaned up at next setUp to keep pointers valid during tests */
}

/* ---- External test function declarations ---- */

/* MPPT CAN Bus Manager Tests */
extern void test_mppt_device_registration(void);
extern void test_mppt_device_discovery(void);
extern void test_mppt_telemetry_decode(void);
extern void test_mppt_control_send(void);
extern void test_mppt_timeout_handling(void);
extern void test_mppt_max_devices(void);
extern void test_mppt_input_validation(void);

/* Packet Processing Tests */
extern void test_packet_validation(void);
extern void test_packet_crc(void);
extern void test_packet_buffer_overflow(void);
extern void test_packet_address_range(void);

/* CAN Bus Tests */
extern void test_canbus_send_success(void);
extern void test_canbus_send_failure(void);
extern void test_canbus_mutex_timeout(void);

/* ---- Main ---- */

int main(void)
{
    UNITY_BEGIN();

    printf("\n=== MPPT CAN Bus Manager Tests ===\n");
    RUN_TEST(test_mppt_device_registration);
    RUN_TEST(test_mppt_device_discovery);
    RUN_TEST(test_mppt_telemetry_decode);
    RUN_TEST(test_mppt_control_send);
    RUN_TEST(test_mppt_timeout_handling);
    RUN_TEST(test_mppt_max_devices);
    RUN_TEST(test_mppt_input_validation);

    printf("\n=== Packet Processing Tests ===\n");
    RUN_TEST(test_packet_validation);
    RUN_TEST(test_packet_crc);
    RUN_TEST(test_packet_buffer_overflow);
    RUN_TEST(test_packet_address_range);

    printf("\n=== CAN Bus Communication Tests ===\n");
    RUN_TEST(test_canbus_send_success);
    RUN_TEST(test_canbus_send_failure);
    RUN_TEST(test_canbus_mutex_timeout);

    return UNITY_END();
}
