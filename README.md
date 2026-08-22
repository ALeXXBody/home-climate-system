# Home Climate System

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
- **ESP32-S3-Zero** → extra target; jumper wires to the shield

See [docs/hardware.md](docs/hardware.md).

## Firmware status (v0.2.0)

PlatformIO project under `firmware/`:

- OpenTherm master via **ihormelnyk/opentherm_library** (MIT)
- Captive portal (WiFiManager) + NVS settings
- Device web UI (status / controls / settings) + ElegantOTA + ArduinoOTA
- WiFi + MQTT (PubSubClient); native **`hcs/`** + **OTGW-compat** for HA
- CH enable, flow setpoint, max modulation, DHW, reboot, OTA URL
- Weather compensation: linear heating curve from boiler outdoor sensor
- CH **off at boot** (failsafe)
- Targets: `d1_mini`, `lolin_s2_mini`, `esp32_d1_mini`, `esp32s3_zero`

Not yet: HA sidebar OTA push end-to-end, slave/gateway mode, WC config persistence.

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
- [MQTT protocol](protocol/mqtt.md)
- [OTGW-firmware license wall](docs/license-otgw.md)

## Support

[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20a%20Coffee-ffdd00?style=for-the-badge&logo=buy-me-a-coffee&logoColor=black)](https://buymeacoffee.com/alexxbody)

https://buymeacoffee.com/alexxbody
