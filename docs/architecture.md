# Home Climate System — firmware architecture (draft)

## Role

Bridge between:

```
Boiler (OpenTherm)  <->  HCS device (ESP32/ESP8266)  <->  MQTT  <->  Home Climate Control (HA)
```

Optional later: local sensors (outdoor, return temp), status LEDs, OTA.

## Targets

| MCU | Framework (TBD) | Notes |
|---|---|---|
| ESP32 | PlatformIO + Arduino or ESP-IDF | Primary |
| ESP8266 | PlatformIO + Arduino | Secondary, flash/RAM tighter |

## Modules (planned)

```
firmware/
  src/
    main.cpp           # boot, WiFi, loop
    ot_master.cpp      # OpenTherm master frame TX/RX (clean-room)
    mqtt_bridge.cpp    # publish telemetry, subscribe commands
    config.cpp         # NVS / WiFi / broker settings
    ota.cpp            # optional
  include/
    ot_protocol.h      # message IDs, frame helpers
    mqtt_topics.h      # topic map (see protocol/)
```

## Control surface (v1 intent)

**Telemetry out (device → broker):** outdoor temp, flow temp, return temp,
relative modulation, flame, CH active, faults.

**Commands in (broker → device):** CH enable, control setpoint (flow °C),
max modulation %.

Exact topic names: `protocol/mqtt.md`.

## Safety

- Never leave CH enabled with invalid / missing flow setpoint for long.
- Watchdog reset on OT bus stall.
- Document that software control of a gas boiler requires working hardware
  safeties on the boiler itself.
