#define USE_ESP_IDF_LOG 1
static constexpr const char *const TAG = "diybms-mppt";

#include "mppt_canbus.h"
#include <esp_timer.h>
#include <string.h>

MPPTManager mppt_manager;

#define DISCOVERY_INTERVAL_US  (30LL * 1000000LL)  // 30 seconds
#define TIMEOUT_US(sec)        ((int64_t)(sec) * 1000000LL)

MPPTManager::MPPTManager()
    : _device_count(0), _settings(nullptr), _rules(nullptr), _last_discovery_us(0)
{
    mutex = xSemaphoreCreateMutex();
    memset(_devices, 0, sizeof(_devices));
#ifdef MPPT_MOCK_MODE
    _mock_last_update_us = 0;
#endif
}

void MPPTManager::init(const diybms_eeprom_settings *settings, Rules *rules)
{
    _settings = settings;
    _rules = rules;
    ESP_LOGI(TAG, "MPPT Manager initialized, enabled=%d", settings->mppt_can_enabled);
}

void MPPTManager::update()
{
    if (!_settings || !_settings->mppt_can_enabled) return;

    int64_t now = esp_timer_get_time();

#ifdef MPPT_MOCK_MODE
    if (_settings->mppt_mock_mode_enabled)
    {
        updateMockDevices();
        return;
    }
#endif

    // Periodic discovery
    if ((now - _last_discovery_us) >= DISCOVERY_INTERVAL_US)
    {
        sendDiscovery();
        _last_discovery_us = now;
    }

    // Check for timeouts
    checkTimeouts();

    // Apply BMS protection rules - disable charging on over-voltage
    if (_rules && _rules->ruleOutcome(Rule::BankOverVoltage))
    {
        // Collect IDs of devices that need charging disabled (while holding mutex)
        uint16_t disable_ids[MAX_MPPT_DEVICES];
        uint8_t disable_count = 0;

        if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            for (uint8_t i = 0; i < _device_count; i++)
            {
                if (_devices[i].status == MPPT_ONLINE && _devices[i].charging_enabled)
                {
                    disable_ids[disable_count++] = _devices[i].node_id;
                }
            }
            xSemaphoreGive(mutex);
        }

        // Send disable commands without holding the mutex
        for (uint8_t i = 0; i < disable_count; i++)
        {
            sendControl(disable_ids[i], false);
        }
    }
}

void MPPTManager::processReceivedMessage(const twai_message_t *msg)
{
    if (!_settings || !_settings->mppt_can_enabled) return;

    uint32_t id = msg->identifier;

    uint32_t base = id & 0xFF000000UL;
    uint16_t source = (uint16_t)(id & 0x0000FFFFUL);

    if (base != THINGSET_PUBSUB_BASE) return;
    if (source < THINGSET_MPPT_ID_MIN || source > THINGSET_MPPT_ID_MAX) return;

    ESP_LOGD(TAG, "MPPT telemetry from 0x%04X, len=%d", source, msg->data_length_code);

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        int idx = findDevice(source);
        if (idx < 0)
        {
            xSemaphoreGive(mutex);
            registerDevice(source);
            if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) != pdTRUE) return;
            idx = findDevice(source);
        }

        if (idx >= 0)
        {
            _devices[idx].last_seen_us = esp_timer_get_time();
            _devices[idx].status = MPPT_ONLINE;

            // Parse CBOR-encoded telemetry: 0xA1 0x19 <ID_HI> <ID_LO> <value>
            const uint8_t *d = msg->data;
            uint8_t len = msg->data_length_code;

            if (len >= 5 && d[0] == 0xA1 && d[1] == 0x19)
            {
                uint16_t obj_id = ((uint16_t)d[2] << 8) | d[3];
                // float32 value (0xFA + 4 bytes)
                if (len >= 9 && d[4] == 0xFA)
                {
                    uint32_t raw;
                    memcpy(&raw, &d[5], 4);
                    raw = __builtin_bswap32(raw);
                    float val;
                    memcpy(&val, &raw, 4);

                    switch (obj_id)
                    {
                    case THINGSET_ID_V_SOLAR: _devices[idx].solar_voltage = val; break;
                    case THINGSET_ID_I_SOLAR: _devices[idx].solar_current = val; break;
                    case THINGSET_ID_P_SOLAR: _devices[idx].solar_power = val; break;
                    case THINGSET_ID_V_BAT:   _devices[idx].battery_voltage = val; break;
                    case THINGSET_ID_I_BAT:   _devices[idx].battery_current = val; break;
                    case THINGSET_ID_E_DAY:   _devices[idx].daily_energy_wh = val; break;
                    default: break;
                    }
                }
                // int16 temperature (0x19 + 2 bytes)
                else if (len >= 7 && d[4] == 0x19)
                {
                    int16_t val16 = (int16_t)(((uint16_t)d[5] << 8) | d[6]);
                    if (obj_id == THINGSET_ID_TEMP) _devices[idx].temperature = val16;
                }
                // small uint (0x00-0x17)
                else if (len >= 5 && d[4] <= 0x17)
                {
                    if (obj_id == THINGSET_ID_STATE) _devices[idx].charge_state = d[4];
                }
            }
        }
        xSemaphoreGive(mutex);
    }
}

bool MPPTManager::sendControl(uint16_t mppt_id, bool enable_charge)
{
    if (!_settings || !_settings->mppt_can_enabled) return false;

    uint8_t buf[6];
    uint8_t pos = 0;
    encodeCborBool(buf, pos, THINGSET_ID_ENABLE, enable_charge);
    sendThingSetRequest(mppt_id, buf, pos);

    // Update local state
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        int idx = findDevice(mppt_id);
        if (idx >= 0) _devices[idx].charging_enabled = enable_charge;
        xSemaphoreGive(mutex);
    }

    ESP_LOGI(TAG, "MPPT 0x%04X charge %s", mppt_id, enable_charge ? "enabled" : "disabled");
    return true;
}

bool MPPTManager::sendVoltageLimit(uint16_t mppt_id, float voltage)
{
    if (!_settings || !_settings->mppt_can_enabled) return false;

    uint8_t buf[9];
    uint8_t pos = 0;
    encodeCborFloat(buf, pos, THINGSET_ID_TARGET_V, voltage);
    sendThingSetRequest(mppt_id, buf, pos);
    return true;
}

bool MPPTManager::sendCurrentLimit(uint16_t mppt_id, float current)
{
    if (!_settings || !_settings->mppt_can_enabled) return false;

    uint8_t buf[9];
    uint8_t pos = 0;
    encodeCborFloat(buf, pos, THINGSET_ID_MAX_CURR, current);
    sendThingSetRequest(mppt_id, buf, pos);
    return true;
}

uint8_t MPPTManager::getDeviceCount() const
{
    return _device_count;
}

const MPPTDevice *MPPTManager::getDevice(uint8_t index) const
{
    if (index >= _device_count) return nullptr;
    return &_devices[index];
}

void MPPTManager::sendDiscovery()
{
    // Broadcast node ID query
    uint32_t can_id = THINGSET_REQRESP_BASE | (5UL << 20) | THINGSET_BROADCAST_ID;
    uint8_t buf[] = {0xA1, 0x19, 0x1D, 0x00};
    send_ext_canbus_message(can_id, buf, sizeof(buf));
    ESP_LOGD(TAG, "MPPT discovery broadcast sent");
}

void MPPTManager::checkTimeouts()
{
    if (!_settings) return;

    int64_t now = esp_timer_get_time();
    int64_t timeout_us = TIMEOUT_US(_settings->mppt_timeout_seconds);

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        for (uint8_t i = 0; i < _device_count; i++)
        {
            if (_devices[i].status == MPPT_ONLINE)
            {
                if ((now - _devices[i].last_seen_us) > timeout_us)
                {
                    ESP_LOGW(TAG, "MPPT 0x%04X timed out", _devices[i].node_id);
                    _devices[i].status = MPPT_TIMEOUT;
                }
            }
        }
        xSemaphoreGive(mutex);
    }
}

void MPPTManager::registerDevice(uint16_t node_id)
{
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) != pdTRUE) return;

    if (_device_count < MAX_MPPT_DEVICES)
    {
        // Check not already registered
        bool found = false;
        for (uint8_t i = 0; i < _device_count; i++)
        {
            if (_devices[i].node_id == node_id) { found = true; break; }
        }

        if (!found)
        {
            ESP_LOGI(TAG, "New MPPT discovered: 0x%04X", node_id);
            memset(&_devices[_device_count], 0, sizeof(MPPTDevice));
            _devices[_device_count].node_id = node_id;
            _devices[_device_count].status = MPPT_ONLINE;
            _devices[_device_count].charging_enabled = true;
            _devices[_device_count].last_seen_us = esp_timer_get_time();
            _device_count++;
        }
    }
    xSemaphoreGive(mutex);
}

int MPPTManager::findDevice(uint16_t node_id) const
{
    for (uint8_t i = 0; i < _device_count; i++)
    {
        if (_devices[i].node_id == node_id) return (int)i;
    }
    return -1;
}

void MPPTManager::encodeCborFloat(uint8_t *buf, uint8_t &pos, uint16_t obj_id, float value)
{
    buf[pos++] = 0xA1;  // map(1)
    buf[pos++] = 0x19;  // uint16 follows
    buf[pos++] = (obj_id >> 8) & 0xFF;
    buf[pos++] = obj_id & 0xFF;
    buf[pos++] = 0xFA;  // float32
    uint32_t raw;
    memcpy(&raw, &value, 4);
    raw = __builtin_bswap32(raw);
    memcpy(&buf[pos], &raw, 4);
    pos += 4;
}

void MPPTManager::encodeCborBool(uint8_t *buf, uint8_t &pos, uint16_t obj_id, bool value)
{
    buf[pos++] = 0xA1;  // map(1)
    buf[pos++] = 0x19;  // uint16 follows
    buf[pos++] = (obj_id >> 8) & 0xFF;
    buf[pos++] = obj_id & 0xFF;
    buf[pos++] = value ? 0xF5 : 0xF4;  // CBOR true / false
}

void MPPTManager::sendThingSetRequest(uint16_t target_id, const uint8_t *data, uint8_t len)
{
    uint32_t can_id = THINGSET_REQRESP_BASE | (5UL << 20) | ((uint32_t)target_id & 0xFFFF);
    send_ext_canbus_message(can_id, data, len);
}

#ifdef MPPT_MOCK_MODE
void MPPTManager::updateMockDevices()
{
    if (!_settings) return;

    int64_t now = esp_timer_get_time();
    if ((now - _mock_last_update_us) < 1000000LL) return;  // update every second
    _mock_last_update_us = now;

    uint8_t count = _settings->mppt_mock_device_count;
    if (count > MAX_MPPT_DEVICES) count = MAX_MPPT_DEVICES;

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        _device_count = count;
        for (uint8_t i = 0; i < count; i++)
        {
            _devices[i].node_id = THINGSET_MPPT_ID_MIN + i;
            _devices[i].status = MPPT_ONLINE;
            _devices[i].last_seen_us = now;
            _devices[i].charging_enabled = true;

            float base_solar_v = 35.0f + (float)(i * 2);
            float base_solar_i = 8.0f + (float)(i) * 0.5f;
            _devices[i].solar_voltage = base_solar_v + (float)(now / 1000000 % 3);
            _devices[i].solar_current = base_solar_i + (float)(now / 2000000 % 2);
            _devices[i].solar_power = _devices[i].solar_voltage * _devices[i].solar_current;
            _devices[i].battery_voltage = 52.0f + (float)(i) * 0.1f;
            _devices[i].battery_current = 5.0f + (float)(i);
            _devices[i].temperature = 25 + (int16_t)(i * 2);
            _devices[i].charge_state = 3;  // bulk charging
            _devices[i].daily_energy_wh = 1000.0f * (float)(i + 1);
        }
        xSemaphoreGive(mutex);
    }
}
#endif
