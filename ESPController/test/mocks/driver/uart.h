#pragma once
/* Mock driver/uart.h for native unit test builds */
#include <stdint.h>

typedef enum {
    UART_DATA_5_BITS = 0,
    UART_DATA_6_BITS,
    UART_DATA_7_BITS,
    UART_DATA_8_BITS,
    UART_DATA_BITS_MAX
} uart_word_length_t;

typedef enum {
    UART_PARITY_DISABLE = 0,
    UART_PARITY_EVEN,
    UART_PARITY_ODD
} uart_parity_t;

typedef enum {
    UART_STOP_BITS_1 = 1,
    UART_STOP_BITS_1_5,
    UART_STOP_BITS_2,
    UART_STOP_BITS_MAX
} uart_stop_bits_t;
