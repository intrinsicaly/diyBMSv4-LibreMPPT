#pragma once
/*
 * Mock esp_timer.h for unit testing on Linux host
 */

#include <stdint.h>

/* Controllable mock timer value (in microseconds) */
extern int64_t mock_esp_timer_value;

inline int64_t esp_timer_get_time(void) {
    return mock_esp_timer_value;
}
