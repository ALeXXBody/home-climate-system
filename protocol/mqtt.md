# MQTT protocol — Home Climate System

Shared contract between **firmware** and **Home Climate Control**.

## Identity

- Node id: `hcs-<mac>` (no colons, lowercase), printed on serial at boot.
- Native prefix: `hcs` (configurable `MQTT_PREFIX` / portal).
- OTGW-compat prefix: `OTGW` / node `hcs-device` (configurable `otgw_node`).

## Discovery (device → broker)

Retained JSON on `hcs/discovery/<node_id>`:

```json
{
  "node_id": "hcs-aabbccddeeff",
  "name": "Home Climate System",
  "board": "lolin_s2_mini",
  "version": "0.6.0",
  "ip": "192.168.x.y",
  "ota_http": "http://192.168.x.y/update",
  "api_status": "http://192.168.x.y/api/status",
  "api_ota": "http://192.168.x.y/api/ota"
}
```

Also retained: `hcs/<node>/ip`, `…/board`, `…/version`.

Ping all devices: publish any payload to `hcs/discovery/ping` → each node
re-publishes discovery.

## Native topics (`hcs/<node>/…`)

### Telemetry (device → broker)

| Topic | Payload |
|---|---|
| `…/online` | `online` / `offline` (LWT retained) |
| `…/version` | firmware version string |
| `…/board` | board env name |
| `…/ip` | device IP |
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
| `…/weather_comp` | `on` / `off` (weather compensation active) |
| `…/wc_target` | effective curve target °C (when WC active + outdoor known) |

### Commands (broker → device)

| Topic | Payload |
|---|---|
| `…/set/ch_enable` | `on` / `off` / `1` / `0` / `true` / `false` |
| `…/set/dhw_enable` | same |
| `…/set/flow_setpoint` | float °C |
| `…/set/max_modulation` | 0–100 (clamped) |
| `…/set/weather_comp` | `on` / `off` |
| `…/set/weather_comp_cfg` | `<ref>,<design>,<fmax>,<fmin>` e.g. `18,-10,65,25` |
| `…/set/reboot` | any |
| `…/set/ota_url` | `http(s)://…/firmware.bin` |

**Weather compensation:** when enabled, the flow setpoint sent to the boiler
follows a linear heating curve between `flow_min` at outdoor `ref` °C and
`flow_max` at design outdoor `design` °C. Outdoor temp comes from the boiler
(MsgID 27); without a valid reading the manual setpoint is used. WC state and
curve config persist in NVS/EEPROM across reboots.

## Gateway topics (ESP32 `*_gw` builds only)

| Topic | Dir | Payload |
|---|---|---|
| `…/boiler_diag` | pub | plain-English boiler fault summary (retained; e.g. `low water pressure`) |
| `…/boiler_state` | pub | `ok` \| `fault` \| `unknown` (retained) |
| `…/gw/mode` | pub | effective role: `master_only` \| `gateway` (retained) |
| `…/gw/set_mode` | sub | `auto` \| `gateway` \| `master_only` → saves + reboots |
| `…/gw/tstat_online` | pub | `ON` / `OFF` (thermostat bus activity) |
| `…/gw/override_setpoint` | pub | forced °C or empty = pass-through |
| `…/gw/override_setpoint` | sub | float °C to force, `off`/`auto`/`release` to release |

Discovery JSON includes a `gw` block with traffic counters on gateway builds.
Web UI: Gateway tab (mode switch, live counters, setpoint override).

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
| `OTGW/set/<otgw_node>/chenable` | `on`/`off` |
| `OTGW/set/<otgw_node>/ctrlsetpt` | float °C |
| `OTGW/set/<otgw_node>/maxmodulation` | 0–100 |

Default `otgw_node` is `hcs-device` (portal / NVS).
