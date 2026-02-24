#pragma once
/* Mock esp_timer.h for native unit test builds */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns microseconds since boot - provided by mock_hal.cpp */
int64_t esp_timer_get_time(void);

#ifdef __cplusplus
}
#endif
