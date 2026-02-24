# 🚀 MPPT Commissioning Procedure

**Document Version:** 1.0  
**Date:** 2026-02-20 06:03:36 UTC  
**System:** diyBMS v4 + Libre Solar MPPT via CAN Bus

---

## Table of Contents

1. [Overview](#overview)
2. [Pre-Commissioning Checklist](#pre-commissioning-checklist)
3. [Step 1: System Power-Up](#step-1-system-power-up-no-solar)
4. [Step 2: CAN Bus Verification](#step-2-can-bus-verification)
5. [Step 3: Configure Charge Parameters](#step-3-configure-charge-parameters)
6. [Step 4: Solar Connection](#step-4-solar-connection-staged-power-up)
7. [Step 5: System Behavior Validation](#step-5-system-behavior-validation)
8. [Step 6: Data Logging & Monitoring](#step-6-data-logging--monitoring)
9. [Step 7: 24-Hour Observation Period](#step-7-24-hour-observation-period)
10. [Step 8: Performance Validation](#step-8-performance-validation)
11. [Step 9: Documentation & Handover](#step-9-documentation--handover)
12. [Step 10: Post-Commissioning Follow-Up](#step-10-post-commissioning-follow-up)
13. [Emergency Procedures](#emergency-procedures)

---

## Overview

This procedure safely brings the diyBMS + MPPT CAN system online for the first time in a production environment.

**Duration:** 2-4 hours  
**Personnel:** Minimum 2 people (one on software, one on hardware)  
**Prerequisites:** All Phase 1-3 tests completed successfully

---

## Pre-Commissioning Checklist

### Documentation
- [ ] System wiring diagram printed and on-site
- [ ] Battery specifications documented (voltage, capacity, chemistry)
- [ ] Solar array specifications documented (Voc, Isc, power)
- [ ] MPPT specifications documented (max voltage, max current)
- [ ] Emergency procedures printed and posted
- [ ] Contact information for technical support available

### Configuration Backup
- [ ] Export diyBMS configuration (Settings → Backup)
- [ ] Export MPPT configuration (if available)
- [ ] Save to USB drive and keep on-site
- [ ] Document all custom settings in commissioning log

### Tools & Equipment On-Site
- [ ] Laptop with web browser
- [ ] Multimeter (calibrated)
- [ ] Clamp meter (for current)
- [ ] Infrared thermometer (for temperature checks)
- [ ] Insulated hand tools
- [ ] Safety glasses
- [ ] Insulated gloves
- [ ] Fire extinguisher
- [ ] First aid kit
- [ ] Flashlight
- [ ] Camera (for documentation)

---

## Step 1: System Power-Up (No Solar)

**Objective:** Verify all components power on correctly with battery only.

### 1.1 Battery Connection

⚠️ **STOP** - Double-check all wiring before this step

- [ ] Verify battery is at safe voltage (12V: 11-13V, 24V: 22-26V, 48V: 44-52V)
- [ ] Check polarity with multimeter BEFORE connecting
- [ ] Insert fuses in battery positive leads
- [ ] Connect battery to MPPT(s) - negative first, then positive
- [ ] Connect battery to diyBMS
- [ ] Power on diyBMS
- [ ] Wait 60 seconds for boot
- [ ] Connect to diyBMS web UI (http://diybms.local or IP address)

**Checkpoint:**
- ✅ diyBMS shows battery voltage correctly
- ✅ All cell modules responding
- ✅ No alarms active
- ✅ Web UI accessible

### 1.2 MPPT Power-Up

- [ ] Verify each MPPT powers on (LED indicators)
- [ ] Check MPPT displays battery voltage (if equipped with display)
- [ ] Verify MPPT node IDs are correctly configured:
  - MPPT 1: 0x0010
  - MPPT 2: 0x0011
  - MPPT 3: 0x0012
  - MPPT 4: 0x0013 (if applicable)
- [ ] Log all node IDs in commissioning record

**Checkpoint:**
- ✅ All MPPTs powered and showing battery voltage
- ✅ Node IDs confirmed

---

## Step 2: CAN Bus Verification

**Objective:** Confirm CAN communication before connecting solar power.

### 2.1 CAN Bus Integrity Check

- [ ] Measure CANH voltage: Should be ~3.5V (multimeter to GND)
- [ ] Measure CANL voltage: Should be ~1.5V (multimeter to GND)
- [ ] Measure differential: CANH - CANL = ~2.0V
- [ ] Check termination resistance: 60Ω between CANH and CANL
- [ ] Verify twisted pair used for CANH/CANL (not parallel wires)
- [ ] Check wire length: <40m for 500kbps

**Checkpoint:**
- ✅ CAN bus voltages correct
- ✅ Proper termination
- ✅ Wiring meets specifications

### 2.2 MPPT Discovery

- [ ] Navigate to diyBMS web UI → MPPT Control page
- [ ] Disable mock mode (if enabled)
- [ ] Wait 30 seconds for automatic discovery
- [ ] Verify all MPPTs discovered and listed
- [ ] Check each MPPT shows:
  - Node ID
  - State: "Online"
  - Battery voltage (matches multimeter)
  - Solar voltage: 0V (no solar connected yet)
  - Temperature: ambient (20-30°C typical)

**If discovery fails:**
- [ ] Check CAN bus wiring
- [ ] Verify termination resistors
- [ ] Check MPPT node ID configuration
- [ ] Check diyBMS logs for CAN errors
- [ ] Power cycle MPPTs one at a time

**Checkpoint:**
- ✅ All MPPTs discovered
- ✅ Telemetry receiving correctly
- ✅ No CAN bus errors

---

## Step 3: Configure Charge Parameters

**Objective:** Set safe, optimal charging parameters before connecting solar.

### 3.1 Battery-Specific Settings

**For LiFePO4 (LFP) Batteries:**
```
Target Voltage:     3.50V × cells = ____ V total
Absorption Voltage: 3.55V × cells = ____ V total  
Float Voltage:      3.40V × cells = ____ V total (or disabled)
Max Charge Current: Battery Ah ÷ 2 = ____ A per MPPT
Temp Compensation:  0 mV/°C (disable for LiFePO4)
```

**For Lead-Acid (AGM/Gel) Batteries:**
```
Target Voltage:     2.35V × cells = ____ V total
Absorption Voltage: 2.40V × cells = ____ V total
Float Voltage:      2.25V × cells = ____ V total
Max Charge Current: Battery Ah ÷ 5 = ____ A per MPPT
Temp Compensation:  -3 mV/°C/cell
```

- [ ] Calculate values for your battery (fill in blanks above)
- [ ] Enter settings in diyBMS web UI → MPPT Control → Settings
- [ ] Save configuration
- [ ] Verify settings persist after page refresh

### 3.2 Safety Limits

- [ ] Set timeout: 10 seconds (default)
- [ ] Set timeout action: "Stop Charging" (safest)
- [ ] Configure BMS over-voltage alarm: Cell voltage × 1.05
- [ ] Configure BMS over-current alarm: Total current × 1.1
- [ ] Configure BMS over-temperature alarm: 45°C (LFP) or 40°C (lead-acid)

**Checkpoint:**
- ✅ Charge parameters configured correctly
- ✅ Safety limits set
- ✅ Configuration saved

---

## Step 4: Solar Connection (Staged Power-Up)

**Objective:** Safely connect solar panels and verify MPPT operation.

### 4.1 Pre-Connection Solar Checks

⚠️ **WARNING:** Solar panels generate voltage even in low light

- [ ] Measure solar array Voc (open-circuit voltage) with multimeter
- [ ] Verify Voc is within MPPT input range (typically 15-150V)
- [ ] Check polarity of solar array (positive and negative marked)
- [ ] Verify no short circuits in solar wiring
- [ ] Confirm MC4 connectors (or equivalent) are secure

### 4.2 First MPPT Solar Connection

**Start with ONE MPPT only:**

- [ ] Cover solar panels with opaque material (or wait for low light)
- [ ] Connect solar array to MPPT 1 (negative first, then positive)
- [ ] Verify MPPT shows solar voltage (should be low due to cover)
- [ ] Uncover panels gradually (remove one layer at a time)
- [ ] Watch solar voltage increase in web UI
- [ ] Verify MPPT does NOT start charging (disabled by default)
- [ ] Check for any alarms (should be none)

**Checkpoint:**
- ✅ Solar voltage detected
- ✅ No alarms or errors
- ✅ MPPT in standby (not charging)

### 4.3 Enable Charging on MPPT 1

- [ ] Set target voltage to calculated value (from Step 3.1)
- [ ] Set current limit to 50% of max initially (e.g., 5A for 10A MPPT)
- [ ] Click "Enable Charging" on MPPT 1
- [ ] Verify charging starts within 5 seconds
- [ ] Monitor for 2 minutes:
  - Battery current increases
  - Battery voltage increases gradually
  - MPPT temperature remains stable
  - Cell balance remains good (<50mV difference)
  - No alarms triggered

**If charging does NOT start:**
- [ ] Check solar voltage is >battery voltage + 5V
- [ ] Check MPPT input polarity
- [ ] Check battery fuse is not blown
- [ ] Check BMS is not blocking (no alarms)
- [ ] Review MPPT logs for errors

**Checkpoint:**
- ✅ MPPT 1 charging successfully
- ✅ Battery voltage increasing
- ✅ No alarms

### 4.4 Increase Current Limit

- [ ] Increase current to 75% of max (e.g., 7.5A)
- [ ] Wait 2 minutes and monitor
- [ ] Increase current to 100% of max (e.g., 10A)
- [ ] Monitor for 5 minutes:
  - MPPT temperature <40°C
  - Battery temperature <35°C (LFP) or <30°C (lead-acid)
  - No oscillations or hunting
  - Smooth power delivery

**Checkpoint:**
- ✅ MPPT 1 at full current
- ✅ System stable

### 4.5 Enable Additional MPPTs

**Repeat for each additional MPPT:**

- [ ] Connect solar to MPPT 2
- [ ] Verify solar voltage detected
- [ ] Enable charging on MPPT 2
- [ ] Set same parameters as MPPT 1
- [ ] Monitor both MPPTs for current sharing (<10% imbalance)
- [ ] Repeat for MPPT 3, 4, etc.

**Checkpoint:**
- ✅ All MPPTs charging
- ✅ Current sharing is balanced
- ✅ Total current within battery limits

---

## Step 5: System Behavior Validation

**Objective:** Verify system responds correctly to various conditions.

### 5.1 BMS Over-Voltage Protection Test

⚠️ **Use EXTREME caution** - this test intentionally triggers an alarm

- [ ] Note current charge state (voltage, current, SOC)
- [ ] Lower BMS over-voltage threshold temporarily:
  - Go to diyBMS → Settings → Rules
  - Set "Cell Over Voltage" to current voltage + 0.1V
  - Save
- [ ] Wait for voltage to trigger alarm (<5 minutes typically)
- [ ] **VERIFY ALL MPPTS STOP CHARGING IMMEDIATELY**
- [ ] Check web UI shows "Charging Disabled by BMS"
- [ ] Check MPPTs show charge state = "Idle" or "Stopped"
- [ ] Restore normal over-voltage threshold
- [ ] Clear alarm (if needed)
- [ ] Verify charging resumes automatically

**Checkpoint:**
- ✅ BMS protection works
- ✅ All MPPTs respond immediately
- ✅ System recovers automatically

### 5.2 MPPT Timeout Test

- [ ] Disconnect diyBMS CAN wire from bus
- [ ] Wait 10 seconds for timeout
- [ ] Verify MPPTs continue operating safely (local control)
- [ ] Check MPPT logs show timeout message
- [ ] Reconnect diyBMS CAN
- [ ] Wait 30 seconds for rediscovery
- [ ] Verify BMS control resumes

**Checkpoint:**
- ✅ Timeout protection works
- ✅ Safe fallback behavior
- ✅ Recovery automatic

### 5.3 Individual MPPT Control Test

- [ ] Disable MPPT 1 only
- [ ] Verify MPPT 1 stops charging
- [ ] Verify other MPPTs continue charging
- [ ] Re-enable MPPT 1
- [ ] Verify MPPT 1 resumes charging

**Checkpoint:**
- ✅ Independent control works
- ✅ No cross-interference

---

## Step 6: Data Logging & Monitoring

**Objective:** Set up long-term monitoring for ongoing operation.

### 6.1 Enable Data Logging

- [ ] Configure InfluxDB (if available):
  - diyBMS → Settings → Influx DB
  - Enter server URL, token, bucket
  - Enable logging
  - Save
- [ ] Configure MQTT (if available):
  - diyBMS → Settings → MQTT
  - Enter broker address
  - Enable MQTT
  - Save
- [ ] Test data flow to logging system

### 6.2 Set Up Alerts

- [ ] Configure email alerts (if available)
- [ ] Set alert thresholds:
  - Battery over-voltage
  - Battery under-voltage
  - Over-temperature
  - CAN communication failure
  - MPPT timeout
- [ ] Test alert delivery (trigger test alarm)

### 6.3 Create Monitoring Dashboard

**Recommended metrics to display:**
- Total solar power (W)
- Battery voltage (V)
- Battery current (A)
- State of charge (%)
- Cell voltage min/max (mV)
- MPPT temperatures (°C)
- Energy today (kWh)
- Energy total (kWh)
- CAN bus status
- Alarms/warnings

**Checkpoint:**
- ✅ Data logging operational
- ✅ Alerts configured
- ✅ Dashboard created

---

## Step 7: 24-Hour Observation Period

**Objective:** Validate system stability over a full charge/discharge cycle.

### 7.1 Daytime Monitoring (0800-1800)

**Check every 2 hours:**
- [ ] 0800: System status, energy today = 0
- [ ] 1000: Charging active, power ramping up
- [ ] 1200: Peak power, all MPPTs at max
- [ ] 1400: Power decreasing, still charging
- [ ] 1600: Low power, absorption mode possible
- [ ] 1800: Minimal power, charging stopping

**Log for each check:**
```
Time:          ____
Solar Power:   ____ W
Battery V:     ____ V
Battery I:     ____ A
SOC:           ____ %
Temp (BMS):    ____ °C
Temp (MPPT):   ____ °C
Alarms:        None / [List if any]
Notes:         ____________________
```

### 7.2 Nighttime Monitoring (1800-0800)

- [ ] 1800: Solar disconnected, MPPTs idle
- [ ] 2200: Battery voltage stable, no phantom loads
- [ ] 0600: Pre-dawn check, system ready for sunrise

**Checkpoint:**
- ✅ Full day of operation
- ✅ No unexpected alarms
- ✅ Battery charged fully
- ✅ System stable overnight

---

## Step 8: Performance Validation

**Objective:** Verify system meets performance expectations.

### 8.1 Efficiency Check

**At solar noon (peak sun):**
- [ ] Measure total solar power (sum of all MPPTs)
- [ ] Measure battery power (voltage × current)
- [ ] Calculate efficiency: (battery power ÷ solar power) × 100%
- [ ] Target: ≥95%

**If efficiency is low:**
- Check for hot spots (IR camera)
- Verify MPPT temperature <40°C
- Check for voltage drops in wiring
- Verify battery is accepting charge (not limiting)

### 8.2 Energy Accounting

- [ ] Record "Energy Today" from web UI at sunset
- [ ] Compare with expected (solar capacity × sun hours × 0.8)
- [ ] Should be within ±20% of expected

**Example:**
```
Solar array: 1000W
Sun hours: 5 hours
Expected: 1000W × 5h × 0.8 = 4.0 kWh
Actual: ____ kWh
Difference: ____ %
```

### 8.3 Response Time Validation

- [ ] Simulate cloud (partially cover panels)
- [ ] Time power reduction: Should be <5 seconds
- [ ] Uncover panels
- [ ] Time power recovery: Should be <10 seconds
- [ ] No oscillations during transitions

**Checkpoint:**
- ✅ Efficiency target met
- ✅ Energy harvest as expected
- ✅ Fast response to changes

---

## Step 9: Documentation & Handover

**Objective:** Complete commissioning documentation for ongoing operation.

### 9.1 As-Built Documentation

Create a commissioning report including:

- [ ] System overview diagram (single-line drawing)
- [ ] CAN bus topology (node IDs, wiring)
- [ ] Battery specifications (make, model, capacity, chemistry)
- [ ] Solar array specifications (panels, wiring, Voc, Isc)
- [ ] MPPT specifications (make, model, ratings)
- [ ] Configuration settings (all parameters documented)
- [ ] Test results (all checklists, measurements)
- [ ] Photos (wiring, labels, equipment)

### 9.2 Operations Manual

Create user guide including:

- [ ] Normal operation (what to expect daily)
- [ ] Web UI walkthrough (screenshots, explanations)
- [ ] Common alarms (what they mean, how to clear)
- [ ] Troubleshooting guide (step-by-step diagnostics)
- [ ] Maintenance schedule (quarterly, annual checks)
- [ ] Emergency procedures (fire, electrical fault, etc.)
- [ ] Contact information (installer, manufacturer support)

### 9.3 Training

- [ ] Train end user on web UI navigation
- [ ] Explain normal vs. abnormal behavior
- [ ] Demonstrate emergency stop procedure
- [ ] Review maintenance requirements
- [ ] Provide contact information for support

### 9.4 Sign-Off

**Commissioning Completion Certificate:**

```
System Name: _________________________________
Location: ____________________________________
Date: ________________________________________

System Components:
- diyBMS: Version ______
- MPPT(s): Qty ____, Model ________________
- Battery: Capacity ____Ah, Voltage ____V
- Solar: Power ____W, Voltage ____V

Commissioning Engineer: _______________________
Signature: ___________________________________

End User Acceptance: __________________________
Signature: ___________________________________

System operational and ready for service: YES / NO
```

**Checkpoint:**
- ✅ Documentation complete
- ✅ Training provided
- ✅ Sign-off obtained

---

## Step 10: Post-Commissioning Follow-Up

**Objective:** Ensure long-term reliability.

### 10.1 One-Week Follow-Up

- [ ] Remote check-in (phone or web access)
- [ ] Review logged data for past week
- [ ] Check for any alarms or anomalies
- [ ] Answer user questions
- [ ] Adjust settings if needed

### 10.2 One-Month Follow-Up

- [ ] On-site visit (recommended) or remote check
- [ ] Download logs for analysis
- [ ] Check physical condition:
  - Connectors tight
  - No corrosion
  - Labels intact
  - No overheating signs
- [ ] Verify performance meets expectations
- [ ] User satisfaction check

### 10.3 Annual Maintenance

Create schedule for:
- [ ] Visual inspection (wiring, connections)
- [ ] CAN bus health check
- [ ] Battery capacity test
- [ ] MPPT calibration check
- [ ] Firmware updates (if available)
- [ ] Configuration backup

**Checkpoint:**
- ✅ Follow-up complete
- ✅ System performing well
- ✅ User satisfied

---

## Emergency Procedures

### Emergency Stop Procedure

1. Open diyBMS web UI on phone/laptop
2. Navigate to MPPT Control page
3. Click "Disable Charging" on ALL MPPTs
4. Verify charging stops (current drops to 0A)
5. If web UI unavailable:
   - Disconnect solar panel MC4 connectors
   - OR open solar breaker/switch
6. Call for support if fire, smoke, or electrical smell

### Alarm Response Quick Guide

| Alarm | Immediate Action | Follow-Up |
|-------|-----------------|-----------|
| **Over-Voltage** | Charging auto-stops | Check if cell balance needed |
| **Under-Voltage** | Load auto-disconnects | Check battery health |
| **Over-Temperature** | Charging auto-stops | Check ventilation, battery |
| **CAN Timeout** | MPPTs revert to local | Check CAN wiring |
| **MPPT Offline** | Other MPPTs continue | Check power, CAN connection |

**For all alarms:** Document the event, check logs, and call installer if alarm persists.

---

## Commissioning Success! 🎉

**Congratulations!** The diyBMS + MPPT CAN system is now fully commissioned and operational.

**Key Achievements:**
- ✅ All hardware connected and operational
- ✅ CAN communication verified
- ✅ Charge parameters optimized
- ✅ Safety systems tested and working
- ✅ 24-hour stability validated
- ✅ Performance targets met
- ✅ Documentation complete
- ✅ User trained

**System is ready for reliable, long-term operation with coordinated solar charging managed by the BMS master controller.**

---

**End of Document**