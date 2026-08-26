# Home Climate System — Wiki

Home Climate System (HCS) is the **ESP firmware + OpenTherm hardware** that acts as a boiler gateway for [Home Climate Control](https://github.com/ALeXXBody/home-climate-control).

| | |
|---|---|
| **Repo** | [github.com/ALeXXBody/home-climate-system](https://github.com/ALeXXBody/home-climate-system) |
| **Current firmware** | **v1.4.0** |
| **HA software** | [Home Climate Control](https://github.com/ALeXXBody/home-climate-control) — [HCC Wiki](https://github.com/ALeXXBody/home-climate-control/blob/main/docs/wiki/Home.md) |
| **License** | MIT |

## What it does

The HCS board is the **boiler gateway only** — not a room thermostat. It:

- Speaks **OpenTherm** to the boiler (master protocol)
- Communicates with Home Assistant via **MQTT**
- Runs a **captive portal** for WiFi/MQTT setup
- Exposes a **web UI** for local control
- Supports **OTA firmware updates** with rollback protection
- Detects and manages **1-Wire temperature probes**
- Provides **connection-loss failsafe** protection

### What it does NOT do

- It does not sense room temperature (that's HA's job via room sensors)
- It does not control TRVs (that's HA's job via Zigbee/etc.)
- It does not store schedules or presets (that's HCC's job)

## Pages

| Page | What's in it |
|---|---|
| [Hardware](Hardware.md) | Supported boards, wiring, OT shield, 1-Wire sensors |
| [Flash & first boot](Flash-and-first-boot.md) | PlatformIO, web flasher, captive portal setup |
| [Web UI](Web-UI.md) | All tabs: Status, Controls, Gateway, Sensors, Settings, System |
| [MQTT protocol](MQTT-protocol.md) | Full topic reference for firmware ↔ HA |
| [Weather compensation](Weather-compensation.md) | Curve math, configuration, NVS persistence |
| [1-Wire sensors](1-Wire-sensors.md) | Probe detection, roles, health, custom sensors |
| [Failsafe](Failsafe.md) | Connection-loss protection, configuration |
| [Gateway mode](Gateway-mode.md) | Dual-OT topology for existing wall thermostats |
| [OTA updates](OTA.md) | HTTP OTA, rollback, from HCC panel |
| [Boards & builds](Boards-and-builds.md) | Build matrix, pin maps, board selection |
| [Troubleshooting](Troubleshooting.md) | Common issues and fixes |
| [Changelog](Changelog.md) | Release history |

## Requirements

- DIYLess Master OpenTherm Shield (or equivalent)
- ESP8266 or ESP32 board (see [Boards & builds](Boards-and-builds.md))
- MQTT broker (accessible from both board and HA)
- Home Climate Control integration in Home Assistant
