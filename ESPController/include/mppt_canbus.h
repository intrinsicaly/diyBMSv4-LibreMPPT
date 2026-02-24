#ifndef DIYBMS_MPPT_CANBUS_H_
#define DIYBMS_MPPT_CANBUS_H_

#include "defines.h"
#include "Rules.h"
#include <driver/twai.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdint.h>

// ThingSet CAN ID constants (29-bit extended IDs)
#define THINGSET_BMS_NODE_ID    0x0001
#define THINGSET_MPPT_ID_MIN    0x0010
#define THINGSET_MPPT_ID_MAX    0x001F
#define THINGSET_REQRESP_BASE   0x1D000000UL
#define THINGSET_PUBSUB_BASE    0x1E000000UL
#define THINGSET_BROADCAST_ID   0xFFFF

// ThingSet data object IDs for MPPT
#define THINGSET_ID_NODE_ID     0x1D00
#define THINGSET_ID_ENABLE      0x4000
#define THINGSET_ID_TARGET_V    0x4001
#define THINGSET_ID_MAX_CURR    0x4002
#define THINGSET_ID_ABS_V       0x4003
#define THINGSET_ID_FLOAT_V     0x4004
#define THINGSET_ID_V_SOLAR     0x6001
#define THINGSET_ID_I_SOLAR     0x6002
#define THINGSET_ID_P_SOLAR     0x6003
#define THINGSET_ID_V_BAT       0x6004
#define THINGSET_ID_I_BAT       0x6005
#define THINGSET_ID_TEMP        0x6006
#define THINGSET_ID_STATE       0x6007
#define THINGSET_ID_E_DAY       0x6008

#define MAX_MPPT_DEVICES 4

enum MPPTStatus : uint8_t
{
    MPPT_OFFLINE = 0,
    MPPT_ONLINE  = 1,
    MPPT_TIMEOUT = 2
};

// Device lifecycle states
enum class DeviceState : uint8_t {
    UNKNOWN          = 0,  // Initial state, never seen
    DISCOVERING      = 1,  // Discovery sent, awaiting response
    ONLINE           = 2,  // Actively receiving telemetry
    DEGRADED         = 3,  // Receiving telemetry but with errors
    TIMEOUT_WARNING  = 4,  // No telemetry for warning period
    OFFLINE          = 5,  // Timed out completely
    ERROR            = 6   // Persistent error state
};

const char* deviceStateToString(DeviceState state);

struct MPPTDevice
{
    uint16_t node_id;
    MPPTStatus status;       // Legacy status field (maintained for backward compatibility)
    int64_t last_seen_us;    // esp_timer_get_time() timestamp

    // Telemetry
    float solar_voltage;
    float solar_current;
    float solar_power;
    float battery_voltage;
    float battery_current;
    int16_t temperature;
    uint8_t charge_state;
    float daily_energy_wh;

    // Control state
    bool charging_enabled;

    // State machine
    DeviceState state;
    DeviceState previous_state;
    int64_t state_entered_time;
    uint32_t state_transition_count;

    // Health metrics
    uint32_t total_messages_received;
    uint32_t total_decode_errors;
    uint32_t consecutive_errors;
    float uptime_percentage;
};

class MPPTManager
{
public:
    // Error statistics
    struct ErrorStats {
        uint32_t total_send_failures;
        uint32_t total_receive_errors;
        uint32_t total_decode_errors;
        uint32_t total_timeouts;
        uint32_t total_validation_errors;
        uint32_t last_error_timestamp;
        char last_error_message[128];
    };

    // Runtime statistics
    struct Statistics {
        uint64_t total_messages_sent;
        uint64_t total_messages_received;
        uint64_t total_discoveries_sent;
        uint64_t total_control_commands_sent;
        uint32_t uptime_seconds;
        uint32_t last_statistics_dump;
    };

    // Recovery / retry configuration
    struct RecoveryConfig {
        uint8_t  max_retries;
        uint32_t initial_retry_delay_ms;
        uint32_t max_retry_delay_ms;
        bool     enable_exponential_backoff;
    };

    MPPTManager();
    void init(const diybms_eeprom_settings *settings, Rules *rules);
    void update();  // Call periodically (every 100ms)
    void processReceivedMessage(const twai_message_t *msg);
    bool sendControl(uint16_t mppt_id, bool enable_charge);
    bool sendVoltageLimit(uint16_t mppt_id, float voltage);
    bool sendCurrentLimit(uint16_t mppt_id, float current);

    uint8_t getDeviceCount() const;
    const MPPTDevice *getDevice(uint8_t index) const;

    // Error tracking
    ErrorStats* getErrorStats() { return &_error_stats; }
    void resetErrorStats();
    void logError(const char* context, const char* message);

    // Statistics
    const Statistics* getStatistics() const { return &_statistics; }
    void updateStatistics();
    void dumpStatistics();

    // Recovery configuration
    void setRecoveryConfig(const RecoveryConfig& config);

    // State machine
    void transitionDeviceState(MPPTDevice* device, DeviceState new_state, const char* reason);
    void updateDeviceStateMachine(MPPTDevice* device);
    const char* getDeviceStateName(uint16_t node_id);

    // Diagnostics
    void dumpDiagnostics();
    void dumpDeviceDetail(uint16_t node_id);
    void handleDegradedOperation();

    SemaphoreHandle_t mutex;

private:
    // Retry state per device
    struct RetryState {
        uint8_t retry_count;
        uint32_t next_retry_time;
        bool in_backoff;
    };

    MPPTDevice _devices[MAX_MPPT_DEVICES];
    uint8_t _device_count;
    const diybms_eeprom_settings *_settings;
    Rules *_rules;
    int64_t _last_discovery_us;

    ErrorStats _error_stats;
    Statistics _statistics;
    RecoveryConfig _recovery_config;
    RetryState _retry_state[MAX_MPPT_DEVICES];

    void sendDiscovery();
    void checkTimeouts();
    void registerDevice(uint16_t node_id);
    int findDevice(uint16_t node_id) const;
    void encodeCborFloat(uint8_t *buf, uint8_t &pos, uint16_t obj_id, float value);
    void encodeCborBool(uint8_t *buf, uint8_t &pos, uint16_t obj_id, bool value);
    void sendThingSetRequest(uint16_t target_id, const uint8_t *data, uint8_t len);
    bool sendWithRetry(uint32_t can_id, const uint8_t* data, uint8_t len, uint8_t device_idx);
    uint32_t calculateBackoffDelay(uint8_t retry_count);

#ifdef MPPT_MOCK_MODE
    void updateMockDevices();
    int64_t _mock_last_update_us;
#endif
};

extern MPPTManager mppt_manager;

extern diybms_eeprom_settings mysettings;
extern Rules rules;

#endif
