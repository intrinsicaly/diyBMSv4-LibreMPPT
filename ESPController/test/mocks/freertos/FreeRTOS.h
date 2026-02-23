#pragma once
/*
 * Mock freertos/FreeRTOS.h for unit testing on Linux host
 */

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t TickType_t;
typedef uint32_t UBaseType_t;
typedef int32_t  BaseType_t;

#define pdTRUE   ((BaseType_t)1)
#define pdFALSE  ((BaseType_t)0)
#define pdPASS   pdTRUE
#define pdFAIL   pdFALSE

#define portMAX_DELAY ((TickType_t)0xFFFFFFFFUL)

/* Convert milliseconds to ticks (1:1 for tests) */
#define pdMS_TO_TICKS(xTimeInMs) ((TickType_t)(xTimeInMs))
