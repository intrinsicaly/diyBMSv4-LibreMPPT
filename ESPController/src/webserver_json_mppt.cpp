#define USE_ESP_IDF_LOG 1
static constexpr const char *const TAG = "diybms-mpptapi";

#include "webserver.h"
#include "webserver_helper_funcs.h"
#include "webserver_json_mppt.h"
#include "mppt_canbus.h"
#include <freertos/semphr.h>

esp_err_t content_handler_mppt(httpd_req_t *req)
{
    int bufferused = 0;

    bufferused += snprintf(&httpbuf[bufferused], BUFSIZE - bufferused,
                           "{\"enabled\":%s,\"mock\":%s,"
                           "\"target_voltage\":%.1f,\"max_current\":%.1f,"
                           "\"abs_voltage\":%.1f,\"float_voltage\":%.1f,"
                           "\"timeout\":%u,\"timeout_action\":%u,"
                           "\"mock_count\":%u,\"count\":%u,\"devices\":[",
                           mysettings.mppt_can_enabled ? "true" : "false",
                           mysettings.mppt_mock_mode_enabled ? "true" : "false",
                           mysettings.mppt_target_voltage / 10.0f,
                           mysettings.mppt_max_charge_current / 10.0f,
                           mysettings.mppt_absorption_voltage / 10.0f,
                           mysettings.mppt_float_voltage / 10.0f,
                           mysettings.mppt_timeout_seconds,
                           mysettings.mppt_timeout_action,
                           mysettings.mppt_mock_device_count,
                           mppt_manager.getDeviceCount());

    if (xSemaphoreTake(mppt_manager.mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        for (uint8_t i = 0; i < mppt_manager.getDeviceCount(); i++)
        {
            const MPPTDevice *dev = mppt_manager.getDevice(i);
            if (!dev) continue;

            if (i > 0) bufferused += snprintf(&httpbuf[bufferused], BUFSIZE - bufferused, ",");

            bufferused += snprintf(&httpbuf[bufferused], BUFSIZE - bufferused,
                                   "{\"id\":%u,\"status\":%u,"
                                   "\"solar_v\":%.2f,\"solar_i\":%.2f,\"solar_p\":%.1f,"
                                   "\"bat_v\":%.2f,\"bat_i\":%.2f,"
                                   "\"temp\":%d,\"state\":%u,\"e_day\":%.1f,\"charge_en\":%s}",
                                   dev->node_id,
                                   dev->status,
                                   dev->solar_voltage,
                                   dev->solar_current,
                                   dev->solar_power,
                                   dev->battery_voltage,
                                   dev->battery_current,
                                   dev->temperature,
                                   dev->charge_state,
                                   dev->daily_energy_wh,
                                   dev->charging_enabled ? "true" : "false");
        }
        xSemaphoreGive(mppt_manager.mutex);
    }

    bufferused += snprintf(&httpbuf[bufferused], BUFSIZE - bufferused, "]}");
    return httpd_resp_send(req, httpbuf, bufferused);
}

esp_err_t post_savemppt_json_handler(httpd_req_t *req, bool urlEncoded)
{
    mysettings.mppt_can_enabled = false;
    mysettings.mppt_mock_mode_enabled = false;

    GetKeyValue(httpbuf, "mpptEnabled", &mysettings.mppt_can_enabled, urlEncoded);
    GetKeyValue(httpbuf, "mpptMockMode", &mysettings.mppt_mock_mode_enabled, urlEncoded);
    GetKeyValue(httpbuf, "mpptTargetV", &mysettings.mppt_target_voltage, urlEncoded);
    GetKeyValue(httpbuf, "mpptMaxI", &mysettings.mppt_max_charge_current, urlEncoded);
    GetKeyValue(httpbuf, "mpptAbsV", &mysettings.mppt_absorption_voltage, urlEncoded);
    GetKeyValue(httpbuf, "mpptFloatV", &mysettings.mppt_float_voltage, urlEncoded);
    GetKeyValue(httpbuf, "mpptTimeout", &mysettings.mppt_timeout_seconds, urlEncoded);
    GetKeyValue(httpbuf, "mpptTimeoutAction", &mysettings.mppt_timeout_action, urlEncoded);
    GetKeyValue(httpbuf, "mpptMockCount", &mysettings.mppt_mock_device_count, urlEncoded);

    saveConfiguration();
    return SendSuccess(req);
}

esp_err_t post_mpptcontrol_json_handler(httpd_req_t *req, bool urlEncoded)
{
    uint32_t mppt_id_u32 = 0;
    if (!GetKeyValue(httpbuf, "id", &mppt_id_u32, urlEncoded))
    {
        return SendFailure(req);
    }

    uint16_t mppt_id = (uint16_t)mppt_id_u32;
    bool enable = false;
    float voltage = 0.0f, current = 0.0f;

    if (GetKeyValue(httpbuf, "enable", &enable, urlEncoded))
    {
        mppt_manager.sendControl(mppt_id, enable);
    }

    if (GetKeyValue(httpbuf, "voltage", &voltage, urlEncoded))
    {
        mppt_manager.sendVoltageLimit(mppt_id, voltage);
    }

    if (GetKeyValue(httpbuf, "current", &current, urlEncoded))
    {
        mppt_manager.sendCurrentLimit(mppt_id, current);
    }

    return SendSuccess(req);
}
