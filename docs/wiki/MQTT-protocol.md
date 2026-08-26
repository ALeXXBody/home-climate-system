# MQTT protocol

Full contract between the HCS firmware and Home Climate Control (HA).

## Identity

| | |
|---|---|
| Node ID | `hcs-<mac>` (lowercase, no colons) |
| Default prefix | `hcs` (configurable via portal / `MQTT_PREFIX`) |
| Example node | `hcs-aabbccddeeff` |

Serial at boot prints: `node_id: hcs-aabbccddeeff`

## Discovery (retained)

Every 30 seconds, the board publishes:

| Topic | Payload |
|---|---|
| `hcs/discovery/<node_id>` | JSON: `node_id`, `name`, `board`, `version`, `ip`, `ota_http`, `api_status`, `api_ota` |
| `hcs/<node>/ip` | IP address (retained) |
| `hcs/<node>/board` | Board env name (retained) |
| `hcs/<node>/version` | Firmware version (retained) |

**Ping all devices:** publish any payload to `hcs/discovery/ping` → each board re-publishes discovery.

Gateway builds include a `gw` block with traffic counters.

## Telemetry (device → broker, every 2 s)

### State

| Topic | Payload | Notes |
|---|---|---|
| `…/online` | `online` / `offline` | LWT retained |
| `…/version` | `1.4.0` | Firmware version |
| `…/board` | `lolin_s2_mini` | Board env |
| `…/ip` | `192.168.50.137` | Device IP |

### Boiler data

| Topic | Payload | Notes |
|---|---|---|
| `…/outdoor_temp` | `12.5` | °C (OT or 1-Wire outdoor) |
| `…/flow_temp` | `45.0` | °C (OT MsgID 1) |
| `…/return_temp` | `38.2` | °C (OT MsgID 25) |
| `…/modulation` | `65` | % (OT MsgID 17) |
| `…/flame` | `ON` / `OFF` | OT MsgID 5 |
| `…/ch_active` | `ON` / `OFF` | OT MsgID 0 |
| `…/fault` | `ON` / `OFF` | Any ASF bit set |
| `…/ch_pressure` | `1.2` | bar (OT MsgID 18) |

### Control state

| Topic | Payload | Notes |
|---|---|---|
| `…/cmd_ch` | `on` / `off` | Last commanded CH |
| `…/cmd_flow_setpoint` | `45.0` | Last commanded flow °C |
| `…/cmd_max_modulation` | `100` | Last commanded max mod % |

### Weather compensation

| Topic | Payload | Notes |
|---|---|---|
| `…/weather_comp` | `on` / `off` | WC enabled |
| `…/wc_target` | `48.5` | Current curve target °C |

### DHW

| Topic | Payload | Notes |
|---|---|---|
| `…/dhw_setpoint` | `55` | Retained, °C |

### Diagnostics

| Topic | Payload | Notes |
|---|---|---|
| `…/boiler_diag` | `OK — no faults` | Retained, human text |
| `…/boiler_state` | `ok` / `fault` / `unknown` | Retained |
| `…/boiler_identity` | `{...}` | Retained JSON: member IDs, config, capacity |
| `…/boiler_member` | `1` | Retained, slave member ID |
| `…/fault_history` | `01 AB 05` | Retained, hex entries |
| `…/failsafe` | `OFF` / `HOLD` / `ON` | Retained |
| `…/ctl` | `{...}` | Retained JSON: full control-state snapshot |

### Sensors

| Topic | Payload | Notes |
|---|---|---|
| `…/sensors` | `{...}` | Retained JSON: probe list with addr, temp, health, role |
| `…/x/<name>` | `21.3` | Retained, custom sensor temperature |

### OTA progress

| Topic | Payload | Notes |
|---|---|---|
| `…/ota` | `{"state":"downloading","progress":45}` | Temporary during OTA |

### Error

| Topic | Payload |
|---|---|
| `…/wc_error` | Bad `weather_comp_cfg` payload description |

## Commands (broker → device)

All under `hcs/<node>/set/`:

| Topic | Payload | Notes |
|---|---|---|
| `…/ch_enable` | `on`/`off`/`1`/`0`/`true`/`false` | CH on/off |
| `…/dhw_enable` | same | DHW on/off |
| `…/flow_setpoint` | `45.0` | °C (clamped to boiler bounds) |
| `…/dhw_setpoint` | `55` or `off`/`auto` | °C or release |
| `…/max_modulation` | `100` | 0–100 (clamped) |
| `…/weather_comp` | `on`/`off` | Enable/disable WC |
| `…/weather_comp_cfg` | `18,-10,65,25` | CSV: ref,design,fmax,fmin |
| `…/failsafe_cfg` | `{"enable":true,"flow":40,"grace_min":10}` | JSON config |
| `…/reboot` | any payload | Reboots device |
| `…/ota_url` | `http://...` | Triggers HTTP OTA |
| `…/settings` | `{...}` | Partial JSON settings update |

### Gateway commands (gw builds only)

| Topic | Payload | Notes |
|---|---|---|
| `…/gw/set_mode` | `auto` / `gateway` / `master_only` | Saves + reboots |
| `…/gw/override_setpoint` | `45.0` or `off`/`auto`/`release` | Force or release flow temp |

## Notes

- **CH commands are NOT retained** (safety — board reverts to failsafe/defaults on restart)
- **DHW setpoint** is retained (persistent setting)
- **Telemetry** cadence is 2 seconds; change-gated where noted
- **Discovery** cadence is 30 seconds
- **Command watchdog:** 5 minutes without any command → optional safety behaviour
