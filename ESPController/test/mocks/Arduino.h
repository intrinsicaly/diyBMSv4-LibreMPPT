#pragma once
/* Mock Arduino.h for native unit test builds */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <array>
#include <string>
#include <algorithm>

/* Pull in ESP logging and timer mocks so all production headers compile */
#include "esp_log.h"
#include "esp_timer.h"

/* FreeRTOS types needed by headers that include <Arduino.h> */
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef unsigned char byte;
typedef bool boolean;
typedef unsigned long ulong;

/* Arduino binary literal macros (B prefix = binary) */
#define B00000000 0x00
#define B00000001 0x01
#define B00000010 0x02
#define B00000011 0x03
#define B00000100 0x04
#define B00000101 0x05
#define B00000110 0x06
#define B00000111 0x07
#define B10000000 0x80

/* Arduino min/max macros used by production code */
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

/* millis() provided by mock_hal.cpp */
unsigned long millis();

/* Minimal Serial stub */
struct MockSerial {
    void print(const char *) {}
    void println(const char *) {}
    void println(int) {}
};

extern MockSerial Serial;
extern MockSerial Serial1;
extern MockSerial Serial2;
