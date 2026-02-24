#pragma once
/*
 * Helper for creating diybms_eeprom_settings with sensible test defaults
 */

#include "defines.h"
#include <cstring>

inline diybms_eeprom_settings make_test_settings()
{
    diybms_eeprom_settings s;
    memset(&s, 0, sizeof(s));

    /* MPPT settings */
    s.mppt_can_enabled        = true;
    s.mppt_target_voltage     = 560;   /* 56.0 V */
    s.mppt_max_charge_current = 200;   /* 20.0 A */
    s.mppt_timeout_seconds    = 60;    /* 60 s   */
    s.mppt_mock_mode_enabled  = false;
    s.mppt_mock_device_count  = 0;

    return s;
}
