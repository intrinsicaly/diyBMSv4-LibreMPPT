/**
 * @file mock_hal.cpp
 * @brief Mock HAL implementation - provides controllable stubs for ESP32 HAL
 */

#include "mock_hal.h"
#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

/* ---------------------------------------------------------------------------
 * MockHAL singleton
 * ------------------------------------------------------------------------- */

MockHAL::MockHAL()
    : mutex_should_fail_take(false), mutex_take_count(0), mutex_give_count(0),
      _mock_time_us(0)
{
}

MockHAL &MockHAL::instance()
{
    static MockHAL hal;
    return hal;
}

void MockHAL::reset()
{
    _mock_time_us        = 0;
    mutex_should_fail_take = false;
    mutex_take_count     = 0;
    mutex_give_count     = 0;
}

void MockHAL::setTime(int64_t time_us)
{
    _mock_time_us = time_us;
}

void MockHAL::advanceTime(int64_t delta_us)
{
    _mock_time_us += delta_us;
}

int64_t MockHAL::getTime() const
{
    return _mock_time_us;
}

unsigned long MockHAL::getMillis() const
{
    return (unsigned long)(_mock_time_us / 1000LL);
}

/* ---------------------------------------------------------------------------
 * C-linkage stubs that the production code calls
 * ------------------------------------------------------------------------- */

extern "C" {

int64_t esp_timer_get_time(void)
{
    return MockHAL::instance().getTime();
}

} /* extern "C" */

/* millis() is used by PacketReceiveProcessor */
unsigned long millis()
{
    return MockHAL::instance().getMillis();
}

/* Arduino Serial stubs */
MockSerial Serial;
MockSerial Serial1;
MockSerial Serial2;

/* ---------------------------------------------------------------------------
 * FreeRTOS semaphore stubs
 * Each call goes through a simple mutex backed by a flag.
 * ------------------------------------------------------------------------- */

static bool _mutex_held = false;

SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    /* Return a non-NULL sentinel so the caller can detect failure */
    static int sentinel = 1;
    return (SemaphoreHandle_t)&sentinel;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait)
{
    (void)xTicksToWait;
    if (xSemaphore == NULL) return pdFALSE;

    MockHAL &hal = MockHAL::instance();
    if (hal.mutex_should_fail_take) return pdFALSE;

    hal.mutex_take_count++;
    _mutex_held = true;
    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore)
{
    if (xSemaphore == NULL) return pdFALSE;
    MockHAL::instance().mutex_give_count++;
    _mutex_held = false;
    return pdTRUE;
}

BaseType_t xTaskNotify(TaskHandle_t xTaskToNotify, uint32_t ulValue, eNotifyAction eAction)
{
    /* No-op in tests */
    (void)xTaskToNotify;
    (void)ulValue;
    (void)eAction;
    return pdTRUE;
}
