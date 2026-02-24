/**
 * @file test_globals.cpp
 * @brief Definitions of all global symbols required by the code under test.
 *
 * Having a single translation unit own these definitions prevents
 * multiple-definition linker errors when several test files are compiled
 * together. Every other test file should include test_globals.h for the
 * extern declarations.
 */

#include "test_globals.h"

diybms_eeprom_settings mysettings;
Rules rules;
CellModuleInfo cmi[maximum_controller_cell_modules];
TaskHandle_t voltageandstatussnapshot_task_handle = nullptr;
