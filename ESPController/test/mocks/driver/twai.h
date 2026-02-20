#pragma once
/* Mock driver/twai.h for native unit test builds */
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t identifier;       /* 11 or 29 bit identifier */
    uint8_t data_length_code;  /* DLC (0-8) */
    uint8_t data[8];           /* CAN frame data */
    bool extd;                 /* extended frame format */
    bool rtr;                  /* remote transmission request */
    bool ss;                   /* single shot */
    bool self;                 /* self reception */
    bool dlc_non_comp;         /* DLC non-compliant */
} twai_message_t;
