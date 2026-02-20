/*
 * Stub Rules implementation for unit testing.
 * Only the symbols needed to link mppt_canbus and PacketReceiveProcessor are provided.
 * Complex logic is replaced with no-ops or trivial returns.
 */

#include "Rules.h"

/* Static member definitions */
const std::array<std::string, 1 + MAXIMUM_RuleNumber> Rules::RuleTextDescription = {};
const std::array<std::string, 1 + MAXIMUM_InternalWarningCode>
    Rules::InternalWarningCodeDescription = {};
const std::array<std::string, 1 + MAXIMUM_InternalErrorCode>
    Rules::InternalErrorCodeDescription = {};

void Rules::setRuleStatus(Rule r, bool value)
{
    rule_outcome[(size_t)r] = value;
}

void Rules::setChargingMode(ChargingMode) {}

void Rules::ClearValues() {}
void Rules::ProcessCell(uint8_t, uint8_t, const CellModuleInfo *, uint16_t) {}
void Rules::ProcessBank(uint8_t) {}
void Rules::SetWarning(InternalWarningCode) {}
void Rules::SetError(InternalErrorCode) {}

bool Rules::SharedChargingDischargingRules(const diybms_eeprom_settings *) { return false; }
bool Rules::IsChargeAllowed(const diybms_eeprom_settings *)    { return true; }
bool Rules::IsDischargeAllowed(const diybms_eeprom_settings *) { return true; }

void Rules::CalculateChargingMode(const diybms_eeprom_settings *, const currentmonitoring_struct *) {}
void Rules::CalculateDynamicChargeVoltage(const diybms_eeprom_settings *, const CellModuleInfo *) {}
void Rules::CalculateDynamicChargeCurrent(const diybms_eeprom_settings *) {}
void Rules::RunRules(const int32_t *, const int32_t *, bool, uint16_t, const currentmonitoring_struct *) {}

uint16_t Rules::DynamicChargeVoltage() const { return 0; }
int16_t  Rules::DynamicChargeCurrent() const { return 0; }
uint16_t Rules::VoltageRangeInBank(uint8_t) const { return 0; }
uint16_t Rules::StateOfChargeWithRulesApplied(const diybms_eeprom_settings *, float) const { return 0; }
