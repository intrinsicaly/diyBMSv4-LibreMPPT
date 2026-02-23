#pragma once
/*
 * Mock driver/uart.h for unit testing on Linux host
 * Provides UART type definitions used in diybms_eeprom_settings
 */

#include <stdint.h>

typedef enum {
    UART_DATA_5_BITS = 0x0,
    UART_DATA_6_BITS = 0x1,
    UART_DATA_7_BITS = 0x2,
    UART_DATA_8_BITS = 0x3,
    UART_DATA_BITS_MAX = 0x4,
} uart_word_length_t;

typedef enum {
    UART_PARITY_DISABLE = 0x0,
    UART_PARITY_EVEN    = 0x2,
    UART_PARITY_ODD     = 0x3,
} uart_parity_t;

typedef enum {
    UART_STOP_BITS_1   = 0x1,
    UART_STOP_BITS_1_5 = 0x2,
    UART_STOP_BITS_2   = 0x3,
    UART_STOP_BITS_MAX = 0x4,
} uart_stop_bits_t;
