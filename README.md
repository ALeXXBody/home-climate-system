# Home Climate System

**Hardware + firmware** companion for
[Home Climate Control](https://github.com/ALeXXBody/home-climate-control)
(the Home Assistant software).

| Product | Repo | Role |
|---|---|---|
| **Home Climate Control** | [home-climate-control](https://github.com/ALeXXBody/home-climate-control) | HA custom integration (software) |
| **Home Climate System** | this repo | ESP32 / ESP8266 firmware & hardware |

Private during development. Planned public license: **MIT**.

## What this repo will contain

- `firmware/` — ESP-IDF / Arduino or PlatformIO project for ESP32 and ESP8266
- `hardware/` — board notes, pinouts, BOM (later)
- `protocol/` — MQTT topic contract shared with Home Climate Control
- `docs/` — design notes, license research

## Goals (firmware)

1. Talk to the boiler over **OpenTherm** (adapter / gateway role).
2. Publish telemetry and accept commands over **MQTT** so Home Climate Control
   (and HA) can drive weather-compensated, gas-minimal heating.
3. Support **ESP32** and **ESP8266** targets.
4. Stay **MIT-clean**: no GPL code in the tree.

## License wall (critical)

| Project | License | Can we copy code into this MIT repo? |
|---|---|---|
| [rvdbreemen/OTGW-firmware](https://github.com/rvdbreemen/OTGW-firmware) | **GPL-3.0** | **NO** |
| [Alexwijn/SAT](https://github.com/Alexwijn/SAT) | GPL-3.0 | **NO** |
| OpenTherm protocol itself | specification | yes (implement from docs) |
| ESPHome `opentherm` component | check per-file | only if license is MIT/Apache-compatible |

**OTGW-firmware is inspiration only:** MQTT topic *ideas*, OpenTherm message
IDs, UX patterns. All firmware here must be written clean-room. Linking or
vendoring GPL sources would force this entire product under GPL and break the
MIT plan for Home Climate Control + Home Climate System.

See [docs/license-otgw.md](docs/license-otgw.md).

## Status

Scaffold only (v0.0.1). No flashable firmware yet.

## Support

If this project helps you, you can support development here:

[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20a%20Coffee-ffdd00?style=for-the-badge&logo=buy-me-a-coffee&logoColor=black)](https://buymeacoffee.com/alexxbody)

https://buymeacoffee.com/alexxbody

## Related

- Software: https://github.com/ALeXXBody/home-climate-control  
- Inspiration (do not copy): https://github.com/rvdbreemen/OTGW-firmware  
