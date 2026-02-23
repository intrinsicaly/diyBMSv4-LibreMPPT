/*
 * Mock HAL implementation for unit testing
 */

#include "mock_hal.h"
#include <cstdio>
#include <cstring>

MockHAL *g_mock_hal = nullptr;

MockHAL::MockHAL()
    : can_mutex_acquired(false)
    , can_mutex_should_fail(false)
    , can_mutex_timeout_ms(0)
{
    g_mock_hal = this;
}

MockHAL::~MockHAL()
{
    g_mock_hal = nullptr;
}

bool MockHAL::GetCANMutex(uint32_t timeout_ms)
{
    can_mutex_timeout_ms = timeout_ms;
    if (can_mutex_should_fail) return false;
    can_mutex_acquired = true;
    return true;
}

void MockHAL::ReleaseCANMutex()
{
    can_mutex_acquired = false;
}

void MockHAL::LogMessage(const char *level, const char *tag, const char *message)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "[%s] %s: %s", level, tag, message);
    log_messages.push_back(buf);
}

void MockHAL::Reset()
{
    can_mutex_acquired    = false;
    can_mutex_should_fail = false;
    can_mutex_timeout_ms  = 0;
    log_messages.clear();
}
