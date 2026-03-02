#define USE_ESP_IDF_LOG 1
static constexpr const char *const TAG = "diybms-mppt";

#include "mppt_canbus.h"
#include "mppt_config.h"
#include "mppt_port.h"
#include <string.h>
#include <stdio.h>

MPPTManager mppt_manager;

#define DISCOVERY_INTERVAL_US  (30LL * 1000000LL)  // 30 seconds
#define TIMEOUT_US(sec)        ((int64_t)(sec) * 1000000LL)

// Timeout thresholds in microseconds
#define WARNING_TIMEOUT_US   (MPPTConfig::DEVICE_TIMEOUT_WARNING_MS  * 1000LL)
#define OFFLINE_TIMEOUT_US   (MPPTConfig::DEVICE_TIMEOUT_OFFLINE_MS  * 1000LL)

// ----------------------------------------------------------------------------
// Free function: deviceStateToString
// ----------------------------------------------------------------------------

const char* deviceStateToString(DeviceState state)
{
    switch (state) {
        case DeviceState::UNKNOWN:         return "UNKNOWN";
        case DeviceState::DISCOVERING:     return "DISCOVERING";
        case DeviceState::ONLINE:          return "ONLINE";
        case DeviceState::DEGRADED:        return "DEGRADED";
        case DeviceState::TIMEOUT_WARNING: return "TIMEOUT_WARNING";
        case DeviceState::OFFLINE:         return "OFFLINE";
        case DeviceState::ERROR:           return "ERROR";
        default:                           return "INVALID";
    }
}

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

MPPTManager::MPPTManager()
    : _device_count(0), _settings(nullptr), _rules(nullptr), _last_discovery_us(0)
{
    mutex = xSemaphoreCreateMutex();
    memset(_devices, 0, sizeof(_devices));
    memset(&_error_stats, 0, sizeof(_error_stats));
    memset(&_statistics, 0, sizeof(_statistics));
    memset(&_retry_state, 0, sizeof(_retry_state));

    // Default recovery configuration
    _recovery_config.max_retries                = MPPTConfig::MAX_RETRIES;
    _recovery_config.initial_retry_delay_ms     = MPPTConfig::INITIAL_RETRY_DELAY_MS;
    _recovery_config.max_retry_delay_ms         = MPPTConfig::MAX_RETRY_DELAY_MS;
    _recovery_config.enable_exponential_backoff = true;

#ifdef MPPT_MOCK_MODE
    _mock_last_update_us = 0;
#endif
}

// ----------------------------------------------------------------------------
// init
// ----------------------------------------------------------------------------

void MPPTManager::init(const diybms_eeprom_settings *settings, Rules *rules)
{
    if (!settings || !rules) {
        ESP_LOGE(TAG, "NULL settings or rules pointer passed to MPPTManager::init");
        return;
    }

    _settings = settings;
    _rules = rules;

    if (_settings->mppt_target_voltage < 4000 || _settings->mppt_target_voltage > 7000) {
        ESP_LOGW(TAG, "MPPT target voltage (%u mV) out of safe range [4000-7000]; using configured value",
                 _settings->mppt_target_voltage);
    }

    if (_settings->mppt_max_charge_current > 10000) {
        ESP_LOGW(TAG, "MPPT max charge current (%u) out of safe range [0-10000]; using configured value",
                 _settings->mppt_max_charge_current);
    }

    ESP_LOGI(TAG, "MPPT Manager initialized, enabled=%d, target_V=%u mV, max_I=%u",
             _settings->mppt_can_enabled,
             _settings->mppt_target_voltage,
             _settings->mppt_max_charge_current);
}

// ----------------------------------------------------------------------------
// update
// ----------------------------------------------------------------------------

void MPPTManager::update()
{
    if (!_settings || !_settings->mppt_can_enabled) return;

    int64_t now = mppt_now_us();

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

    // Update state machines for all devices
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        for (uint8_t i = 0; i < _device_count; i++)
        {
            updateDeviceStateMachine(&_devices[i]);
        }
        xSemaphoreGive(mutex);
    }

    // Update runtime statistics
    updateStatistics();

    // Handle degraded operation
    handleDegradedOperation();

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

// ----------------------------------------------------------------------------
// processReceivedMessage
// ----------------------------------------------------------------------------

void MPPTManager::processReceivedMessage(const twai_message_t *msg)
{
    if (!_settings || !_settings->mppt_can_enabled) return;

    if (!msg || msg->data_length_code > sizeof(msg->data)) {
        ESP_LOGE(TAG, "Invalid CAN message: NULL pointer or invalid DLC");
        _error_stats.total_receive_errors++;
        return;
    }

    uint32_t id = msg->identifier;

    uint32_t base = id & 0xFF000000UL;
    uint16_t source = (uint16_t)(id & 0x0000FFFFUL);

    if (base != THINGSET_PUBSUB_BASE) return;
    if (source < THINGSET_MPPT_ID_MIN || source > THINGSET_MPPT_ID_MAX) return;

    ESP_LOGD(TAG, "MPPT telemetry from 0x%04X, len=%d", source, msg->data_length_code);

    _statistics.total_messages_received++;

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
            _devices[idx].last_seen_us = mppt_now_us();
            _devices[idx].status = MPPT_ONLINE;
            _devices[idx].total_messages_received++;

            // Parse CBOR-encoded telemetry: 0xA1 0x19 <ID_HI> <ID_LO> <value>
            const uint8_t *d = msg->data;
            uint8_t len = msg->data_length_code;

            bool decoded = false;

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
                    case THINGSET_ID_V_SOLAR:
                        if (val >= MPPTConfig::MIN_SOLAR_VOLTAGE && val <= MPPTConfig::MAX_SOLAR_VOLTAGE) {
                            _devices[idx].solar_voltage = val;
                        } else {
                            ESP_LOGW(TAG, "Solar voltage out of range: %.2f V (node 0x%04X)", val, source);
                            _devices[idx].solar_voltage = 0.0f;
                            _error_stats.total_validation_errors++;
                        }
                        decoded = true;
                        break;
                    case THINGSET_ID_I_SOLAR:
                        if (val >= MPPTConfig::MIN_CURRENT && val <= MPPTConfig::MAX_CURRENT) {
                            _devices[idx].solar_current = val;
                        } else {
                            ESP_LOGW(TAG, "Solar current out of range: %.2f A (node 0x%04X)", val, source);
                            _devices[idx].solar_current = 0.0f;
                            _error_stats.total_validation_errors++;
                        }
                        decoded = true;
                        break;
                    case THINGSET_ID_P_SOLAR:
                        if (val >= 0.0f && val <= MPPTConfig::MAX_POWER) {
                            _devices[idx].solar_power = val;
                        } else {
                            ESP_LOGW(TAG, "Solar power out of range: %.2f W (node 0x%04X)", val, source);
                            _devices[idx].solar_power = 0.0f;
                            _error_stats.total_validation_errors++;
                        }
                        decoded = true;
                        break;
                    case THINGSET_ID_V_BAT:
                        if (val >= MPPTConfig::MIN_BATTERY_VOLTAGE && val <= MPPTConfig::MAX_BATTERY_VOLTAGE) {
                            _devices[idx].battery_voltage = val;
                        } else {
                            ESP_LOGW(TAG, "Battery voltage out of range: %.2f V (node 0x%04X)", val, source);
                            _devices[idx].battery_voltage = 0.0f;
                            _error_stats.total_validation_errors++;
                        }
                        decoded = true;
                        break;
                    case THINGSET_ID_I_BAT:
                        if (val >= MPPTConfig::MIN_CURRENT && val <= MPPTConfig::MAX_CURRENT) {
                            _devices[idx].battery_current = val;
                        } else {
                            ESP_LOGW(TAG, "Battery current out of range: %.2f A (node 0x%04X)", val, source);
                            _devices[idx].battery_current = 0.0f;
                            _error_stats.total_validation_errors++;
                        }
                        decoded = true;
                        break;
                    case THINGSET_ID_E_DAY:
                        _devices[idx].daily_energy_wh = val;
                        decoded = true;
                        break;
                    default:
                        break;
                    }
                }
                // int16 temperature (0x19 + 2 bytes)
                else if (len >= 7 && d[4] == 0x19)
                {
                    int16_t val16 = (int16_t)(((uint16_t)d[5] << 8) | d[6]);
                    if (obj_id == THINGSET_ID_TEMP) {
                        _devices[idx].temperature = val16;
                        decoded = true;
                    }
                }
                // small uint (0x00-0x17)
                else if (len >= 5 && d[4] <= 0x17)
                {
                    if (obj_id == THINGSET_ID_STATE) {
                        _devices[idx].charge_state = d[4];
                        decoded = true;
                    }
                }
            }

            if (decoded) {
                _devices[idx].consecutive_errors = 0;
            } else {
                // CBOR parsed but no matching field - not necessarily an error
                // (could be an unknown object ID)
            }

            // Update state machine
            updateDeviceStateMachine(&_devices[idx]);
        }
        xSemaphoreGive(mutex);
    }
}

// ----------------------------------------------------------------------------
// sendControl
// ----------------------------------------------------------------------------

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

    _statistics.total_control_commands_sent++;
    ESP_LOGI(TAG, "MPPT 0x%04X charge %s", mppt_id, enable_charge ? "enabled" : "disabled");
    return true;
}

// ----------------------------------------------------------------------------
// sendVoltageLimit
// ----------------------------------------------------------------------------

bool MPPTManager::sendVoltageLimit(uint16_t mppt_id, float voltage)
{
    if (!_settings || !_settings->mppt_can_enabled) return false;

    uint8_t buf[9];
    uint8_t pos = 0;
    encodeCborFloat(buf, pos, THINGSET_ID_TARGET_V, voltage);
    sendThingSetRequest(mppt_id, buf, pos);
    return true;
}

// ----------------------------------------------------------------------------
// sendCurrentLimit
// ----------------------------------------------------------------------------

bool MPPTManager::sendCurrentLimit(uint16_t mppt_id, float current)
{
    if (!_settings || !_settings->mppt_can_enabled) return false;

    uint8_t buf[9];
    uint8_t pos = 0;
    encodeCborFloat(buf, pos, THINGSET_ID_MAX_CURR, current);
    sendThingSetRequest(mppt_id, buf, pos);
    return true;
}

// ----------------------------------------------------------------------------
// getDeviceCount / getDevice
// ----------------------------------------------------------------------------

uint8_t MPPTManager::getDeviceCount() const
{
    return _device_count;
}

const MPPTDevice *MPPTManager::getDevice(uint8_t index) const
{
    if (index >= _device_count) return nullptr;
    return &_devices[index];
}

// ----------------------------------------------------------------------------
// Error tracking
// ----------------------------------------------------------------------------

void MPPTManager::logError(const char* context, const char* message)
{
    _error_stats.last_error_timestamp = (uint32_t)(mppt_now_us() / 1000000LL);
    snprintf(_error_stats.last_error_message, sizeof(_error_stats.last_error_message),
             "%s: %s", context, message);
    ESP_LOGE(TAG, "[%s] %s", context, message);
}

void MPPTManager::resetErrorStats()
{
    memset(&_error_stats, 0, sizeof(_error_stats));
}

// ----------------------------------------------------------------------------
// Recovery configuration
// ----------------------------------------------------------------------------

void MPPTManager::setRecoveryConfig(const RecoveryConfig& config)
{
    _recovery_config = config;
    ESP_LOGI(TAG, "Recovery config: max_retries=%u, initial_delay=%ums, backoff=%s",
             config.max_retries, config.initial_retry_delay_ms,
             config.enable_exponential_backoff ? "enabled" : "disabled");
}

uint32_t MPPTManager::calculateBackoffDelay(uint8_t retry_count)
{
    if (!_recovery_config.enable_exponential_backoff) {
        return _recovery_config.initial_retry_delay_ms;
    }

    // Exponential backoff: delay = initial * 2^retry_count
    // Cap shift to avoid overflow (max 31 bits)
    uint8_t shift = retry_count < 31 ? retry_count : 31;
    uint32_t delay = _recovery_config.initial_retry_delay_ms << shift;

    // Cap at max delay
    if (delay > _recovery_config.max_retry_delay_ms || delay < _recovery_config.initial_retry_delay_ms) {
        delay = _recovery_config.max_retry_delay_ms;
    }

    return delay;
}

bool MPPTManager::sendWithRetry(uint32_t can_id, const uint8_t* data, uint8_t len, uint8_t device_idx)
{
    if (device_idx >= MAX_MPPT_DEVICES) return false;

    RetryState& retry = _retry_state[device_idx];

    // Check if we're in backoff period
    if (retry.in_backoff) {
        uint32_t now = (uint32_t)(mppt_now_us() / 1000LL);
        if (now < retry.next_retry_time) {
            ESP_LOGD(TAG, "Device %u in backoff, waiting %ums",
                     device_idx, retry.next_retry_time - now);
            return false;
        }
        retry.in_backoff = false;
    }

    // Attempt to send
    if (mppt_can_send(can_id, data, len)) {
        // Success - reset retry state
        retry.retry_count = 0;
        retry.in_backoff = false;
        _statistics.total_messages_sent++;
        return true;
    }

    // Failed - increment retry count
    retry.retry_count++;
    _error_stats.total_send_failures++;

    if (retry.retry_count >= _recovery_config.max_retries) {
        ESP_LOGE(TAG, "Max retries (%u) reached for device %u",
                 _recovery_config.max_retries, device_idx);
        retry.retry_count = 0;  // Reset for next attempt
        return false;
    }

    // Calculate backoff delay
    uint32_t backoff_ms = calculateBackoffDelay(retry.retry_count);
    retry.next_retry_time = (uint32_t)(mppt_now_us() / 1000LL) + backoff_ms;
    retry.in_backoff = true;

    ESP_LOGW(TAG, "Send failed for device %u, retry %u/%u in %ums",
             device_idx, retry.retry_count, _recovery_config.max_retries, backoff_ms);

    return false;
}

// ----------------------------------------------------------------------------
// State machine
// ----------------------------------------------------------------------------

void MPPTManager::transitionDeviceState(MPPTDevice* device, DeviceState new_state, const char* reason)
{
    if (!device) return;

    if (device->state == new_state) {
        return;  // No change
    }

    DeviceState old_state = device->state;
    device->previous_state = old_state;
    device->state = new_state;
    device->state_entered_time = mppt_now_us();
    device->state_transition_count++;

    // Update legacy status field for backward compatibility
    switch (new_state) {
        case DeviceState::ONLINE:
        case DeviceState::DEGRADED:
            device->status = MPPT_ONLINE;
            break;
        case DeviceState::OFFLINE:
        case DeviceState::TIMEOUT_WARNING:
            device->status = MPPT_TIMEOUT;
            break;
        default:
            break;
    }

    ESP_LOGI(TAG, "MPPT 0x%04X state: %s -> %s (%s)",
             device->node_id,
             deviceStateToString(old_state),
             deviceStateToString(new_state),
             reason ? reason : "");

    // State-specific actions
    switch (new_state) {
        case DeviceState::ONLINE:
            device->consecutive_errors = 0;
            break;

        case DeviceState::DEGRADED:
            ESP_LOGW(TAG, "MPPT 0x%04X degraded: %u consecutive errors",
                     device->node_id, device->consecutive_errors);
            break;

        case DeviceState::OFFLINE:
            _error_stats.total_timeouts++;
            ESP_LOGW(TAG, "MPPT 0x%04X went offline", device->node_id);
            break;

        case DeviceState::ERROR:
            ESP_LOGE(TAG, "MPPT 0x%04X entered ERROR state", device->node_id);
            break;

        default:
            break;
    }
}

void MPPTManager::updateDeviceStateMachine(MPPTDevice* device)
{
    if (!device || device->node_id == 0) return;

    int64_t now = mppt_now_us();
    int64_t time_since_seen = now - device->last_seen_us;

    switch (device->state) {
        case DeviceState::UNKNOWN:
        case DeviceState::DISCOVERING:
            if (device->total_messages_received > 0) {
                transitionDeviceState(device, DeviceState::ONLINE, "First message received");
            }
            break;

        case DeviceState::ONLINE:
            if (time_since_seen > OFFLINE_TIMEOUT_US) {
                transitionDeviceState(device, DeviceState::OFFLINE, "Timeout");
            } else if (time_since_seen > WARNING_TIMEOUT_US) {
                transitionDeviceState(device, DeviceState::TIMEOUT_WARNING, "Warning timeout");
            } else if (device->consecutive_errors >= MPPTConfig::DEGRADED_ERROR_THRESHOLD) {
                transitionDeviceState(device, DeviceState::DEGRADED, "Multiple errors");
            }
            break;

        case DeviceState::DEGRADED:
            if (time_since_seen > OFFLINE_TIMEOUT_US) {
                transitionDeviceState(device, DeviceState::OFFLINE, "Timeout");
            } else if (device->consecutive_errors == 0) {
                transitionDeviceState(device, DeviceState::ONLINE, "Errors cleared");
            } else if (device->consecutive_errors > MPPTConfig::ERROR_STATE_THRESHOLD) {
                transitionDeviceState(device, DeviceState::ERROR, "Too many errors");
            }
            break;

        case DeviceState::TIMEOUT_WARNING:
            if (time_since_seen > OFFLINE_TIMEOUT_US) {
                transitionDeviceState(device, DeviceState::OFFLINE, "Timeout");
            } else if (time_since_seen < WARNING_TIMEOUT_US) {
                transitionDeviceState(device, DeviceState::ONLINE, "Recovered");
            }
            break;

        case DeviceState::OFFLINE:
            if (time_since_seen < WARNING_TIMEOUT_US) {
                transitionDeviceState(device, DeviceState::ONLINE, "Back online");
            }
            break;

        case DeviceState::ERROR:
            // Manual recovery required or long timeout (60 seconds)
            if ((now - device->state_entered_time) > 60000000LL) {
                transitionDeviceState(device, DeviceState::UNKNOWN, "Reset after error");
            }
            break;
    }
}

const char* MPPTManager::getDeviceStateName(uint16_t node_id)
{
    int idx = findDevice(node_id);
    if (idx < 0) return "NOT_FOUND";
    return deviceStateToString(_devices[idx].state);
}

// ----------------------------------------------------------------------------
// Statistics
// ----------------------------------------------------------------------------

void MPPTManager::updateStatistics()
{
    _statistics.uptime_seconds = (uint32_t)(mppt_now_us() / 1000000LL);

    // Update per-device uptime percentage
    int64_t now = mppt_now_us();
    for (int i = 0; i < MAX_MPPT_DEVICES; i++) {
        MPPTDevice& dev = _devices[i];
        if (dev.node_id == 0) continue;

        if (now > 0 && (dev.state == DeviceState::ONLINE || dev.state == DeviceState::DEGRADED)) {
            int64_t time_in_state = now - dev.state_entered_time;
            if (time_in_state < 0) time_in_state = 0;
            dev.uptime_percentage = (float)time_in_state / (float)now * 100.0f;
        }
    }
}

void MPPTManager::dumpStatistics()
{
    ESP_LOGI(TAG, "=== MPPT Statistics ===");
    ESP_LOGI(TAG, "Uptime: %u seconds", _statistics.uptime_seconds);
    ESP_LOGI(TAG, "Messages sent: %llu", (unsigned long long)_statistics.total_messages_sent);
    ESP_LOGI(TAG, "Messages received: %llu", (unsigned long long)_statistics.total_messages_received);
    ESP_LOGI(TAG, "Discoveries sent: %llu", (unsigned long long)_statistics.total_discoveries_sent);
    ESP_LOGI(TAG, "Control commands: %llu", (unsigned long long)_statistics.total_control_commands_sent);
    ESP_LOGI(TAG, "Active devices: %u", _device_count);

    float avg_messages = 0.0f;
    if (_device_count > 0) {
        uint64_t total = 0;
        for (int i = 0; i < MAX_MPPT_DEVICES; i++) {
            if (_devices[i].node_id != 0) {
                total += _devices[i].total_messages_received;
            }
        }
        avg_messages = (float)total / (float)_device_count;
    }
    ESP_LOGI(TAG, "Avg messages per device: %.1f", avg_messages);
    ESP_LOGI(TAG, "=======================");
}

// ----------------------------------------------------------------------------
// Diagnostics
// ----------------------------------------------------------------------------

void MPPTManager::dumpDiagnostics()
{
    ESP_LOGI(TAG, "=== MPPT Manager Diagnostics ===");
    ESP_LOGI(TAG, "Device count: %u / %u", _device_count, MAX_MPPT_DEVICES);
    ESP_LOGI(TAG, "Error stats:");
    ESP_LOGI(TAG, "  Send failures: %u", _error_stats.total_send_failures);
    ESP_LOGI(TAG, "  Receive errors: %u", _error_stats.total_receive_errors);
    ESP_LOGI(TAG, "  Decode errors: %u", _error_stats.total_decode_errors);
    ESP_LOGI(TAG, "  Timeouts: %u", _error_stats.total_timeouts);
    ESP_LOGI(TAG, "  Validation errors: %u", _error_stats.total_validation_errors);

    if (_error_stats.last_error_timestamp > 0) {
        ESP_LOGI(TAG, "  Last error: %s (at %us)",
                 _error_stats.last_error_message,
                 _error_stats.last_error_timestamp);
    }

    ESP_LOGI(TAG, "Devices:");
    for (int i = 0; i < MAX_MPPT_DEVICES; i++) {
        MPPTDevice& dev = _devices[i];
        if (dev.node_id == 0) continue;

        /* now is used by ESP_LOGI below (suppressed as no-op in test builds) */
        int64_t now = mppt_now_us();
        (void)now;
        ESP_LOGI(TAG, "  [%d] MPPT 0x%04X:", i, dev.node_id);
        ESP_LOGI(TAG, "    State: %s (for %lld ms)",
                 deviceStateToString(dev.state),
                 (now - dev.state_entered_time) / 1000LL);
        ESP_LOGI(TAG, "    Last seen: %lld ms ago",
                 (now - dev.last_seen_us) / 1000LL);
        ESP_LOGI(TAG, "    Messages: %u (errors: %u)",
                 dev.total_messages_received,
                 dev.total_decode_errors);
        ESP_LOGI(TAG, "    Telemetry: V_sol=%.2fV, I_sol=%.2fA, V_bat=%.2fV, P=%.1fW",
                 dev.solar_voltage, dev.solar_current,
                 dev.battery_voltage, dev.solar_power);
    }
    ESP_LOGI(TAG, "================================");
}

void MPPTManager::dumpDeviceDetail(uint16_t node_id)
{
    int idx = findDevice(node_id);
    if (idx < 0) {
        ESP_LOGE(TAG, "Device 0x%04X not found", node_id);
        return;
    }

    /* device and now are used by ESP_LOGI below (suppressed as no-op in test builds) */
    MPPTDevice& device = _devices[idx];
    int64_t now = mppt_now_us();
    (void)device;
    (void)now;

    ESP_LOGI(TAG, "=== MPPT 0x%04X Detail ===", node_id);
    ESP_LOGI(TAG, "State: %s", deviceStateToString(device.state));
    ESP_LOGI(TAG, "Previous state: %s", deviceStateToString(device.previous_state));
    ESP_LOGI(TAG, "State transitions: %u", device.state_transition_count);
    ESP_LOGI(TAG, "Time in current state: %lld ms",
             (now - device.state_entered_time) / 1000LL);
    ESP_LOGI(TAG, "Last seen: %lld ms ago",
             (now - device.last_seen_us) / 1000LL);
    ESP_LOGI(TAG, "Total messages: %u", device.total_messages_received);
    ESP_LOGI(TAG, "Decode errors: %u (consecutive: %u)",
             device.total_decode_errors, device.consecutive_errors);
    ESP_LOGI(TAG, "Uptime: %.1f%%", device.uptime_percentage);
    ESP_LOGI(TAG, "Current telemetry:");
    ESP_LOGI(TAG, "  Solar: %.2fV @ %.2fA = %.1fW",
             device.solar_voltage, device.solar_current, device.solar_power);
    ESP_LOGI(TAG, "  Battery: %.2fV @ %.2fA",
             device.battery_voltage, device.battery_current);
    ESP_LOGI(TAG, "========================");
}

// ----------------------------------------------------------------------------
// Graceful degradation
// ----------------------------------------------------------------------------

void MPPTManager::handleDegradedOperation()
{
    uint8_t online_count = 0;
    uint8_t degraded_count = 0;

    for (int i = 0; i < MAX_MPPT_DEVICES; i++) {
        if (_devices[i].node_id == 0) continue;

        if (_devices[i].state == DeviceState::ONLINE) {
            online_count++;
        } else if (_devices[i].state == DeviceState::DEGRADED) {
            degraded_count++;
        }
    }

    if (online_count == 0 && _device_count > 0) {
        ESP_LOGW(TAG, "No MPPT devices online - system degraded");
    }

    if (degraded_count > 0) {
        ESP_LOGW(TAG, "Partial system degradation: %u online, %u degraded",
                 online_count, degraded_count);
    }
}

// ----------------------------------------------------------------------------
// Internal helpers
// ----------------------------------------------------------------------------

void MPPTManager::sendDiscovery()
{
    uint32_t can_id = THINGSET_REQRESP_BASE | (5UL << 20) | THINGSET_BROADCAST_ID;
    uint8_t buf[] = {0xA1, 0x19, 0x1D, 0x00};
    if (!mppt_can_send(can_id, buf, sizeof(buf))) {
        _error_stats.total_send_failures++;
        logError("sendDiscovery", "Failed to send CAN broadcast");

        if (_error_stats.total_send_failures % 10 == 0) {
            ESP_LOGW(TAG, "Multiple send failures (%u), CAN bus may be down",
                     _error_stats.total_send_failures);
        }
    } else {
        _statistics.total_discoveries_sent++;
        _statistics.total_messages_sent++;
        ESP_LOGD(TAG, "MPPT discovery broadcast sent");
    }
}

void MPPTManager::checkTimeouts()
{
    if (!_settings) return;

    int64_t now = mppt_now_us();
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
                    _error_stats.total_timeouts++;
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
            _devices[_device_count].last_seen_us = mppt_now_us();
            _devices[_device_count].state = DeviceState::DISCOVERING;
            _devices[_device_count].previous_state = DeviceState::UNKNOWN;
            _devices[_device_count].state_entered_time = mppt_now_us();
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
    if (!mppt_can_send(can_id, data, len)) {
        _error_stats.total_send_failures++;
        ESP_LOGW(TAG, "Failed to send ThingSet request to 0x%04X", target_id);
    } else {
        _statistics.total_messages_sent++;
    }
}

#ifdef MPPT_MOCK_MODE
void MPPTManager::updateMockDevices()
{
    if (!_settings) return;

    int64_t now = mppt_now_us();
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
            _devices[i].state = DeviceState::ONLINE;

            float base_solar_v = 35.0f + (float)(i * 2);
            float base_solar_i = 8.0f + (float)(i * 1) * 0.5f;
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

