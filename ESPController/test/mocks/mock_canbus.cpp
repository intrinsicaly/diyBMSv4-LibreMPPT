/**
 * @file mock_canbus.cpp
 * @brief Mock CAN bus implementation
 *
 * Provides a controllable stub for send_ext_canbus_message() used by the
 * MPPT manager and other CAN-based modules.
 */

#include "mock_canbus.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * MockCANBus singleton
 * ------------------------------------------------------------------------- */

MockCANBus::MockCANBus()
    : should_fail_transmit(false), sent_count(0)
{
    memset(_sent, 0, sizeof(_sent));
}

MockCANBus &MockCANBus::instance()
{
    static MockCANBus bus;
    return bus;
}

void MockCANBus::reset()
{
    should_fail_transmit = false;
    sent_count           = 0;
    memset(_sent, 0, sizeof(_sent));
}

bool MockCANBus::transmit(uint32_t identifier, const uint8_t *data, uint8_t length)
{
    if (should_fail_transmit) return false;
    if (sent_count >= MOCK_CANBUS_MAX_MESSAGES) return false;

    SentCANMessage &m = _sent[sent_count++];
    m.identifier = identifier;
    m.length     = (length <= 8) ? length : 8;
    memset(m.data, 0, sizeof(m.data));
    if (data && m.length > 0) {
        memcpy(m.data, data, m.length);
    }
    return true;
}

const SentCANMessage *MockCANBus::getLastSent() const
{
    if (sent_count == 0) return nullptr;
    return &_sent[sent_count - 1];
}

const SentCANMessage *MockCANBus::getSentAt(int index) const
{
    if (index < 0 || index >= sent_count) return nullptr;
    return &_sent[index];
}

/* ---------------------------------------------------------------------------
 * C-linkage stub that production code calls
 * ------------------------------------------------------------------------- */

bool send_ext_canbus_message(uint32_t identifier, const uint8_t *buffer, const uint8_t length)
{
    return MockCANBus::instance().transmit(identifier, buffer, length);
}
