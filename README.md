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
- **ESP32-S3-Zero** → jumper wires to the shield

See [docs/hardware.md](docs/hardware.md).

## Firmware status (v0.1.0)

PlatformIO project under `firmware/`:

- OpenTherm master via **ihormelnyk/opentherm_library** (MIT)
- WiFi + MQTT (PubSubClient)
- Publishes **native `hcs/`** topics and **OTGW-compat** topics for HA
- Accepts CH enable, flow setpoint, max modulation from MQTT
- CH **off at boot** (failsafe)

Not yet: captive portal / BLE provisioning, OTA UI in HA sidebar, slave/gateway mode.

## Quick start

```bash
cd firmware
cp include/secrets.example.h include/secrets.h   # edit WiFi + MQTT
pio run -e d1_mini -t upload                     # or esp32s3_zero
pio device monitor -b 115200
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
