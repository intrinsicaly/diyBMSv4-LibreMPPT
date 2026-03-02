/**
 * @file test_mppt_canbus.cpp
 * @brief Unit tests for the MPPTManager class
 *
 * Tests cover:
 *  - Device registration (normal, duplicate, max devices)
 *  - Device discovery broadcasting
 *  - Telemetry decoding (valid and invalid CBOR)
 *  - Control message sending
 *  - Timeout handling
 *  - Input validation
 *  - CAN bus send failure handling
 */

#include "unity.h"
#include "test_globals.h"
#include "mocks/mock_hal.h"
#include "mocks/mock_canbus.h"
#include "mppt_canbus.h"
#include "mppt_config.h"

#include <string.h>

/* ---------------------------------------------------------------------------
 * Shared test fixtures
 * ------------------------------------------------------------------------- */

/** Settings used across MPPT tests */
static diybms_eeprom_settings g_settings;

/** Rules object used across MPPT tests */
static Rules g_rules;

/** Fresh MPPTManager created per test to keep tests independent */
static MPPTManager *g_mgr = nullptr;

/**
 * Build a valid pub/sub CAN message.
 *
 * Standard CAN (ISO 11898) limits data to 8 bytes (DLC 0-8).
 * The code rejects messages with data_length_code > 8.
 */
static twai_message_t make_pubsub_msg(uint16_t source_id,
                                      const uint8_t *payload, uint8_t len)
{
    twai_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.identifier       = THINGSET_PUBSUB_BASE | (uint32_t)source_id;
    msg.data_length_code = len;
    msg.extd             = true;
    if (payload && len > 0) {
        memcpy(msg.data, payload, len <= 8 ? len : 8);
    }
    return msg;
}

/**
 * Encode a 7-byte CBOR int16 message suitable for a standard CAN frame.
 *
 * Format: 0xA1 0x19 <ID_HI> <ID_LO> 0x19 <VAL_HI> <VAL_LO>
 */
static void encode_cbor_int16(uint8_t *buf, uint16_t obj_id, int16_t value)
{
    buf[0] = 0xA1;                          /* map(1) */
    buf[1] = 0x19;                          /* uint16 key follows */
    buf[2] = (obj_id >> 8) & 0xFF;
    buf[3] = obj_id & 0xFF;
    buf[4] = 0x19;                          /* int16 value follows */
    buf[5] = (uint8_t)((value >> 8) & 0xFF);
    buf[6] = (uint8_t)(value & 0xFF);
}

/**
 * Encode a 5-byte CBOR small-uint message (value 0x00-0x17).
 *
 * Format: 0xA1 0x19 <ID_HI> <ID_LO> <VALUE>
 */
static void encode_cbor_small_uint(uint8_t *buf, uint16_t obj_id, uint8_t value)
{
    buf[0] = 0xA1;
    buf[1] = 0x19;
    buf[2] = (obj_id >> 8) & 0xFF;
    buf[3] = obj_id & 0xFF;
    buf[4] = value & 0x17; /* clamp to small-uint range */
}

/**
 * Encode a 9-byte CBOR float32 message for CAN FD frames.
 *
 * Format: 0xA1 0x19 <ID_HI> <ID_LO> 0xFA <F3> <F2> <F1> <F0>
 * (float is big-endian in CBOR, little-endian in memory on this CPU)
 */
static void encode_cbor_float32(uint8_t *buf, uint16_t obj_id, float value)
{
    buf[0] = 0xA1;                          /* map(1) */
    buf[1] = 0x19;                          /* uint16 key follows */
    buf[2] = (obj_id >> 8) & 0xFF;
    buf[3] = obj_id & 0xFF;
    buf[4] = 0xFA;                          /* float32 follows */
    uint32_t raw;
    memcpy(&raw, &value, 4);
    raw = __builtin_bswap32(raw);           /* to big-endian */
    memcpy(&buf[5], &raw, 4);              /* copy big-endian bytes */
}

/**
 * Build a 9-byte float32 pub/sub CAN message (DLC=9, requires extended mock).
 */
static twai_message_t make_float32_msg(uint16_t source_id,
                                       uint16_t obj_id, float value)
{
    twai_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.identifier       = THINGSET_PUBSUB_BASE | (uint32_t)source_id;
    msg.data_length_code = 9;
    msg.extd             = true;
    encode_cbor_float32(msg.data, obj_id, value);
    return msg;
}



void setUp(void)
{
    MockHAL::instance().reset();
    MockCANBus::instance().reset();

    memset(&g_settings, 0, sizeof(g_settings));
    g_settings.mppt_can_enabled        = true;
    g_settings.mppt_target_voltage     = 5600;
    g_settings.mppt_max_charge_current = 200;
    g_settings.mppt_timeout_seconds    = 60;

    g_mgr = new MPPTManager();
    g_mgr->init(&g_settings, &g_rules);

    /* Clear shared CellModuleInfo array used by PacketReceiveProcessor tests */
    memset(cmi, 0, sizeof(CellModuleInfo) * maximum_controller_cell_modules);
}

void tearDown(void)
{
    delete g_mgr;
    g_mgr = nullptr;
}

/* ---------------------------------------------------------------------------
 * Test 1: register a single new device
 * ------------------------------------------------------------------------- */

void test_mppt_device_registration(void)
{
    /* 5-byte CBOR small-uint: charge state = 3 */
    uint8_t payload[5];
    encode_cbor_small_uint(payload, THINGSET_ID_STATE, 3);

    twai_message_t msg = make_pubsub_msg(THINGSET_MPPT_ID_MIN, payload, 5);
    g_mgr->processReceivedMessage(&msg);

    TEST_ASSERT_EQUAL_UINT8(1, g_mgr->getDeviceCount());

    const MPPTDevice *dev = g_mgr->getDevice(0);
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_UINT16(THINGSET_MPPT_ID_MIN, dev->node_id);
    TEST_ASSERT_EQUAL_INT(MPPT_ONLINE, dev->status);
}

/* ---------------------------------------------------------------------------
 * Test 2: duplicate registration – same device must only appear once
 * ------------------------------------------------------------------------- */

void test_mppt_duplicate_registration(void)
{
    uint8_t payload[5];
    encode_cbor_small_uint(payload, THINGSET_ID_STATE, 3);
    twai_message_t msg = make_pubsub_msg(THINGSET_MPPT_ID_MIN, payload, 5);

    g_mgr->processReceivedMessage(&msg);
    g_mgr->processReceivedMessage(&msg); /* send the same message twice */

    TEST_ASSERT_EQUAL_UINT8(1, g_mgr->getDeviceCount());
}

/* ---------------------------------------------------------------------------
 * Test 3: MAX_MPPT_DEVICES limit is enforced
 * ------------------------------------------------------------------------- */

void test_mppt_max_devices(void)
{
    uint8_t payload[5];
    encode_cbor_small_uint(payload, THINGSET_ID_STATE, 3);

    for (uint16_t id = THINGSET_MPPT_ID_MIN;
         id < THINGSET_MPPT_ID_MIN + MAX_MPPT_DEVICES + 1;
         id++)
    {
        twai_message_t msg = make_pubsub_msg(id, payload, 5);
        g_mgr->processReceivedMessage(&msg);
    }

    TEST_ASSERT_EQUAL_UINT8(MAX_MPPT_DEVICES, g_mgr->getDeviceCount());
}

/* ---------------------------------------------------------------------------
 * Test 4: device discovery broadcast sends a CAN message
 * ------------------------------------------------------------------------- */

void test_mppt_device_discovery(void)
{
    /* Advance time past DISCOVERY_INTERVAL_US (30 seconds) */
    MockHAL::instance().setTime(31LL * 1000000LL);

    g_mgr->update();

    TEST_ASSERT_TRUE(MockCANBus::instance().getSentCount() >= 1);

    const SentCANMessage *m = MockCANBus::instance().getSentAt(0);
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL_UINT32(THINGSET_BROADCAST_ID, m->identifier & 0xFFFF);
    TEST_ASSERT_EQUAL_UINT32(THINGSET_REQRESP_BASE, m->identifier & 0xFF000000UL);
}

/* ---------------------------------------------------------------------------
 * Test 5: telemetry decode – valid CBOR int16 (temperature)
 * ------------------------------------------------------------------------- */

void test_mppt_telemetry_decode(void)
{
    const int16_t expected_temp = 35;
    uint8_t payload[7];
    encode_cbor_int16(payload, THINGSET_ID_TEMP, expected_temp);

    twai_message_t msg = make_pubsub_msg(THINGSET_MPPT_ID_MIN, payload, 7);
    g_mgr->processReceivedMessage(&msg);

    const MPPTDevice *dev = g_mgr->getDevice(0);
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_INT16(expected_temp, dev->temperature);
}

/* ---------------------------------------------------------------------------
 * Test 6: invalid CBOR (wrong map marker) must not crash or corrupt state
 * ------------------------------------------------------------------------- */

void test_mppt_invalid_telemetry(void)
{
    /* Garbage payload – first byte is not 0xA1 */
    uint8_t payload[5] = {0xFF, 0x00, 0x00, 0x00, 0x00};

    twai_message_t msg = make_pubsub_msg(THINGSET_MPPT_ID_MIN, payload, 5);
    g_mgr->processReceivedMessage(&msg);

    /* Device still registered (we just can't decode the telemetry) */
    TEST_ASSERT_EQUAL_UINT8(1, g_mgr->getDeviceCount());
    const MPPTDevice *dev = g_mgr->getDevice(0);
    TEST_ASSERT_NOT_NULL(dev);
    /* Temperature should remain at zero-initialised default */
    TEST_ASSERT_EQUAL_INT16(0, dev->temperature);
}

/* ---------------------------------------------------------------------------
 * Test 7: sendControl sends a properly formatted CAN frame
 * ------------------------------------------------------------------------- */

void test_mppt_control_send(void)
{
    MockCANBus::instance().reset();

    bool ok = g_mgr->sendControl(THINGSET_MPPT_ID_MIN, false);
    TEST_ASSERT_TRUE(ok);

    TEST_ASSERT_EQUAL_INT(1, MockCANBus::instance().getSentCount());

    const SentCANMessage *m = MockCANBus::instance().getLastSent();
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL_UINT32(THINGSET_REQRESP_BASE, m->identifier & 0xFF000000UL);
    TEST_ASSERT_EQUAL_UINT16(THINGSET_MPPT_ID_MIN, (uint16_t)(m->identifier & 0xFFFF));
    /* Payload: map(1) 0xA1, uint16 0x19, ID hi/lo, CBOR false 0xF4 */
    TEST_ASSERT_EQUAL_UINT8(0xA1, m->data[0]);
    TEST_ASSERT_EQUAL_UINT8(0xF4, m->data[4]); /* CBOR false */
}

/* ---------------------------------------------------------------------------
 * Test 8: sendControl with enable=true sends CBOR true
 * ------------------------------------------------------------------------- */

void test_mppt_control_enable(void)
{
    bool ok = g_mgr->sendControl(THINGSET_MPPT_ID_MIN, true);
    TEST_ASSERT_TRUE(ok);

    const SentCANMessage *m = MockCANBus::instance().getLastSent();
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL_UINT8(0xF5, m->data[4]); /* CBOR true */
}

/* ---------------------------------------------------------------------------
 * Test 9: timeout handling – device goes from ONLINE to TIMEOUT
 * ------------------------------------------------------------------------- */

void test_mppt_timeout_handling(void)
{
    /* Register a device at t=0 */
    MockHAL::instance().setTime(0);

    uint8_t payload[5];
    encode_cbor_small_uint(payload, THINGSET_ID_STATE, 3);
    twai_message_t msg = make_pubsub_msg(THINGSET_MPPT_ID_MIN, payload, 5);
    g_mgr->processReceivedMessage(&msg);

    const MPPTDevice *dev = g_mgr->getDevice(0);
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_INT(MPPT_ONLINE, dev->status);

    /* Advance time past the 60-second timeout */
    MockHAL::instance().setTime(61LL * 1000000LL);
    g_mgr->update();

    TEST_ASSERT_EQUAL_INT(MPPT_TIMEOUT, dev->status);
}

/* ---------------------------------------------------------------------------
 * Test 10: input validation – NULL message pointer must not crash
 * ------------------------------------------------------------------------- */

void test_mppt_input_validation(void)
{
    g_mgr->processReceivedMessage(nullptr);
    TEST_ASSERT_EQUAL_UINT8(0, g_mgr->getDeviceCount());
}

/* ---------------------------------------------------------------------------
 * Test 11: CAN bus send failure is handled gracefully
 * ------------------------------------------------------------------------- */

void test_mppt_canbus_send_failure(void)
{
    MockCANBus::instance().should_fail_transmit = true;

    bool ok = g_mgr->sendControl(THINGSET_MPPT_ID_MIN, true);
    TEST_ASSERT_TRUE(ok);

    TEST_ASSERT_EQUAL_INT(0, MockCANBus::instance().getSentCount());
}

/* ---------------------------------------------------------------------------
 * Test 12: messages outside MPPT source ID range are ignored
 * ------------------------------------------------------------------------- */

void test_mppt_out_of_range_source_id(void)
{
    uint8_t payload[5];
    encode_cbor_small_uint(payload, THINGSET_ID_STATE, 3);

    /* ID below THINGSET_MPPT_ID_MIN */
    twai_message_t msg = make_pubsub_msg(0x0001, payload, 5);
    g_mgr->processReceivedMessage(&msg);
    TEST_ASSERT_EQUAL_UINT8(0, g_mgr->getDeviceCount());

    /* ID above THINGSET_MPPT_ID_MAX */
    msg = make_pubsub_msg(0x0020, payload, 5);
    g_mgr->processReceivedMessage(&msg);
    TEST_ASSERT_EQUAL_UINT8(0, g_mgr->getDeviceCount());
}

/* ---------------------------------------------------------------------------
 * Test 13: messages on wrong CAN base (not PUBSUB) are ignored
 * ------------------------------------------------------------------------- */

void test_mppt_wrong_can_base(void)
{
    uint8_t payload[5];
    encode_cbor_small_uint(payload, THINGSET_ID_STATE, 3);

    twai_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.identifier       = THINGSET_REQRESP_BASE | (uint32_t)THINGSET_MPPT_ID_MIN;
    msg.data_length_code = 5;
    memcpy(msg.data, payload, 5);
    msg.extd = true;

    g_mgr->processReceivedMessage(&msg);
    TEST_ASSERT_EQUAL_UINT8(0, g_mgr->getDeviceCount());
}

/* ---------------------------------------------------------------------------
 * Test 14: init with NULL pointers must not crash
 * ------------------------------------------------------------------------- */

void test_mppt_init_null_pointers(void)
{
    MPPTManager mgr;
    mgr.init(nullptr, nullptr);
    TEST_ASSERT_EQUAL_UINT8(0, mgr.getDeviceCount());

    mgr.update();
    TEST_ASSERT_EQUAL_UINT8(0, mgr.getDeviceCount());
}

/* ---------------------------------------------------------------------------
 * Test 15: DLC > 8 is rejected (invalid CAN message)
 * ------------------------------------------------------------------------- */

void test_mppt_invalid_dlc(void)
{
    twai_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.identifier       = THINGSET_PUBSUB_BASE | (uint32_t)THINGSET_MPPT_ID_MIN;
    msg.data_length_code = 16; /* invalid: exceeds maximum DLC of 15 */
    msg.extd             = true;

    g_mgr->processReceivedMessage(&msg);
    TEST_ASSERT_EQUAL_UINT8(0, g_mgr->getDeviceCount());
}


/* ---------------------------------------------------------------------------
 * Test 16: state machine – newly registered device starts in DISCOVERING,
 *          transitions to ONLINE after first message
 * ------------------------------------------------------------------------- */

void test_mppt_state_machine_initial(void)
{
    uint8_t payload[5];
    encode_cbor_small_uint(payload, THINGSET_ID_STATE, 3);
    twai_message_t msg = make_pubsub_msg(THINGSET_MPPT_ID_MIN, payload, 5);
    g_mgr->processReceivedMessage(&msg);

    const MPPTDevice *dev = g_mgr->getDevice(0);
    TEST_ASSERT_NOT_NULL(dev);
    /* After first message the state machine transitions from DISCOVERING to ONLINE */
    TEST_ASSERT_EQUAL_INT((int)DeviceState::ONLINE, (int)dev->state);
    TEST_ASSERT_EQUAL_UINT32(1, dev->total_messages_received);
}

/* ---------------------------------------------------------------------------
 * Test 17: state machine – device transitions to TIMEOUT_WARNING
 * ------------------------------------------------------------------------- */

void test_mppt_state_machine_timeout_warning(void)
{
    MockHAL::instance().setTime(0);

    uint8_t payload[5];
    encode_cbor_small_uint(payload, THINGSET_ID_STATE, 3);
    twai_message_t msg = make_pubsub_msg(THINGSET_MPPT_ID_MIN, payload, 5);
    g_mgr->processReceivedMessage(&msg);

    const MPPTDevice *dev = g_mgr->getDevice(0);
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_INT((int)DeviceState::ONLINE, (int)dev->state);

    /* Advance past the warning threshold (5 seconds) but before offline (10 seconds) */
    MockHAL::instance().setTime(6LL * 1000000LL);
    g_mgr->update();

    TEST_ASSERT_EQUAL_INT((int)DeviceState::TIMEOUT_WARNING, (int)dev->state);
}

/* ---------------------------------------------------------------------------
 * Test 18: state machine – device transitions to OFFLINE after full timeout
 * ------------------------------------------------------------------------- */

void test_mppt_state_machine_offline(void)
{
    MockHAL::instance().setTime(0);

    uint8_t payload[5];
    encode_cbor_small_uint(payload, THINGSET_ID_STATE, 3);
    twai_message_t msg = make_pubsub_msg(THINGSET_MPPT_ID_MIN, payload, 5);
    g_mgr->processReceivedMessage(&msg);

    const MPPTDevice *dev = g_mgr->getDevice(0);
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_INT((int)DeviceState::ONLINE, (int)dev->state);

    /* Advance past offline threshold (10 seconds) */
    MockHAL::instance().setTime(11LL * 1000000LL);
    g_mgr->update();

    TEST_ASSERT_EQUAL_INT((int)DeviceState::OFFLINE, (int)dev->state);
}

/* ---------------------------------------------------------------------------
 * Test 19: error stats – send failures are tracked
 * ------------------------------------------------------------------------- */

void test_mppt_error_stats_send_failure(void)
{
    MockCANBus::instance().should_fail_transmit = true;

    /* Advance time past discovery interval to trigger sendDiscovery */
    MockHAL::instance().setTime(31LL * 1000000LL);
    g_mgr->update();

    const MPPTManager::ErrorStats *stats = g_mgr->getErrorStats();
    TEST_ASSERT_NOT_NULL(stats);
    TEST_ASSERT_TRUE(stats->total_send_failures >= 1);
}

/* ---------------------------------------------------------------------------
 * Test 20: error stats – receive errors tracked on invalid message
 * ------------------------------------------------------------------------- */

void test_mppt_error_stats_receive_error(void)
{
    g_mgr->resetErrorStats();

    /* Send a NULL message to trigger receive error count */
    g_mgr->processReceivedMessage(nullptr);

    const MPPTManager::ErrorStats *stats = g_mgr->getErrorStats();
    TEST_ASSERT_NOT_NULL(stats);
    TEST_ASSERT_EQUAL_UINT32(1, stats->total_receive_errors);
}

/* ---------------------------------------------------------------------------
 * Test 21: resetErrorStats clears all error counters
 * ------------------------------------------------------------------------- */

void test_mppt_reset_error_stats(void)
{
    MockCANBus::instance().should_fail_transmit = true;
    MockHAL::instance().setTime(31LL * 1000000LL);
    g_mgr->update();

    g_mgr->resetErrorStats();

    const MPPTManager::ErrorStats *stats = g_mgr->getErrorStats();
    TEST_ASSERT_NOT_NULL(stats);
    TEST_ASSERT_EQUAL_UINT32(0, stats->total_send_failures);
    TEST_ASSERT_EQUAL_UINT32(0, stats->total_receive_errors);
    TEST_ASSERT_EQUAL_UINT32(0, stats->total_timeouts);
}

/* ---------------------------------------------------------------------------
 * Test 22: getDeviceStateName returns correct state string
 * ------------------------------------------------------------------------- */

void test_mppt_state_name(void)
{
    uint8_t payload[5];
    encode_cbor_small_uint(payload, THINGSET_ID_STATE, 3);
    twai_message_t msg = make_pubsub_msg(THINGSET_MPPT_ID_MIN, payload, 5);
    g_mgr->processReceivedMessage(&msg);

    const char *name = g_mgr->getDeviceStateName(THINGSET_MPPT_ID_MIN);
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_EQUAL_STRING("ONLINE", name);
}

/* ---------------------------------------------------------------------------
 * Test 23: deviceStateToString covers all states
 * ------------------------------------------------------------------------- */

void test_mppt_device_state_to_string(void)
{
    TEST_ASSERT_EQUAL_STRING("UNKNOWN",         deviceStateToString(DeviceState::UNKNOWN));
    TEST_ASSERT_EQUAL_STRING("DISCOVERING",     deviceStateToString(DeviceState::DISCOVERING));
    TEST_ASSERT_EQUAL_STRING("ONLINE",          deviceStateToString(DeviceState::ONLINE));
    TEST_ASSERT_EQUAL_STRING("DEGRADED",        deviceStateToString(DeviceState::DEGRADED));
    TEST_ASSERT_EQUAL_STRING("TIMEOUT_WARNING", deviceStateToString(DeviceState::TIMEOUT_WARNING));
    TEST_ASSERT_EQUAL_STRING("OFFLINE",         deviceStateToString(DeviceState::OFFLINE));
    TEST_ASSERT_EQUAL_STRING("ERROR",           deviceStateToString(DeviceState::ERROR));
}

/* ---------------------------------------------------------------------------
 * Test 24: logError stores error context and message in ErrorStats
 *
 * Note: CBOR float32 telemetry requires 9 bytes which exceeds the standard
 * CAN 8-byte frame limit. Validation errors for float values are exercised
 * here via the public logError API instead.
 * ------------------------------------------------------------------------- */

void test_mppt_validation_error_out_of_range(void)
{
    g_mgr->resetErrorStats();

    /* Directly invoke logError to simulate an out-of-range condition */
    g_mgr->logError("decodeTelemetry", "Solar voltage out of range: 300.00 V");

    const MPPTManager::ErrorStats *stats = g_mgr->getErrorStats();
    TEST_ASSERT_NOT_NULL(stats);

    /* last_error_message must be non-empty */
    TEST_ASSERT_TRUE(stats->last_error_message[0] != '\0');

    /* The message should contain the context and the detail */
    TEST_ASSERT_NOT_NULL(strstr(stats->last_error_message, "decodeTelemetry"));
    TEST_ASSERT_NOT_NULL(strstr(stats->last_error_message, "Solar voltage"));
}

/* ---------------------------------------------------------------------------
 * Test 25: recovers to ONLINE state after new message in TIMEOUT_WARNING
 * ------------------------------------------------------------------------- */

void test_mppt_state_machine_recovery(void){
    MockHAL::instance().setTime(0);

    uint8_t payload[5];
    encode_cbor_small_uint(payload, THINGSET_ID_STATE, 3);
    twai_message_t msg = make_pubsub_msg(THINGSET_MPPT_ID_MIN, payload, 5);
    g_mgr->processReceivedMessage(&msg);

    const MPPTDevice *dev = g_mgr->getDevice(0);
    TEST_ASSERT_NOT_NULL(dev);

    /* Advance to WARNING state */
    MockHAL::instance().setTime(6LL * 1000000LL);
    g_mgr->update();
    TEST_ASSERT_EQUAL_INT((int)DeviceState::TIMEOUT_WARNING, (int)dev->state);

    /* Receive new message to update last_seen_us to t=6s */
    g_mgr->processReceivedMessage(&msg);

    /* Move time forward slightly (1s after last_seen) – within warning window */
    MockHAL::instance().setTime(7LL * 1000000LL);
    g_mgr->update();

    /* Should be ONLINE now (time_since_seen = 1s < 5s WARNING_TIMEOUT) */
    TEST_ASSERT_EQUAL_INT((int)DeviceState::ONLINE, (int)dev->state);
}

/* ===========================================================================
 * Float32 telemetry decode tests
 * ========================================================================= */

/* ---------------------------------------------------------------------------
 * Test 26: float32 solar voltage – valid value is stored on the device
 * ------------------------------------------------------------------------- */

void test_mppt_float32_solar_voltage(void)
{
    const float expected = 42.5f;
    twai_message_t msg = make_float32_msg(THINGSET_MPPT_ID_MIN, THINGSET_ID_V_SOLAR, expected);
    g_mgr->processReceivedMessage(&msg);

    const MPPTDevice *dev = g_mgr->getDevice(0);
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_FLOAT(expected, dev->solar_voltage);
    TEST_ASSERT_EQUAL_UINT32(0, g_mgr->getErrorStats()->total_validation_errors);
}

/* ---------------------------------------------------------------------------
 * Test 27: float32 solar voltage – out-of-range increments validation counter
 * ------------------------------------------------------------------------- */

void test_mppt_float32_solar_voltage_out_of_range(void)
{
    g_mgr->resetErrorStats();

    /* 300 V exceeds MAX_SOLAR_VOLTAGE (200 V) */
    twai_message_t msg = make_float32_msg(THINGSET_MPPT_ID_MIN, THINGSET_ID_V_SOLAR, 300.0f);
    g_mgr->processReceivedMessage(&msg);

    TEST_ASSERT_EQUAL_UINT32(1, g_mgr->getErrorStats()->total_validation_errors);

    const MPPTDevice *dev = g_mgr->getDevice(0);
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, dev->solar_voltage); /* zeroed on out-of-range */
}

/* ---------------------------------------------------------------------------
 * Test 28: float32 solar current – valid value is stored
 * ------------------------------------------------------------------------- */

void test_mppt_float32_solar_current(void)
{
    const float expected = 15.0f;
    twai_message_t msg = make_float32_msg(THINGSET_MPPT_ID_MIN, THINGSET_ID_I_SOLAR, expected);
    g_mgr->processReceivedMessage(&msg);

    const MPPTDevice *dev = g_mgr->getDevice(0);
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_FLOAT(expected, dev->solar_current);
    TEST_ASSERT_EQUAL_UINT32(0, g_mgr->getErrorStats()->total_validation_errors);
}

/* ---------------------------------------------------------------------------
 * Test 29: float32 solar current – out-of-range increments validation counter
 * ------------------------------------------------------------------------- */

void test_mppt_float32_solar_current_out_of_range(void)
{
    g_mgr->resetErrorStats();

    /* 200 A exceeds MAX_CURRENT (100 A) */
    twai_message_t msg = make_float32_msg(THINGSET_MPPT_ID_MIN, THINGSET_ID_I_SOLAR, 200.0f);
    g_mgr->processReceivedMessage(&msg);

    TEST_ASSERT_EQUAL_UINT32(1, g_mgr->getErrorStats()->total_validation_errors);

    const MPPTDevice *dev = g_mgr->getDevice(0);
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, dev->solar_current);
}

/* ---------------------------------------------------------------------------
 * Test 30: float32 solar power – valid value is stored
 * ------------------------------------------------------------------------- */

void test_mppt_float32_solar_power(void)
{
    const float expected = 1500.0f;
    twai_message_t msg = make_float32_msg(THINGSET_MPPT_ID_MIN, THINGSET_ID_P_SOLAR, expected);
    g_mgr->processReceivedMessage(&msg);

    const MPPTDevice *dev = g_mgr->getDevice(0);
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_FLOAT(expected, dev->solar_power);
    TEST_ASSERT_EQUAL_UINT32(0, g_mgr->getErrorStats()->total_validation_errors);
}

/* ---------------------------------------------------------------------------
 * Test 31: float32 solar power – out-of-range increments validation counter
 * ------------------------------------------------------------------------- */

void test_mppt_float32_solar_power_out_of_range(void)
{
    g_mgr->resetErrorStats();

    /* 20000 W exceeds MAX_POWER (10000 W) */
    twai_message_t msg = make_float32_msg(THINGSET_MPPT_ID_MIN, THINGSET_ID_P_SOLAR, 20000.0f);
    g_mgr->processReceivedMessage(&msg);

    TEST_ASSERT_EQUAL_UINT32(1, g_mgr->getErrorStats()->total_validation_errors);

    const MPPTDevice *dev = g_mgr->getDevice(0);
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, dev->solar_power);
}

/* ---------------------------------------------------------------------------
 * Test 32: float32 battery voltage – valid value is stored
 * ------------------------------------------------------------------------- */

void test_mppt_float32_battery_voltage(void)
{
    const float expected = 52.0f;
    twai_message_t msg = make_float32_msg(THINGSET_MPPT_ID_MIN, THINGSET_ID_V_BAT, expected);
    g_mgr->processReceivedMessage(&msg);

    const MPPTDevice *dev = g_mgr->getDevice(0);
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_FLOAT(expected, dev->battery_voltage);
    TEST_ASSERT_EQUAL_UINT32(0, g_mgr->getErrorStats()->total_validation_errors);
}

/* ---------------------------------------------------------------------------
 * Test 33: float32 battery voltage – out-of-range increments validation counter
 * ------------------------------------------------------------------------- */

void test_mppt_float32_battery_voltage_out_of_range(void)
{
    g_mgr->resetErrorStats();

    /* 300 V exceeds MAX_BATTERY_VOLTAGE (200 V) */
    twai_message_t msg = make_float32_msg(THINGSET_MPPT_ID_MIN, THINGSET_ID_V_BAT, 300.0f);
    g_mgr->processReceivedMessage(&msg);

    TEST_ASSERT_EQUAL_UINT32(1, g_mgr->getErrorStats()->total_validation_errors);

    const MPPTDevice *dev = g_mgr->getDevice(0);
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, dev->battery_voltage);
}

/* ---------------------------------------------------------------------------
 * Test 34: float32 battery current – valid value is stored
 * ------------------------------------------------------------------------- */

void test_mppt_float32_battery_current(void)
{
    const float expected = 20.0f;
    twai_message_t msg = make_float32_msg(THINGSET_MPPT_ID_MIN, THINGSET_ID_I_BAT, expected);
    g_mgr->processReceivedMessage(&msg);

    const MPPTDevice *dev = g_mgr->getDevice(0);
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_FLOAT(expected, dev->battery_current);
    TEST_ASSERT_EQUAL_UINT32(0, g_mgr->getErrorStats()->total_validation_errors);
}

/* ---------------------------------------------------------------------------
 * Test 35: float32 battery current – out-of-range increments validation counter
 * ------------------------------------------------------------------------- */

void test_mppt_float32_battery_current_out_of_range(void)
{
    g_mgr->resetErrorStats();

    /* -200 A is below MIN_CURRENT (-100 A) */
    twai_message_t msg = make_float32_msg(THINGSET_MPPT_ID_MIN, THINGSET_ID_I_BAT, -200.0f);
    g_mgr->processReceivedMessage(&msg);

    TEST_ASSERT_EQUAL_UINT32(1, g_mgr->getErrorStats()->total_validation_errors);

    const MPPTDevice *dev = g_mgr->getDevice(0);
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, dev->battery_current);
}

/* ===========================================================================
 * sendWithRetry and calculateBackoffDelay tests
 * ========================================================================= */

/* Minimal 4-byte discovery-like payload for sendWithRetry calls */
static const uint8_t g_dummy_data[4] = {0xA1, 0x19, 0x1D, 0x00};
static const uint32_t g_dummy_can_id = THINGSET_REQRESP_BASE | (uint32_t)THINGSET_MPPT_ID_MIN;

/* ---------------------------------------------------------------------------
 * Test 36: sendWithRetry succeeds on first attempt – returns true and
 *          records the transmitted frame without incrementing send failures
 * ------------------------------------------------------------------------- */

void test_mppt_send_with_retry_success(void)
{
    MockCANBus::instance().reset();
    g_mgr->resetErrorStats();

    bool ok = g_mgr->sendWithRetry(g_dummy_can_id, g_dummy_data, sizeof(g_dummy_data), 0);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(1, MockCANBus::instance().getSentCount());
    TEST_ASSERT_EQUAL_UINT32(0, g_mgr->getErrorStats()->total_send_failures);
}

/* ---------------------------------------------------------------------------
 * Test 37: sendWithRetry failure – returns false and increments send failures
 * ------------------------------------------------------------------------- */

void test_mppt_send_with_retry_fail_enters_backoff(void)
{
    MockCANBus::instance().reset();
    g_mgr->resetErrorStats();
    MockHAL::instance().setTime(0);

    MockCANBus::instance().should_fail_transmit = true;

    bool ok = g_mgr->sendWithRetry(g_dummy_can_id, g_dummy_data, sizeof(g_dummy_data), 0);

    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_INT(0, MockCANBus::instance().getSentCount());
    TEST_ASSERT_EQUAL_UINT32(1, g_mgr->getErrorStats()->total_send_failures);
}

/* ---------------------------------------------------------------------------
 * Test 38: calling sendWithRetry during backoff does NOT increment failures
 *          (the retry is deferred until the backoff timer expires)
 * ------------------------------------------------------------------------- */

void test_mppt_send_with_retry_backoff_respected(void)
{
    MockCANBus::instance().reset();
    g_mgr->resetErrorStats();
    MockHAL::instance().setTime(0);

    MockCANBus::instance().should_fail_transmit = true;

    /* First call – fails and enters backoff */
    g_mgr->sendWithRetry(g_dummy_can_id, g_dummy_data, sizeof(g_dummy_data), 0);
    TEST_ASSERT_EQUAL_UINT32(1, g_mgr->getErrorStats()->total_send_failures);

    /* Second call immediately (still in backoff) – must not count as another failure */
    bool ok = g_mgr->sendWithRetry(g_dummy_can_id, g_dummy_data, sizeof(g_dummy_data), 0);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_UINT32(1, g_mgr->getErrorStats()->total_send_failures);
}

/* ---------------------------------------------------------------------------
 * Test 39: after the backoff window expires sendWithRetry attempts again
 * ------------------------------------------------------------------------- */

void test_mppt_send_with_retry_backoff_expires(void)
{
    MockCANBus::instance().reset();
    g_mgr->resetErrorStats();
    MockHAL::instance().setTime(0);

    MockCANBus::instance().should_fail_transmit = true;

    /* First failure at t=0: retry_count=1, backoff = initial(100)*2^1 = 200 ms */
    g_mgr->sendWithRetry(g_dummy_can_id, g_dummy_data, sizeof(g_dummy_data), 0);
    TEST_ASSERT_EQUAL_UINT32(1, g_mgr->getErrorStats()->total_send_failures);

    /* Advance 250 ms – past the 200 ms backoff */
    MockHAL::instance().setTime(250LL * 1000LL);

    /* Next call after backoff should attempt and fail again (failure count rises) */
    g_mgr->sendWithRetry(g_dummy_can_id, g_dummy_data, sizeof(g_dummy_data), 0);
    TEST_ASSERT_EQUAL_UINT32(2, g_mgr->getErrorStats()->total_send_failures);
}

/* ---------------------------------------------------------------------------
 * Test 40: out-of-range device_idx is rejected immediately
 * ------------------------------------------------------------------------- */

void test_mppt_send_with_retry_invalid_device_idx(void)
{
    MockCANBus::instance().reset();
    g_mgr->resetErrorStats();

    bool ok = g_mgr->sendWithRetry(g_dummy_can_id, g_dummy_data, sizeof(g_dummy_data),
                                   MAX_MPPT_DEVICES /* out of range */);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL_INT(0, MockCANBus::instance().getSentCount());
}

/* ---------------------------------------------------------------------------
 * Test 41: calculateBackoffDelay – exponential mode
 *          delay doubles with each retry count up to the configured maximum
 * ------------------------------------------------------------------------- */

void test_mppt_backoff_delay_exponential(void)
{
    /* Default config: initial=100 ms, max=5000 ms, exponential=true */
    uint32_t d0 = g_mgr->calculateBackoffDelay(0);
    uint32_t d1 = g_mgr->calculateBackoffDelay(1);
    uint32_t d2 = g_mgr->calculateBackoffDelay(2);

    TEST_ASSERT_EQUAL_UINT32(100,  d0); /* 100 * 2^0 = 100 */
    TEST_ASSERT_EQUAL_UINT32(200,  d1); /* 100 * 2^1 = 200 */
    TEST_ASSERT_EQUAL_UINT32(400,  d2); /* 100 * 2^2 = 400 */
}

/* ---------------------------------------------------------------------------
 * Test 42: calculateBackoffDelay – exponential mode is capped at max delay
 * ------------------------------------------------------------------------- */

void test_mppt_backoff_delay_capped(void)
{
    /* retry_count=8: 100 * 2^8 = 25600, exceeds max of 5000 → capped */
    uint32_t delay = g_mgr->calculateBackoffDelay(8);
    TEST_ASSERT_EQUAL_UINT32(MPPTConfig::MAX_RETRY_DELAY_MS, delay);
}

/* ---------------------------------------------------------------------------
 * Test 43: calculateBackoffDelay – linear (non-exponential) mode
 *          always returns initial_retry_delay_ms regardless of retry count
 * ------------------------------------------------------------------------- */

void test_mppt_backoff_delay_linear(void)
{
    MPPTManager::RecoveryConfig cfg;
    cfg.max_retries                = 3;
    cfg.initial_retry_delay_ms     = 150;
    cfg.max_retry_delay_ms         = 5000;
    cfg.enable_exponential_backoff = false;
    g_mgr->setRecoveryConfig(cfg);

    TEST_ASSERT_EQUAL_UINT32(150, g_mgr->calculateBackoffDelay(0));
    TEST_ASSERT_EQUAL_UINT32(150, g_mgr->calculateBackoffDelay(3));
    TEST_ASSERT_EQUAL_UINT32(150, g_mgr->calculateBackoffDelay(7));

    /* Restore default config so later tests are unaffected */
    MPPTManager::RecoveryConfig def;
    def.max_retries                = MPPTConfig::MAX_RETRIES;
    def.initial_retry_delay_ms     = MPPTConfig::INITIAL_RETRY_DELAY_MS;
    def.max_retry_delay_ms         = MPPTConfig::MAX_RETRY_DELAY_MS;
    def.enable_exponential_backoff = true;
    g_mgr->setRecoveryConfig(def);
}
