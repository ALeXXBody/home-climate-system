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
  "version": "0.2.0",
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

### Commands (broker → device)

| Topic | Payload |
|---|---|
| `…/set/ch_enable` | `on` / `off` / `1` / `0` / `true` / `false` |
| `…/set/dhw_enable` | same |
| `…/set/flow_setpoint` | float °C |
| `…/set/max_modulation` | 0–100 (clamped) |
| `…/set/reboot` | any |
| `…/set/ota_url` | `http(s)://…/firmware.bin` |

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
