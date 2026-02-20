#pragma once
/*
 * Mock driver/twai.h for unit testing on Linux host
 * Mimics ESP-IDF TWAI (CAN bus) driver types
 */

#include <stdint.h>
#include <stdbool.h>

#define TWAI_MSG_FLAG_NONE  0x00
#define TWAI_MSG_FLAG_EXTD  0x01  /* Extended frame (29-bit ID) */
#define TWAI_MSG_FLAG_RTR   0x02  /* Remote transmission request */
#define TWAI_MSG_FLAG_SS    0x04  /* Single shot */
#define TWAI_MSG_FLAG_SELF  0x08  /* Self reception */
#define TWAI_MSG_FLAG_DLC_NON_COMP 0x10

typedef struct {
    uint32_t identifier;         /* 11 or 29 bit identifier */
    uint8_t  data_length_code;   /* Data length code (0–8) */
    uint8_t  data[8];            /* Data bytes */
    uint8_t  flags;              /* Message flags (TWAI_MSG_FLAG_*) */
} twai_message_t;
