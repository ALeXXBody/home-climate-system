<p align="center">
  <a href="https://buymeacoffee.com/ALeXXBody" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/default-orange.png" alt="Buy Me A Coffee" height="41" width="174"></a>
</p>

# Home Climate System

[![CI](https://github.com/ALeXXBody/home-climate-system/actions/workflows/ci.yml/badge.svg)](https://github.com/ALeXXBody/home-climate-system/actions/workflows/ci.yml)

**Hardware + firmware** for [Home Climate Control](https://github.com/ALeXXBody/home-climate-control).

**Docs wiki:** [Home · Hardware · Flash · MQTT · Failsafe · Boards](docs/wiki/Home.md)  
*(GitHub Wiki flag is on; seed pages from `docs/wiki/` via repo → Wiki if you want the `/wiki` URL.)*

| Product | Repo | Role |
|---|---|---|
| **Home Climate Control** | [home-climate-control](https://github.com/ALeXXBody/home-climate-control) | HA integration + sidebar app |
| **Home Climate System** | this repo | ESP firmware + DIYLess OpenTherm hardware |

| | |
|---|---|
| **Current firmware** | **v1.4.7** |
| **License** | MIT |

## Supported hardware

- **DIYLess Master OpenTherm Shield** → boiler OT bus (master/thermostat)
- **ESP8266 D1 mini** · **LOLIN S2 mini** · **LOLIN C3 mini v2.1** · **ESP32 D1 mini** · **ESP32-S3-Zero**
- Gateway builds (`*_gw`) for selected boards

See [docs/hardware.md](docs/hardware.md) and the [Wiki — Hardware](https://github.com/ALeXXBody/home-climate-system/wiki/Hardware).

## Firmware (v1.4.7)

PlatformIO project under `firmware/`.

### Core

- OpenTherm master ([ihormelnyk/opentherm_library](https://github.com/ihormelnyk/opentherm_library), MIT)
- Weather compensation curve (outdoor → flow); **ESP32 NVS persistence** for WC keys
- CH enable, flow setpoint, max modulation, DHW enable + setpoint, reboot
- CH **off at boot** (failsafe)
- Captive portal (WiFiManager) + NVS; unique hostname / setup AP `HCS-Setup-XXXX`
- Device web UI + ElegantOTA + ArduinoOTA
- WiFi + MQTT (PubSubClient); native **`hcs/<node>`** contract only

### Reliability & management

- OTA progress over MQTT; scheme-aware HTTP OTA (LAN mirror)
- **LittleFS-backed OTA rollback** (confirm / revert after flash)
- Auto-detect 1-Wire probes + custom roles
- Two-way settings sync (`ctl` snapshot retained on connect)
- OT snapshot guards (ignore invalid 0 °C on failed frames)
- 1-Wire inject re-applied after return-temp read
- Authenticated `/api/reboot`; HTTP DHW setpoint on `/api/control`
- referencePoll includes DHW enable bit

### Build targets

`d1_mini`, `lolin_s2_mini`, `lolin_c3_mini`, `esp32_d1_mini`, `esp32s3_zero`  
(+ `lolin_s2_mini_gw`, `esp32_d1_mini_gw`, `lolin_c3_mini_gw`)

Release assets: [v1.4.7](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.4.7)  
(`firmware-<env>.bin` for each board)

## Quick start

```bash
cd firmware
pio run -e lolin_s2_mini -t upload   # or d1_mini / lolin_c3_mini / …
pio device monitor -b 115200
# Join AP HCS-Setup-XXXX → set WiFi + MQTT broker
```

Prefer OTA from **Home Climate Control → Devices** after the catalog shows 1.4.0.

Full steps: [docs/flash.md](docs/flash.md) · [Wiki — Flash](docs/wiki/Flash-and-first-boot.md)

## MQTT (summary)

- Discovery: `hcs/discovery/<node_id>` (retained JSON)
- Telemetry: `hcs/<node>/outdoor_temp`, `flow_temp`, `return_temp`, `modulation`, …
- Commands: `hcs/<node>/set/ch_enable`, `…/flow_setpoint`, `…/weather_comp_cfg` (CSV), …

Full contract: [protocol/mqtt.md](protocol/mqtt.md) · [Wiki — MQTT](docs/wiki/MQTT-protocol.md)

## Docs

- **[Docs wiki](docs/wiki/Home.md)** — [Hardware](docs/wiki/Hardware.md) · [Flash](docs/wiki/Flash-and-first-boot.md) · [Web UI](docs/wiki/Web-UI.md) · [MQTT](docs/wiki/MQTT-protocol.md) · [WC](docs/wiki/Weather-compensation.md) · [1-Wire](docs/wiki/1-Wire-sensors.md) · [Failsafe](docs/wiki/Failsafe.md) · [Gateway](docs/wiki/Gateway-mode.md) · [OTA](docs/wiki/OTA.md) · [Boards](docs/wiki/Boards-and-builds.md) · [Troubleshooting](docs/wiki/Troubleshooting.md) · [Changelog](docs/wiki/Changelog.md)  
- [Hardware detail](docs/hardware.md) · [Flash detail](docs/flash.md) · [Architecture](docs/architecture.md)  
- [Failsafe detail](docs/failsafe.md) · [Gateway design](docs/design-gateway.md)  
- [MQTT protocol](protocol/mqtt.md) · [OTGW license notes](docs/license-otgw.md)  
- HA software: [home-climate-control](https://github.com/ALeXXBody/home-climate-control) · [HCC docs wiki](https://github.com/ALeXXBody/home-climate-control/blob/main/docs/wiki/Home.md)

## Support

[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20a%20Coffee-ffdd00?style=for-the-badge&logo=buy-me-a-coffee&logoColor=black)](https://buymeacoffee.com/alexxbody)

## License

MIT — see [`LICENSE`](LICENSE).
