# MPPT CAN Bus Integration

This document describes the CAN Bus Master Functionality for MPPT (Maximum Power Point Tracker) control implemented in the diyBMSv4 ESP32 controller.

## Overview

The MPPT integration allows the diyBMS controller to act as a CAN Bus master that communicates with compatible MPPT solar charge controllers via a ThingSet-based protocol over 29-bit extended CAN frames.

## Protocol

Communication uses the [ThingSet protocol](https://thingset.io/) over CAN Bus with 29-bit extended identifiers:

- **PubSub messages** (base `0x1E000000`): Telemetry published by MPPT devices
- **ReqResp messages** (base `0x1D000000`): Commands sent by the BMS controller

MPPT device node IDs are expected in the range `0x0010`–`0x001F` (the protocol supports up to 16 node IDs in this range; the implementation tracks a maximum of `MAX_MPPT_DEVICES` = 4 devices simultaneously).

Data is CBOR-encoded. Supported object IDs:

| ID     | Description         |
|--------|---------------------|
| 0x4000 | Enable/disable charging (bool) |
| 0x4001 | Target voltage (float, V) |
| 0x4002 | Max charge current (float, A) |
| 0x6001 | Solar panel voltage (float, V) |
| 0x6002 | Solar panel current (float, A) |
| 0x6003 | Solar panel power (float, W) |
| 0x6004 | Battery voltage (float, V) |
| 0x6005 | Battery current (float, A) |
| 0x6006 | Temperature (int16, °C) |
| 0x6007 | Charge state (uint8) |
| 0x6008 | Daily energy (float, Wh) |

## Settings

Settings are stored in NVS and configurable via the web interface at `/mppt.htm`:

| Setting | Type | Scale | Default | Description |
|---------|------|-------|---------|-------------|
| `mppt_can_enabled` | bool | — | false | Enable MPPT CAN control |
| `mppt_target_voltage` | uint16 | 0.1V | 560 (56.0V) | Target charge voltage |
| `mppt_max_charge_current` | uint16 | 0.1A | 200 (20.0A) | Maximum charge current |
| `mppt_absorption_voltage` | uint16 | 0.1V | 565 (56.5V) | Absorption voltage |
| `mppt_float_voltage` | uint16 | 0.1V | 540 (54.0V) | Float voltage |
| `mppt_timeout_seconds` | uint16 | 1s | 10 | Timeout before action |
| `mppt_timeout_action` | uint8 | — | 0 | 0=stop, 1=continue, 2=local |
| `mppt_mock_mode_enabled` | bool | — | false | Enable simulated devices |
| `mppt_mock_device_count` | uint8 | — | 2 | Number of simulated devices |

## Web API

### GET `/api/mppt`
Returns current settings and live telemetry for all discovered MPPT devices as JSON.

### POST `/post/savemppt`
Save MPPT settings. Parameters (URL-encoded or JSON):
- `mpptEnabled`, `mpptMockMode`, `mpptTargetV`, `mpptMaxI`, `mpptAbsV`, `mpptFloatV`, `mpptTimeout`, `mpptTimeoutAction`, `mpptMockCount`

### POST `/post/mpptcontrol`
Send control commands to a specific MPPT device. Parameters:
- `id` (required): MPPT node ID
- `enable` (optional): `true`/`false` to enable/disable charging
- `voltage` (optional): Set target voltage (V)
- `current` (optional): Set current limit (A)

## BMS Protection Integration

When the `BankOverVoltage` rule triggers, the MPPT manager automatically sends disable-charging commands to all online MPPT devices.

## Mock Mode

When `mppt_mock_mode_enabled` is true and `MPPT_MOCK_MODE` is defined at compile time, the manager simulates the configured number of MPPT devices with realistic telemetry values. Useful for testing without hardware.

## Architecture

- `MPPTManager` class in `mppt_canbus.h`/`mppt_canbus.cpp` handles all CAN communication
- A dedicated FreeRTOS task (`mppt_can_task`, 100ms period) calls `MPPTManager::update()`
- `canbus_rx` task calls `MPPTManager::processReceivedMessage()` for all 29-bit extended ID messages
- Thread safety via a FreeRTOS mutex (`mppt_manager.mutex`)
