#pragma once
/*
 * Mock Hardware Abstraction Layer for Testing
 */

#include <stdint.h>
#include <stdbool.h>
#include <vector>
#include <string>

class MockHAL {
public:
    MockHAL();
    ~MockHAL();

    /* CAN bus mutex simulation */
    bool GetCANMutex(uint32_t timeout_ms);
    void ReleaseCANMutex();
    bool     can_mutex_acquired;
    bool     can_mutex_should_fail;
    uint32_t can_mutex_timeout_ms;

    /* Log capture */
    void LogMessage(const char *level, const char *tag, const char *message);
    std::vector<std::string> log_messages;

    /* Reset all mock state */
    void Reset();
};

/* Global mock instance (set by MockHAL constructor/destructor) */
extern MockHAL *g_mock_hal;
