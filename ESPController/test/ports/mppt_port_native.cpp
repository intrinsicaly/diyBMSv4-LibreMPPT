/**
 * @file mppt_port_native.cpp
 * @brief Native (Linux) test implementation of the MPPT port layer.
 *
 * Provides a fake clock controllable from tests and a CAN transmit
 * recorder. The implementation delegates to the MockHAL and MockCANBus
 * singletons so that existing test helpers (MockHAL::instance().setTime(),
 * MockCANBus::instance().getSentCount(), etc.) continue to work unchanged.
 *
 * Compiled only in the CMake native unit-test build.
 */

#include "mppt_port.h"
#include "mocks/mock_hal.h"
#include "mocks/mock_canbus.h"

extern "C" {

int64_t mppt_now_us(void)
{
    return MockHAL::instance().getTime();
}

bool mppt_can_send(uint32_t identifier, const uint8_t *data, uint8_t len)
{
    return MockCANBus::instance().transmit(identifier, data, len);
}

} /* extern "C" */
