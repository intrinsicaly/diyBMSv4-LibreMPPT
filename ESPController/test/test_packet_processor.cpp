/*
 * Unit tests for PacketReceiveProcessor (PacketReceiveProcessor.cpp)
 *
 * setUp / tearDown live in test_main.cpp.
 */

#include "unity.h"
#include "PacketReceiveProcessor.h"
#include "crc16.h"
#include <cstring>

/* Extern symbols defined in test_main.cpp */
extern uint32_t mock_millis_value;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/** Build a PacketStruct with a valid CRC and the ReplyWasProcessedByAModule
 *  bit set in command (0x80 OR command nibble). */
static PacketStruct make_packet(uint8_t start, uint8_t end, uint8_t cmd_nibble,
                                uint16_t sequence = 1, uint8_t hops = 1)
{
    PacketStruct pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.start_address = start;
    pkt.end_address   = end;
    /* Set bit 7 so ReplyWasProcessedByAModule() returns true */
    pkt.command       = (uint8_t)(0x80 | (cmd_nibble & 0x0F));
    pkt.hops          = hops;
    pkt.sequence      = sequence;
    pkt.crc           = CRC16::CalculateArray(
                            reinterpret_cast<uint8_t *>(&pkt),
                            sizeof(pkt) - sizeof(pkt.crc));
    return pkt;
}

/* ------------------------------------------------------------------ */
/* Tests                                                               */
/* ------------------------------------------------------------------ */

/**
 * Test: Valid Packet Processing
 * A correctly formed packet (good CRC, valid address range) is accepted.
 */
void test_packet_validation(void)
{
    PacketReceiveProcessor proc;

    PacketStruct pkt = make_packet(0, 0, COMMAND::ReadVoltageAndStatus);
    bool result = proc.ProcessReply(&pkt);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT32(1, proc.packetsReceived);
    TEST_ASSERT_EQUAL_UINT16(0, proc.totalCRCErrors);
}

/**
 * Test: Invalid CRC
 * A packet with a corrupt CRC must be rejected.
 */
void test_packet_crc(void)
{
    PacketReceiveProcessor proc;

    PacketStruct pkt = make_packet(0, 0, COMMAND::ReadVoltageAndStatus);
    pkt.crc ^= 0xFFFF;  /* corrupt the CRC */

    bool result = proc.ProcessReply(&pkt);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_UINT16(1, proc.totalCRCErrors);
}

/**
 * Test: Buffer Overflow Protection
 * Packets whose address fields exceed maximum_controller_cell_modules are
 * rejected before any array access.
 */
void test_packet_buffer_overflow(void)
{
    PacketReceiveProcessor proc;

    PacketStruct pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.start_address = maximum_controller_cell_modules;     /* == 200, out of range */
    pkt.end_address   = maximum_controller_cell_modules + 5;
    pkt.command       = (uint8_t)(0x80 | COMMAND::ReadVoltageAndStatus);
    pkt.crc           = CRC16::CalculateArray(
                            reinterpret_cast<uint8_t *>(&pkt),
                            sizeof(pkt) - sizeof(pkt.crc));

    bool result = proc.ProcessReply(&pkt);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_UINT16(1, proc.totalOutofSequenceErrors);
}

/**
 * Test: Invalid Address Range
 * start_address > end_address must be rejected.
 */
void test_packet_address_range(void)
{
    PacketReceiveProcessor proc;

    PacketStruct pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.start_address = 5;
    pkt.end_address   = 2;  /* invalid: start > end */
    pkt.command       = (uint8_t)(0x80 | COMMAND::ReadVoltageAndStatus);
    pkt.crc           = CRC16::CalculateArray(
                            reinterpret_cast<uint8_t *>(&pkt),
                            sizeof(pkt) - sizeof(pkt.crc));

    bool result = proc.ProcessReply(&pkt);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_UINT16(1, proc.totalOutofSequenceErrors);
}
