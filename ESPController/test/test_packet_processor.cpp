/**
 * @file test_packet_processor.cpp
 * @brief Unit tests for the PacketReceiveProcessor class
 *
 * Tests cover:
 *  - Valid packet processing
 *  - CRC validation
 *  - Buffer overflow protection (address range)
 *  - Address range validation
 *  - NULL pointer handling
 */

#include "unity.h"
#include "mocks/mock_hal.h"
#include "PacketReceiveProcessor.h"
#include "crc16.h"

/* ---------------------------------------------------------------------------
 * Globals required by PacketReceiveProcessor and defines.h
 * ------------------------------------------------------------------------- */

/** Global CellModuleInfo array referenced throughout the codebase */
CellModuleInfo cmi[maximum_controller_cell_modules];

/** Voltage snapshot task handle – NULL is fine for unit tests */
TaskHandle_t voltageandstatussnapshot_task_handle = nullptr;

/* ---------------------------------------------------------------------------
 * Helper utilities
 * ------------------------------------------------------------------------- */

/**
 * Build a PacketStruct with a valid CRC and 'processed by module' bit set.
 *
 * @param start   start_address field
 * @param end     end_address field
 * @param hops    hops field (total modules)
 * @param cmd     lower nibble command; top bit set => processed by module
 */
static PacketStruct make_packet(uint8_t start, uint8_t end,
                                uint8_t hops, uint8_t cmd)
{
    PacketStruct pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.start_address = start;
    pkt.end_address   = end;
    pkt.hops          = hops;
    pkt.command       = cmd;
    pkt.sequence      = 1;
    /* Calculate and embed a valid CRC */
    pkt.crc = CRC16::CalculateArray((uint8_t *)&pkt, sizeof(pkt) - 2);
    return pkt;
}

/* ---------------------------------------------------------------------------
 * Test 1: valid packet is accepted and returns true
 * ------------------------------------------------------------------------- */

void test_packet_validation(void)
{
    PacketReceiveProcessor proc;

    /* command = ReadVoltageAndStatus (1) | processed-by-module bit (0x80) */
    PacketStruct pkt = make_packet(0, 0, 1, (uint8_t)(0x80 | COMMAND::ReadVoltageAndStatus));

    bool result = proc.ProcessReply(&pkt);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT32(1, proc.packetsReceived);
    TEST_ASSERT_EQUAL_UINT16(0, proc.totalCRCErrors);
}

/* ---------------------------------------------------------------------------
 * Test 2: corrupted CRC is rejected
 * ------------------------------------------------------------------------- */

void test_packet_crc(void)
{
    PacketReceiveProcessor proc;

    PacketStruct pkt = make_packet(0, 0, 1, (uint8_t)(0x80 | COMMAND::ReadVoltageAndStatus));
    pkt.crc ^= 0xFFFF; /* corrupt the CRC */

    bool result = proc.ProcessReply(&pkt);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_UINT16(1, proc.totalCRCErrors);
}

/* ---------------------------------------------------------------------------
 * Test 3: start_address >= maximum_controller_cell_modules is rejected
 * ------------------------------------------------------------------------- */

void test_packet_buffer_overflow(void)
{
    PacketReceiveProcessor proc;

    PacketStruct pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.start_address = maximum_controller_cell_modules; /* out of range */
    pkt.end_address   = maximum_controller_cell_modules;
    pkt.hops          = 1;
    pkt.command       = (uint8_t)(0x80 | COMMAND::ReadVoltageAndStatus);
    pkt.sequence      = 1;
    pkt.crc = CRC16::CalculateArray((uint8_t *)&pkt, sizeof(pkt) - 2);

    bool result = proc.ProcessReply(&pkt);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_UINT16(1, proc.totalOutofSequenceErrors);
}

/* ---------------------------------------------------------------------------
 * Test 4: start_address > end_address is rejected
 * ------------------------------------------------------------------------- */

void test_packet_address_range(void)
{
    PacketReceiveProcessor proc;

    PacketStruct pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.start_address = 5;
    pkt.end_address   = 2; /* start > end */
    pkt.hops          = 6;
    pkt.command       = (uint8_t)(0x80 | COMMAND::ReadVoltageAndStatus);
    pkt.sequence      = 1;
    pkt.crc = CRC16::CalculateArray((uint8_t *)&pkt, sizeof(pkt) - 2);

    bool result = proc.ProcessReply(&pkt);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_UINT16(1, proc.totalOutofSequenceErrors);
}

/* ---------------------------------------------------------------------------
 * Test 5: NULL pointer is handled safely
 * ------------------------------------------------------------------------- */

void test_packet_null_pointer(void)
{
    PacketReceiveProcessor proc;

    bool result = proc.ProcessReply(nullptr);

    TEST_ASSERT_FALSE(result);
    /* packetsReceived increments even for NULL (matches implementation) */
    TEST_ASSERT_EQUAL_UINT32(1, proc.packetsReceived);
}
