#pragma once
/*
 * Mock CAN Bus for unit testing
 */

#include <stdint.h>
#include <stdbool.h>
#include <vector>
#include <queue>
#include <cstring>
#include "driver/twai.h"

class MockCANBus {
public:
    MockCANBus();
    ~MockCANBus();

    /* Transmit/receive simulation */
    bool transmit(const twai_message_t *message, uint32_t timeout_ms);
    bool receive(twai_message_t *message, uint32_t timeout_ms);

    /* Inject a message into the receive queue (for testing incoming frames) */
    void InjectMessage(const twai_message_t &message);

    /* Test helpers */
    void ClearQueues();
    void Reset();

    bool should_fail_transmit;

    /* Message tracking */
    std::vector<twai_message_t> transmitted_messages;
    std::queue<twai_message_t>  receive_queue;
};

/* Global mock instance (set by MockCANBus constructor/destructor) */
extern MockCANBus *g_mock_canbus;

/* Mock implementations of CAN bus functions referenced by production code */
bool send_ext_canbus_message(uint32_t identifier, const uint8_t *buffer, const uint8_t length);
bool receive_canbus_message(twai_message_t *message, uint32_t timeout_ms);
