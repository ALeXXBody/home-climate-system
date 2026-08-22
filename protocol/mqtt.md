# MQTT protocol — Home Climate System

Shared contract between **firmware** and **Home Climate Control**.

## Identity

- Node id: `hcs-<mac>` (no colons, lowercase), printed on serial at boot.
- Native prefix: `hcs` (configurable `MQTT_PREFIX`).
- OTGW-compat prefix: `OTGW` / node `hcs-device` (configurable).

## Native topics (`hcs/<node>/…`)

### Telemetry (device → broker)

| Topic | Payload |
|---|---|
| `…/online` | `online` / `offline` (LWT retained) |
| `…/version` | firmware version string |
| `…/protocol_version` | `1` |
| `…/outdoor_temp` | °C |
| `…/flow_temp` | °C (boiler water) |
| `…/return_temp` | °C |
| `…/modulation` | % |
| `…/flame` | `ON` / `OFF` |
| `…/ch_active` | `ON` / `OFF` |
| `…/fault` | `ON` / `OFF` |
| `…/cmd_ch` | last commanded CH |
| `…/cmd_flow_setpoint` | last commanded flow °C |
| `…/cmd_max_modulation` | last commanded MM |

### Commands (broker → device)

| Topic | Payload |
|---|---|
| `…/set/ch_enable` | `on` / `off` |
| `…/set/dhw_enable` | `on` / `off` |
| `…/set/flow_setpoint` | float °C (0.5 steps) |
| `…/set/max_modulation` | 0–100 |

## OTGW-firmware compatibility

So the existing Home Climate Control **OTGW MQTT** backend works unchanged:

### Telemetry

| Topic | Same as OTGW-firmware |
|---|---|
| `OTGW/outsidetemperature` | outdoor °C |
| `OTGW/boilertemperature` | flow °C |
| `OTGW/returnwatertemperature` | return °C |
| `OTGW/relmodlvl` | modulation % |
| `OTGW/flamestatus` | `ON`/`OFF` |
| `OTGW/chmodus` | `ON`/`OFF` |
| `OTGW/controlsetpoint` | commanded flow |

### Commands

| Topic | Payload |
|---|---|
| `OTGW/set/hcs-device/chenable` | `on`/`off` |
| `OTGW/set/hcs-device/ctrlsetpt` | float °C |
| `OTGW/set/hcs-device/maxmodulation` | 0–100 |

Change `OTGW_COMPAT_NODE` if you use a different node id in HA config.
