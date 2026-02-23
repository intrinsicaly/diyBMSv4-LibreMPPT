#pragma once
/*
 * Mock Arduino.h for unit testing on Linux host
 * Provides platform-independent replacements for Arduino/ESP32 APIs
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <array>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>

/* Standard type aliases */
typedef uint8_t  byte;
typedef uint16_t word;

/* Binary literal macros (Arduino extension) */
#define B0        0
#define B1        1
#define B00       0
#define B01       1
#define B10       2
#define B11       3
#define B000      0
#define B001      1
#define B010      2
#define B011      3
#define B100      4
#define B101      5
#define B110      6
#define B111      7
#define B00000000 0x00
#define B00000001 0x01
#define B00000010 0x02
#define B00000011 0x03
#define B00000100 0x04
#define B00000101 0x05
#define B00000110 0x06
#define B00000111 0x07
#define B00001000 0x08
#define B00010000 0x10
#define B00100000 0x20
#define B00110000 0x30
#define B01000000 0x40
#define B10000000 0x80
#define B11000000 0xC0
#define B11111111 0xFF

/* min/max macros */
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

/* millis() mock */
extern uint32_t mock_millis_value;
inline uint32_t millis() { return mock_millis_value; }

/* PROGMEM / flash string stubs */
#define PROGMEM
#define F(str) (str)
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))

/* ESP-IDF logging macros (no-ops for clean test output) */
#define ESP_LOGE(tag, format, ...) ((void)0)
#define ESP_LOGW(tag, format, ...) ((void)0)
#define ESP_LOGI(tag, format, ...) ((void)0)
#define ESP_LOGD(tag, format, ...) ((void)0)
#define ESP_LOGV(tag, format, ...) ((void)0)

/* Include esp_timer and FreeRTOS task mocks */
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
