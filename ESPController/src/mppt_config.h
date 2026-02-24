#pragma once

/**
 * @file mppt_config.h
 * @brief MPPT CAN Bus Configuration Constants
 *
 * Named constants replacing magic numbers throughout the MPPT codebase.
 */

namespace MPPTConfig {
    // Timing
    constexpr uint32_t DISCOVERY_INTERVAL_MS             = 30000;
    constexpr uint32_t DEVICE_TIMEOUT_WARNING_MS         = 5000;
    constexpr uint32_t DEVICE_TIMEOUT_OFFLINE_MS         = 10000;
    constexpr uint32_t STATE_MACHINE_UPDATE_INTERVAL_MS  = 1000;
    constexpr uint32_t DIAGNOSTIC_DUMP_INTERVAL_MS       = 60000;

    // Limits
    constexpr uint8_t  MAX_DEVICES                       = 4;
    constexpr uint16_t MIN_NODE_ID                       = 0x0010;
    constexpr uint16_t MAX_NODE_ID                       = 0x001F;
    constexpr uint8_t  MAX_RETRIES                       = 3;
    constexpr uint32_t INITIAL_RETRY_DELAY_MS            = 100;
    constexpr uint32_t MAX_RETRY_DELAY_MS                = 5000;

    // Thresholds
    constexpr uint32_t DEGRADED_ERROR_THRESHOLD          = 3;
    constexpr uint32_t ERROR_STATE_THRESHOLD             = 10;
    constexpr float    MIN_SOLAR_VOLTAGE                 = 0.0f;
    constexpr float    MAX_SOLAR_VOLTAGE                 = 200.0f;
    constexpr float    MIN_BATTERY_VOLTAGE               = 0.0f;
    constexpr float    MAX_BATTERY_VOLTAGE               = 200.0f;
    constexpr float    MIN_CURRENT                       = -100.0f;
    constexpr float    MAX_CURRENT                       = 100.0f;
    constexpr float    MAX_POWER                         = 10000.0f;

    // CAN Bus
    constexpr uint32_t CAN_MUTEX_TIMEOUT_MS              = 100;
    constexpr uint32_t CAN_TRANSMIT_TIMEOUT_MS           = 100;
    constexpr uint8_t  MAX_CAN_DATA_LENGTH               = 8;

    // ThingSet Protocol base IDs (use CANBUS_ prefix to avoid macro name conflicts)
    constexpr uint32_t CANBUS_PUBSUB_BASE                = 0x1E000000UL;
    constexpr uint32_t CANBUS_REQRESP_BASE               = 0x1D000000UL;
    constexpr uint8_t  THINGSET_CAN_TYPE_PUBSUB          = 0x1E;
    constexpr uint8_t  THINGSET_CAN_TYPE_REQRESP         = 0x1D;

    // Data IDs
    constexpr uint16_t DATA_ID_SOLAR_VOLTAGE             = 0x6001;
    constexpr uint16_t DATA_ID_SOLAR_CURRENT             = 0x6002;
    constexpr uint16_t DATA_ID_SOLAR_POWER               = 0x6003;
    constexpr uint16_t DATA_ID_BATTERY_VOLTAGE           = 0x6004;
    constexpr uint16_t DATA_ID_BATTERY_CURRENT           = 0x6005;
    constexpr uint16_t DATA_ID_TEMPERATURE               = 0x6006;
    constexpr uint16_t DATA_ID_STATE                     = 0x6007;
    constexpr uint16_t DATA_ID_DAILY_ENERGY              = 0x6008;
    constexpr uint16_t DATA_ID_TARGET_VOLTAGE            = 0x4001;
    constexpr uint16_t DATA_ID_MAX_CURRENT               = 0x4002;
    constexpr uint16_t DATA_ID_ENABLE                    = 0x4000;
    constexpr uint16_t DATA_ID_NODE_ID                   = 0x1D00;
}
