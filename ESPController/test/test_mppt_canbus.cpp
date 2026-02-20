/*
 * Unit tests for MPPTManager (mppt_canbus.cpp)
 *
 * setUp / tearDown live in test_main.cpp and create fresh MockHAL /
 * MockCANBus instances before every test.  Each test function creates its
 * own MPPTManager and local diybms_eeprom_settings so tests remain
 * independent.
 */

#include "unity.h"
#include "mock_hal.h"
#include "mock_canbus.h"
#include "mock_settings.h"
#include "mppt_canbus.h"
#include "Rules.h"
#include <cstring>

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/*
 * Build a valid TWAI pubsub message that MPPTManager::processReceivedMessage
 * will accept.
 *
 * Encodes a single small-integer CBOR value so we stay within the 8-byte
 * CAN frame limit:
 *
 *   0xA1               map(1)
 *   0x19 0xHH 0xLL     uint16 key = obj_id
 *   0xVV               small uint value (must be <= 0x17)
 */
static twai_message_t make_pubsub_msg(uint16_t source_id,
                                      uint16_t obj_id,
                                      uint8_t  small_value)
{
    twai_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.identifier       = THINGSET_PUBSUB_BASE | source_id;
    msg.flags            = TWAI_MSG_FLAG_EXTD;
    msg.data[0]          = 0xA1;
    msg.data[1]          = 0x19;
    msg.data[2]          = (uint8_t)(obj_id >> 8);
    msg.data[3]          = (uint8_t)(obj_id & 0xFF);
    msg.data[4]          = small_value;  /* CBOR small uint (0x00–0x17) */
    msg.data_length_code = 5;
    return msg;
}

/*
 * Build a pubsub message that encodes an int16 value via CBOR 0x19 tag.
 * Layout: 0xA1 0x19 <key_hi> <key_lo> 0x19 <val_hi> <val_lo>  (7 bytes)
 */
static twai_message_t make_pubsub_msg_int16(uint16_t source_id,
                                            uint16_t obj_id,
                                            int16_t  value)
{
    twai_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.identifier       = THINGSET_PUBSUB_BASE | source_id;
    msg.flags            = TWAI_MSG_FLAG_EXTD;
    msg.data[0]          = 0xA1;
    msg.data[1]          = 0x19;
    msg.data[2]          = (uint8_t)(obj_id >> 8);
    msg.data[3]          = (uint8_t)(obj_id & 0xFF);
    msg.data[4]          = 0x19;  /* CBOR uint16 / int16 tag */
    msg.data[5]          = (uint8_t)((uint16_t)value >> 8);
    msg.data[6]          = (uint8_t)((uint16_t)value & 0xFF);
    msg.data_length_code = 7;
    return msg;
}

/* Extern for the mock timer (defined in test_main.cpp) */
extern int64_t mock_esp_timer_value;

/* ------------------------------------------------------------------ */
/* Tests                                                               */
/* ------------------------------------------------------------------ */

/**
 * Test: Device Registration
 * A valid pubsub message from a known MPPT node ID triggers device registration.
 */
void test_mppt_device_registration(void)
{
    diybms_eeprom_settings settings = make_test_settings();
    Rules                  rules_obj;
    MPPTManager            mgr;
    mgr.init(&settings, &rules_obj);

    TEST_ASSERT_EQUAL_UINT8(0, mgr.getDeviceCount());

    twai_message_t msg = make_pubsub_msg(THINGSET_MPPT_ID_MIN, THINGSET_ID_STATE, 0x03);
    mgr.processReceivedMessage(&msg);

    TEST_ASSERT_EQUAL_UINT8(1, mgr.getDeviceCount());

    const MPPTDevice *dev = mgr.getDevice(0);
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_UINT16(THINGSET_MPPT_ID_MIN, dev->node_id);
    TEST_ASSERT_EQUAL_INT(MPPT_ONLINE, (int)dev->status);
}

/**
 * Test: Device Discovery Broadcast
 * When the discovery interval has elapsed, update() sends a CAN broadcast.
 */
void test_mppt_device_discovery(void)
{
    diybms_eeprom_settings settings = make_test_settings();
    Rules                  rules_obj;
    MPPTManager            mgr;
    mgr.init(&settings, &rules_obj);

    /* Advance mock timer past the 30-second discovery interval */
    mock_esp_timer_value = 31LL * 1000000LL;

    g_mock_canbus->ClearQueues();
    mgr.update();

    /* At least one discovery frame should have been transmitted */
    TEST_ASSERT_GREATER_THAN(0, (int)g_mock_canbus->transmitted_messages.size());

    const twai_message_t &tx = g_mock_canbus->transmitted_messages[0];
    TEST_ASSERT_EQUAL_UINT32(TWAI_MSG_FLAG_EXTD, tx.flags & TWAI_MSG_FLAG_EXTD);
    /* Discovery uses REQRESP base */
    TEST_ASSERT_EQUAL_UINT32(THINGSET_REQRESP_BASE,
                             tx.identifier & 0xFF000000UL);
}

/**
 * Test: Telemetry Decode
 * A pubsub message containing a CBOR int16 temperature value is decoded
 * and stored in the device struct.
 */
void test_mppt_telemetry_decode(void)
{
    diybms_eeprom_settings settings = make_test_settings();
    Rules                  rules_obj;
    MPPTManager            mgr;
    mgr.init(&settings, &rules_obj);

    const uint16_t node_id  = THINGSET_MPPT_ID_MIN;
    const int16_t  temp_val = 25;  /* 25 °C */

    /* First register the device */
    twai_message_t reg_msg = make_pubsub_msg(node_id, THINGSET_ID_STATE, 0x03);
    mgr.processReceivedMessage(&reg_msg);

    /* Now send a temperature telemetry frame */
    twai_message_t temp_msg = make_pubsub_msg_int16(node_id, THINGSET_ID_TEMP, temp_val);
    mgr.processReceivedMessage(&temp_msg);

    const MPPTDevice *dev = mgr.getDevice(0);
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_INT16(temp_val, dev->temperature);

    /* Also verify charge_state was set by the earlier frame */
    TEST_ASSERT_EQUAL_UINT8(0x03, dev->charge_state);
}

/**
 * Test: Control Message Send
 * sendControl() formats and transmits a ThingSet request over CAN.
 */
void test_mppt_control_send(void)
{
    diybms_eeprom_settings settings = make_test_settings();
    Rules                  rules_obj;
    MPPTManager            mgr;
    mgr.init(&settings, &rules_obj);

    g_mock_canbus->ClearQueues();

    bool result = mgr.sendControl(THINGSET_MPPT_ID_MIN, true);
    TEST_ASSERT_TRUE(result);

    TEST_ASSERT_EQUAL_INT(1, (int)g_mock_canbus->transmitted_messages.size());

    const twai_message_t &tx = g_mock_canbus->transmitted_messages[0];
    /* Must be an extended-frame request/response packet targeting the node */
    TEST_ASSERT_EQUAL_UINT32(THINGSET_REQRESP_BASE,
                             tx.identifier & 0xFF000000UL);
    TEST_ASSERT_EQUAL_UINT32(THINGSET_MPPT_ID_MIN,
                             tx.identifier & 0x0000FFFFUL);
    /* First byte should be 0xA1: CBOR map(1) */
    TEST_ASSERT_EQUAL_HEX8(0xA1, tx.data[0]);
}

/**
 * Test: Device Timeout Handling
 * Devices that have not been heard from within the configured timeout
 * period are transitioned to MPPT_TIMEOUT by update().
 */
void test_mppt_timeout_handling(void)
{
    diybms_eeprom_settings settings = make_test_settings();
    settings.mppt_timeout_seconds = 10;  /* Short timeout for the test */
    Rules       rules_obj;
    MPPTManager mgr;
    mgr.init(&settings, &rules_obj);

    /* Register a device at t=0 */
    mock_esp_timer_value = 0;
    twai_message_t msg = make_pubsub_msg(THINGSET_MPPT_ID_MIN, THINGSET_ID_STATE, 0x03);
    mgr.processReceivedMessage(&msg);

    const MPPTDevice *dev = mgr.getDevice(0);
    TEST_ASSERT_NOT_NULL(dev);
    TEST_ASSERT_EQUAL_INT(MPPT_ONLINE, (int)dev->status);

    /* Advance timer past the timeout threshold; skip discovery interval */
    mock_esp_timer_value = 11LL * 1000000LL;   /* 11 s > 10 s timeout */

    /* Force _last_discovery_us ahead so update() doesn't try to send CAN */
    /* (We skip discovery by advancing past the 30 s interval on the first
       update so it fires once and sets _last_discovery_us = now, then we
       call update() a second time.) */
    mgr.update();                       /* may trigger discovery */
    mock_esp_timer_value = 11LL * 1000000LL;
    mgr.update();                       /* checkTimeouts() runs here */

    TEST_ASSERT_EQUAL_INT(MPPT_TIMEOUT, (int)dev->status);
}

/**
 * Test: Maximum Device Limit
 * The manager accepts at most MAX_MPPT_DEVICES devices; additional ones
 * are silently ignored.
 */
void test_mppt_max_devices(void)
{
    diybms_eeprom_settings settings = make_test_settings();
    Rules       rules_obj;
    MPPTManager mgr;
    mgr.init(&settings, &rules_obj);

    /* Register the maximum number of devices */
    for (int i = 0; i < MAX_MPPT_DEVICES; i++) {
        uint16_t     node_id = (uint16_t)(THINGSET_MPPT_ID_MIN + i);
        twai_message_t msg   = make_pubsub_msg(node_id, THINGSET_ID_STATE, 0x03);
        mgr.processReceivedMessage(&msg);
    }

    TEST_ASSERT_EQUAL_UINT8(MAX_MPPT_DEVICES, mgr.getDeviceCount());

    /* Attempting to add one more node (outside the reserved ID range) is
       filtered before registration – count must stay the same. */
    /* The valid MPPT range is [THINGSET_MPPT_ID_MIN, THINGSET_MPPT_ID_MAX].
       If MAX_MPPT_DEVICES == (MAX - MIN + 1), all slots within the range
       are filled; an ID outside the range is rejected at source-ID check. */
    twai_message_t extra_msg;
    memset(&extra_msg, 0, sizeof(extra_msg));
    extra_msg.identifier       = THINGSET_PUBSUB_BASE | (THINGSET_MPPT_ID_MAX + 1);
    extra_msg.flags            = TWAI_MSG_FLAG_EXTD;
    extra_msg.data[0]          = 0xA1;
    extra_msg.data_length_code = 1;
    mgr.processReceivedMessage(&extra_msg);

    TEST_ASSERT_EQUAL_UINT8(MAX_MPPT_DEVICES, mgr.getDeviceCount());
}

/**
 * Test: Input Validation
 * Null and malformed messages are rejected without crashing or registering
 * spurious devices.
 */
void test_mppt_input_validation(void)
{
    diybms_eeprom_settings settings = make_test_settings();
    Rules       rules_obj;
    MPPTManager mgr;
    mgr.init(&settings, &rules_obj);

    /* NULL message pointer */
    mgr.processReceivedMessage(nullptr);
    TEST_ASSERT_EQUAL_UINT8(0, mgr.getDeviceCount());

    /* data_length_code > 8 (invalid) */
    twai_message_t bad_dlc;
    memset(&bad_dlc, 0, sizeof(bad_dlc));
    bad_dlc.identifier       = THINGSET_PUBSUB_BASE | THINGSET_MPPT_ID_MIN;
    bad_dlc.data_length_code = 9;
    mgr.processReceivedMessage(&bad_dlc);
    TEST_ASSERT_EQUAL_UINT8(0, mgr.getDeviceCount());

    /* Wrong CAN ID base (not THINGSET_PUBSUB_BASE) */
    twai_message_t wrong_base;
    memset(&wrong_base, 0, sizeof(wrong_base));
    wrong_base.identifier       = 0x1D000000UL | THINGSET_MPPT_ID_MIN;
    wrong_base.data_length_code = 5;
    mgr.processReceivedMessage(&wrong_base);
    TEST_ASSERT_EQUAL_UINT8(0, mgr.getDeviceCount());

    /* Source node ID below the valid MPPT range */
    twai_message_t below_range;
    memset(&below_range, 0, sizeof(below_range));
    below_range.identifier       = THINGSET_PUBSUB_BASE | (THINGSET_MPPT_ID_MIN - 1);
    below_range.data_length_code = 5;
    below_range.data[0]          = 0xA1;
    mgr.processReceivedMessage(&below_range);
    TEST_ASSERT_EQUAL_UINT8(0, mgr.getDeviceCount());

    /* Source node ID above the valid MPPT range */
    twai_message_t above_range;
    memset(&above_range, 0, sizeof(above_range));
    above_range.identifier       = THINGSET_PUBSUB_BASE | (THINGSET_MPPT_ID_MAX + 1);
    above_range.data_length_code = 5;
    above_range.data[0]          = 0xA1;
    mgr.processReceivedMessage(&above_range);
    TEST_ASSERT_EQUAL_UINT8(0, mgr.getDeviceCount());
}
