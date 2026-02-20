#pragma once
/*
 * Mock freertos/semphr.h for unit testing on Linux host
 * Provides a simple single-threaded semaphore mock
 */

#include "FreeRTOS.h"

/* Internal semaphore structure */
struct MockSemaphore {
    bool locked;
    MockSemaphore() : locked(false) {}
};

typedef MockSemaphore* SemaphoreHandle_t;

/* Global flag to force semaphore take failure (for testing timeout paths) */
extern bool g_mock_semphr_fail;

inline SemaphoreHandle_t xSemaphoreCreateMutex() {
    return new MockSemaphore();
}

inline BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t ticks) {
    (void)ticks;
    if (g_mock_semphr_fail) return pdFALSE;
    if (sem && sem->locked) return pdFALSE;
    if (sem) sem->locked = true;
    return pdTRUE;
}

inline BaseType_t xSemaphoreGive(SemaphoreHandle_t sem) {
    if (sem) sem->locked = false;
    return pdTRUE;
}

inline void vSemaphoreDelete(SemaphoreHandle_t sem) {
    delete sem;
}
