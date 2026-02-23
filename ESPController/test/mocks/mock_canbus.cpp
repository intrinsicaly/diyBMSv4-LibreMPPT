/*
 * Mock CAN bus implementation for unit testing
 */

#include "mock_canbus.h"
#include <cstring>

MockCANBus *g_mock_canbus = nullptr;

MockCANBus::MockCANBus()
    : should_fail_transmit(false)
{
    g_mock_canbus = this;
}

MockCANBus::~MockCANBus()
{
    g_mock_canbus = nullptr;
}

bool MockCANBus::transmit(const twai_message_t *message, uint32_t timeout_ms)
{
    (void)timeout_ms;
    if (should_fail_transmit) return false;
    transmitted_messages.push_back(*message);
    return true;
}

bool MockCANBus::receive(twai_message_t *message, uint32_t timeout_ms)
{
    (void)timeout_ms;
    if (receive_queue.empty()) return false;
    *message = receive_queue.front();
    receive_queue.pop();
    return true;
}

void MockCANBus::InjectMessage(const twai_message_t &message)
{
    receive_queue.push(message);
}

void MockCANBus::ClearQueues()
{
    while (!receive_queue.empty()) receive_queue.pop();
    transmitted_messages.clear();
}

void MockCANBus::Reset()
{
    ClearQueues();
    should_fail_transmit = false;
}

/* ---- Production-code hook implementations ---- */

bool send_ext_canbus_message(uint32_t identifier, const uint8_t *buffer, const uint8_t length)
{
    if (!g_mock_canbus) return false;

    twai_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.identifier       = identifier;
    msg.data_length_code = length;
    msg.flags            = TWAI_MSG_FLAG_EXTD;
    if (buffer && length > 0) {
        uint8_t clamped = (length <= 8) ? length : 8;
        memcpy(msg.data, buffer, clamped);
    }
    return g_mock_canbus->transmit(&msg, 100);
}

bool receive_canbus_message(twai_message_t *message, uint32_t timeout_ms)
{
    if (!g_mock_canbus) return false;
    return g_mock_canbus->receive(message, timeout_ms);
}
