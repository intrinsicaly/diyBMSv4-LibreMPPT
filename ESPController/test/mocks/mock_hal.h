#pragma once
/**
 * @file mock_hal.h
 * @brief Mock Hardware Abstraction Layer for unit testing
 *
 * Provides controllable stubs for ESP32 hardware functions:
 *   - esp_timer_get_time()
 *   - millis()
 *   - FreeRTOS semaphore functions
 *   - Logging functions (via esp_log.h macros)
 *
 * Usage:
 *   MockHAL& hal = MockHAL::instance();
 *   hal.reset();
 *   hal.setTime(1000000);   // set clock to 1 second
 *   hal.advanceTime(500000); // advance by 500 ms
 */

#include <stdint.h>
#include <stdbool.h>

class MockHAL
{
public:
    static MockHAL &instance();

    /** Reset all state to defaults (call in setUp) */
    void reset();

    /** Set the simulated microsecond timer */
    void setTime(int64_t time_us);

    /** Advance the simulated microsecond timer */
    void advanceTime(int64_t delta_us);

    /** Get the current simulated microsecond timer */
    int64_t getTime() const;

    /** Get the current simulated millisecond timer */
    unsigned long getMillis() const;

    /* Semaphore behaviour controls */
    bool mutex_should_fail_take; /**< When true, xSemaphoreTake always returns pdFALSE */
    int  mutex_take_count;       /**< Number of successful xSemaphoreTake calls */
    int  mutex_give_count;       /**< Number of xSemaphoreGive calls */

private:
    MockHAL();
    int64_t _mock_time_us;
};
