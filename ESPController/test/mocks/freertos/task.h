#pragma once
/*
 * Mock freertos/task.h for unit testing on Linux host
 */

#include "FreeRTOS.h"
#include <stdint.h>

typedef void* TaskHandle_t;

/* Notify action enum (matches FreeRTOS API) */
enum eNotifyAction {
    eNoAction = 0,
    eSetBits,
    eIncrement,
    eSetValueWithOverwrite,
    eSetValueWithoutOverwrite
};

inline BaseType_t xTaskNotify(TaskHandle_t task, uint32_t value, eNotifyAction action) {
    (void)task;
    (void)value;
    (void)action;
    return pdTRUE;
}
