# MQTT protocol (draft) — Home Climate System

Shared contract between **Home Climate System** firmware and
**Home Climate Control** software.

Prefix default: `hcs` (configurable).  
Node id: unique device id, e.g. `hcs-AABBCCDDEEFF`.

## Telemetry (device publishes)

| Topic | Payload | Unit / notes |
|---|---|---|
| `{prefix}/{node}/outdoor_temp` | float | °C |
| `{prefix}/{node}/flow_temp` | float | °C |
| `{prefix}/{node}/return_temp` | float | °C |
| `{prefix}/{node}/modulation` | float | % |
| `{prefix}/{node}/flame` | `ON` / `OFF` | |
| `{prefix}/{node}/ch_active` | `ON` / `OFF` | |
| `{prefix}/{node}/fault` | `ON` / `OFF` | |
| `{prefix}/{node}/online` | `online` / `offline` | LWT recommended |

## Commands (device subscribes)

| Topic | Payload | Meaning |
|---|---|---|
| `{prefix}/{node}/set/ch_enable` | `on` / `off` | Central heating enable |
| `{prefix}/{node}/set/flow_setpoint` | float °C | Control setpoint (0.5 °C steps) |
| `{prefix}/{node}/set/max_modulation` | 0–100 | Max relative modulation |

## Compatibility mode (optional)

To work with the existing Home Climate Control OTGW backend without a new
driver, firmware may *also* publish/subscribe OTGW-firmware-compatible names
under a separate prefix (e.g. `OTGW/...`). That is a **behavioural** shim only;
implementation must remain original (no OTGW-firmware source).

## Versioning

Breaking topic changes bump `protocol_version` (publish
`{prefix}/{node}/protocol_version` as an integer).
