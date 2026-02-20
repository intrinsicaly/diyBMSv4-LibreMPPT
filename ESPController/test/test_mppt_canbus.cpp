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
#include "mocks/mock_hal.h"
#include "mocks/mock_canbus.h"
#include "mppt_canbus.h"

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

/* ---------------------------------------------------------------------------
 * setUp / tearDown
 * ------------------------------------------------------------------------- */

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
    extern CellModuleInfo cmi[maximum_controller_cell_modules];
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
    msg.data_length_code = 9; /* invalid: standard CAN max is 8 */
    msg.extd             = true;

    g_mgr->processReceivedMessage(&msg);
    TEST_ASSERT_EQUAL_UINT8(0, g_mgr->getDeviceCount());
}

