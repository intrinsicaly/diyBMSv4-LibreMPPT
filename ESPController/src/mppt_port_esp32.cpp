/**
 * @file mppt_port_esp32.cpp
 * @brief ESP32 production implementation of the MPPT port layer.
 *
 * Delegates to real platform APIs already available in the firmware.
 * Compiled only in PlatformIO ESP32 builds (not in the native CMake test build).
 */

#include "mppt_port.h"
#include <esp_timer.h>

/* send_ext_canbus_message is defined in main.cpp for the ESP32 firmware */
extern bool send_ext_canbus_message(uint32_t identifier, const uint8_t *buffer, const uint8_t length);

extern "C" {

int64_t mppt_now_us(void)
{
    return esp_timer_get_time();
}

bool mppt_can_send(uint32_t identifier, const uint8_t *data, uint8_t len)
{
    return send_ext_canbus_message(identifier, data, len);
}

} /* extern "C" */
