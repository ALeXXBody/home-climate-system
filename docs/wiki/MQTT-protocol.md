# MQTT protocol

Canonical doc: [protocol/mqtt.md](https://github.com/ALeXXBody/home-climate-system/blob/main/protocol/mqtt.md)

## Identity

- Node: `hcs-<mac>` (lowercase, no colons)
- Prefix: `hcs` (configurable)

## Discovery

Retained JSON: `hcs/discovery/<node_id>`  
Ping all: publish to `hcs/discovery/ping`

## Telemetry (examples)

`hcs/<node>/outdoor_temp`, `flow_temp`, `return_temp`, `modulation`, `flame`, `ch_active`, `version`, `online`

## Commands

`hcs/<node>/set/ch_enable` · `flow_setpoint` · `dhw_enable` · `dhw_setpoint` · `max_modulation` · `weather_comp` · `weather_comp_cfg` (**CSV** `ref,design,fmax,fmin`)

HCC does **not** retain CH/flow commands (safety).
