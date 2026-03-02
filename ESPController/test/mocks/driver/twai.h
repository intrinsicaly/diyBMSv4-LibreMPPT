#pragma once
/* Mock driver/twai.h for native unit test builds */
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t identifier;       /* 11 or 29 bit identifier */
    uint8_t data_length_code;  /* DLC (0-15; values >8 used for CAN FD) */
    uint8_t data[16];          /* CAN/CAN-FD frame data (up to 16 bytes for DLC 0-15) */
    bool extd;                 /* extended frame format */
    bool rtr;                  /* remote transmission request */
    bool ss;                   /* single shot */
    bool self;                 /* self reception */
    bool dlc_non_comp;         /* DLC non-compliant */
} twai_message_t;
