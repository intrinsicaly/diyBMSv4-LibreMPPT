/*
 * Unit tests for CAN bus mock and send_ext_canbus_message()
 *
 * setUp / tearDown live in test_main.cpp.
 */

#include "unity.h"
#include "mock_hal.h"
#include "mock_canbus.h"
#include <cstring>

/* Extern symbols defined in test_main.cpp */
extern bool g_mock_semphr_fail;

/* ------------------------------------------------------------------ */
/* Tests                                                               */
/* ------------------------------------------------------------------ */

/**
 * Test: CAN Bus Send Success
 * send_ext_canbus_message() records the transmitted frame in the mock.
 */
void test_canbus_send_success(void)
{
    g_mock_canbus->Reset();

    const uint8_t data[] = {0x01, 0x02, 0x03};
    bool result = send_ext_canbus_message(0x1E001234UL, data, sizeof(data));

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_INT(1, (int)g_mock_canbus->transmitted_messages.size());

    const twai_message_t &tx = g_mock_canbus->transmitted_messages[0];
    TEST_ASSERT_EQUAL_UINT32(0x1E001234UL, tx.identifier);
    TEST_ASSERT_EQUAL_UINT8(sizeof(data), tx.data_length_code);
    TEST_ASSERT_EQUAL_HEX8(0x01, tx.data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x02, tx.data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x03, tx.data[2]);
    TEST_ASSERT_EQUAL_HEX8(TWAI_MSG_FLAG_EXTD, tx.flags & TWAI_MSG_FLAG_EXTD);
}

/**
 * Test: CAN Bus Send Failure
 * When the mock is configured to reject transmits, send_ext_canbus_message()
 * returns false and records no frame.
 */
void test_canbus_send_failure(void)
{
    g_mock_canbus->Reset();
    g_mock_canbus->should_fail_transmit = true;

    const uint8_t data[] = {0xAA};
    bool result = send_ext_canbus_message(0x1E001234UL, data, sizeof(data));

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_INT(0, (int)g_mock_canbus->transmitted_messages.size());
}

/**
 * Test: HAL Mutex Acquisition and Timeout
 * Exercises MockHAL::GetCANMutex() in both the success and the
 * configured-to-fail paths.
 */
void test_canbus_mutex_timeout(void)
{
    g_mock_hal->Reset();

    /* Normal acquisition */
    bool acquired = g_mock_hal->GetCANMutex(100);
    TEST_ASSERT_TRUE(acquired);
    TEST_ASSERT_TRUE(g_mock_hal->can_mutex_acquired);
    TEST_ASSERT_EQUAL_UINT32(100, g_mock_hal->can_mutex_timeout_ms);

    g_mock_hal->ReleaseCANMutex();
    TEST_ASSERT_FALSE(g_mock_hal->can_mutex_acquired);

    /* Simulated timeout / failure */
    g_mock_hal->can_mutex_should_fail = true;
    bool result = g_mock_hal->GetCANMutex(50);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_FALSE(g_mock_hal->can_mutex_acquired);
}
