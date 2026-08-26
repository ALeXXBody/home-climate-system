# Home Climate System Wiki

**ESP firmware + OpenTherm hardware** for [Home Climate Control](https://github.com/ALeXXBody/home-climate-control).

| | |
|---|---|
| Repo | https://github.com/ALeXXBody/home-climate-system |
| Firmware | **v1.4.0** |
| HA software | [HCC](https://github.com/ALeXXBody/home-climate-control) · [HCC docs](https://github.com/ALeXXBody/home-climate-control/blob/main/docs/wiki/Home.md) |

## Pages

- [Hardware](Hardware.md)
- [Flash and first boot](Flash-and-first-boot.md)
- [MQTT protocol](MQTT-protocol.md)
- [Failsafe](Failsafe.md)
- [Boards and builds](Boards-and-builds.md)
- [Changelog highlights](Changelog.md)

## Role

The HCS board is the **boiler gateway only** — not a room thermostat. It speaks OpenTherm to the boiler and MQTT to Home Assistant (HCC).
