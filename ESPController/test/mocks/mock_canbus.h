#pragma once
/**
 * @file mock_canbus.h
 * @brief Mock CAN bus interface for unit testing
 *
 * Provides a controllable stub for send_ext_canbus_message() and
 * allows tests to inspect sent messages and inject received messages.
 *
 * Usage:
 *   MockCANBus& bus = MockCANBus::instance();
 *   bus.reset();
 *
 *   // After calling code-under-test:
 *   assert(bus.sent_count == 1);
 *   assert(bus.last_identifier == expected_id);
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define MOCK_CANBUS_MAX_MESSAGES 32

/** Record of a single CAN message transmitted by the code under test */
struct SentCANMessage
{
    uint32_t identifier;
    uint8_t  data[8];
    uint8_t  length;
};

class MockCANBus
{
public:
    static MockCANBus &instance();

    /** Reset all state to defaults (call in setUp) */
    void reset();

    /** Returns the number of messages sent since reset */
    int getSentCount() const { return sent_count; }

    /** Returns the last sent message (or nullptr if none sent) */
    const SentCANMessage *getLastSent() const;

    /** Returns sent message at given index (0 = first) */
    const SentCANMessage *getSentAt(int index) const;

    /* Controls */
    bool should_fail_transmit; /**< When true, transmit always fails */

    /* State (read-only from tests) */
    int sent_count;

    /** Called by the send_ext_canbus_message stub */
    bool transmit(uint32_t identifier, const uint8_t *data, uint8_t length);

private:
    MockCANBus();
    SentCANMessage _sent[MOCK_CANBUS_MAX_MESSAGES];
};
