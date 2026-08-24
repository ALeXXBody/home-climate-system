# Home Climate System

[![CI](https://github.com/ALeXXBody/home-climate-system/actions/workflows/ci.yml/badge.svg)](https://github.com/ALeXXBody/home-climate-system/actions/workflows/ci.yml)

**Hardware + firmware** for [Home Climate Control](https://github.com/ALeXXBody/home-climate-control).

| Product | Repo | Role |
|---|---|---|
| **Home Climate Control** | [home-climate-control](https://github.com/ALeXXBody/home-climate-control) | HA integration + sidebar app |
| **Home Climate System** | this repo | ESP firmware + DIYLess OT hardware |

License: **MIT**.

## Your hardware (supported now)

- **DIYLess Master OpenTherm Shield** → boiler OT bus (master/thermostat)
- **ESP8266 D1 mini** → stacks on the shield
- **LOLIN / Wemos S2 mini** → D1-mini layout, same OT pins (GPIO4/5)
- **LOLIN C3 mini v2.1** → direct fitment, stacks on the shield (GPIO7/6)
- **ESP32-S3-Zero** → extra target; jumper wires to the shield

See [docs/hardware.md](docs/hardware.md).

## Firmware status (v1.2.2)

PlatformIO project under `firmware/`:

### Core

- OpenTherm master via **ihormelnyk/opentherm_library** (MIT)
- Weather compensation: linear heating curve from boiler outdoor sensor
- CH enable, flow setpoint, max modulation, DHW control, reboot
- CH **off at boot** (failsafe)
- Captive portal (WiFiManager) + NVS settings; unique hostname and per-device
  setup AP (`HCS-Setup-XXXX`)
- Device web UI (status / controls / settings) + ElegantOTA + ArduinoOTA
- WiFi + MQTT (PubSubClient); native **`hcs/<node>`** topics are the only contract

### Remote management (v1.1 – v1.2 line)

- **OTA progress reporting** over MQTT (v1.1.1)
- **Auto-detect 1-Wire probes** with custom roles, published as MQTT state (v1.1.0)
- **Two-way settings sync** over MQTT — HA can read *and* write board settings;
  retained `ctl` snapshot on connect so the app is instantly up to date (v1.2.0 / v1.2.2)
- **Scheme-aware OTA client** — plain-HTTP pulls from LAN mirrors work (v1.2.1);
  OTA can be triggered end-to-end from the HA sidebar app with progress + success detection
- **DHW setpoint subscription** over MQTT (v1.2.2)

### Targets

`d1_mini`, `lolin_s2_mini`, `lolin_c3_mini`, `esp32_d1_mini`, `esp32s3_zero`
(+ `*_gw` gateway builds).

Not yet: slave/gateway mode.

## Quick start

```bash
cd firmware
pio run -e lolin_s2_mini -t upload   # or d1_mini / esp32s3_zero
pio device monitor -b 115200
# Join AP HCS-Setup / homeclimate → set WiFi + MQTT
```

Full steps: [docs/flash.md](docs/flash.md).

## Docs

- [Hardware wiring](docs/hardware.md)
- [Flash & MQTT test](docs/flash.md)
- [Architecture](docs/architecture.md)
- [Gateway mode design (draft)](docs/design-gateway.md)
- [MQTT protocol](protocol/mqtt.md)
- [OTGW-firmware license wall](docs/license-otgw.md)

## Support

[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20a%20Coffee-ffdd00?style=for-the-badge&logo=buy-me-a-coffee&logoColor=black)](https://buymeacoffee.com/alexxbody)

https://buymeacoffee.com/alexxbody
