#ifndef DIYBMS_MPPT_CANBUS_H_
#define DIYBMS_MPPT_CANBUS_H_

#include "defines.h"
#include "Rules.h"
#include <driver/twai.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

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

struct MPPTDevice
{
    uint16_t node_id;
    MPPTStatus status;
    int64_t last_seen_us;  // esp_timer_get_time() timestamp

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
};

class MPPTManager
{
public:
    MPPTManager();
    void init(const diybms_eeprom_settings *settings, Rules *rules);
    void update();  // Call periodically (every 100ms)
    void processReceivedMessage(const twai_message_t *msg);
    bool sendControl(uint16_t mppt_id, bool enable_charge);
    bool sendVoltageLimit(uint16_t mppt_id, float voltage);
    bool sendCurrentLimit(uint16_t mppt_id, float current);

    uint8_t getDeviceCount() const;
    const MPPTDevice *getDevice(uint8_t index) const;

    SemaphoreHandle_t mutex;

private:
    MPPTDevice _devices[MAX_MPPT_DEVICES];
    uint8_t _device_count;
    const diybms_eeprom_settings *_settings;
    Rules *_rules;
    int64_t _last_discovery_us;

    void sendDiscovery();
    void checkTimeouts();
    void registerDevice(uint16_t node_id);
    int findDevice(uint16_t node_id) const;
    void encodeCborFloat(uint8_t *buf, uint8_t &pos, uint16_t obj_id, float value);
    void encodeCborBool(uint8_t *buf, uint8_t &pos, uint16_t obj_id, bool value);
    void sendThingSetRequest(uint16_t target_id, const uint8_t *data, uint8_t len);

#ifdef MPPT_MOCK_MODE
    void updateMockDevices();
    int64_t _mock_last_update_us;
#endif
};

extern MPPTManager mppt_manager;

extern bool send_ext_canbus_message(uint32_t identifier, const uint8_t *buffer, const uint8_t length);

extern diybms_eeprom_settings mysettings;
extern Rules rules;

#endif
