#pragma once
/**
 * @file mppt_port.h
 * @brief Minimal port/adapter seam for MPPT CAN bus code.
 *
 * Production code in mppt_canbus.cpp calls these functions instead of
 * platform headers directly. This allows native unit tests to substitute
 * a controllable fake implementation without broad header shadowing.
 *
 * Platform implementations:
 *   ESP32  – ESPController/src/mppt_port_esp32.cpp
 *   Native – ESPController/test/ports/mppt_port_native.cpp
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Return microseconds elapsed since boot (monotonic). */
int64_t mppt_now_us(void);

/**
 * Transmit an extended CAN frame.
 *
 * @param identifier  29-bit CAN identifier
 * @param data        payload bytes (up to 8)
 * @param len         payload length (0-8)
 * @return true on success, false if the frame could not be enqueued
 */
bool mppt_can_send(uint32_t identifier, const uint8_t *data, uint8_t len);

#ifdef __cplusplus
}
#endif
