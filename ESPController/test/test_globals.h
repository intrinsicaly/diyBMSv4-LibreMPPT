#pragma once
/**
 * @file test_globals.h
 * @brief Shared global variable declarations for native unit tests.
 *
 * All global symbols required by the code under test are defined once in
 * test_globals.cpp and declared here. Include this header in every test
 * translation unit that needs these globals; do NOT redefine them.
 */

#include "defines.h"
#include "Rules.h"

/** Global EEPROM settings (referenced by mppt_canbus and others) */
extern diybms_eeprom_settings mysettings;

/** Global rules object (referenced by mppt_canbus) */
extern Rules rules;

/** Cell module info array (referenced by PacketReceiveProcessor) */
extern CellModuleInfo cmi[maximum_controller_cell_modules];

/** Voltage-snapshot task handle (referenced by PacketReceiveProcessor) */
extern TaskHandle_t voltageandstatussnapshot_task_handle;
