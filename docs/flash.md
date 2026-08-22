# Building & flashing Home Climate System firmware

## Prerequisites

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation.html) or PlatformIO IDE
- USB cable for D1 mini or ESP32-S3-Zero
- MQTT broker reachable from the device (e.g. HA Mosquitto)
- DIYLess Master OpenTherm Shield wired per [hardware.md](hardware.md)

## 1. Secrets

```bash
cd firmware
cp include/secrets.example.h include/secrets.h
# edit WIFI_SSID, WIFI_PASS, MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASS
```

`secrets.h` is gitignored.

## 2. Build

```bash
cd firmware

# ESP8266 D1 mini (stacked on DIYLess Master shield)
pio run -e d1_mini

# ESP32-S3-Zero (jumper wires)
pio run -e esp32s3_zero

# Classic ESP32 D1 mini form factor
pio run -e esp32_d1_mini
```

## 3. Flash

```bash
# D1 mini
pio run -e d1_mini -t upload

# S3-Zero: hold BOOT, plug USB, then:
pio run -e esp32s3_zero -t upload
```

Serial monitor:

```bash
pio device monitor -e d1_mini -b 115200
```

You should see WiFi IP, MQTT node id (`hcs-<mac>`), and OT pin numbers.

## 4. MQTT smoke test

Replace `hcs-aabbccddeeff` with the node id from serial:

```bash
# Enable CH and set flow 50 °C (native HCS topics)
mosquitto_pub -h <broker> -t 'hcs/hcs-aabbccddeeff/set/ch_enable' -m 'on'
mosquitto_pub -h <broker> -t 'hcs/hcs-aabbccddeeff/set/flow_setpoint' -m '50.0'

# Or OTGW-compat (Home Climate Control existing backend)
mosquitto_pub -h <broker> -t 'OTGW/set/hcs-device/chenable' -m 'on'
mosquitto_pub -h <broker> -t 'OTGW/set/hcs-device/ctrlsetpt' -m '50.0'
```

Subscribe:

```bash
mosquitto_sub -h <broker> -t 'hcs/#' -v
mosquitto_sub -h <broker> -t 'OTGW/#' -v
```

## 5. Home Climate Control

In HA, add **Home Climate Control** with backend **Real OTGW via MQTT**:

- MQTT top topic: `OTGW` (default)
- Node id: `hcs-device` (or change `OTGW_COMPAT_NODE` in `secrets.h` / `config.h`)

Telemetry subjects match OTGW-firmware names (`boilertemperature`, `flamestatus`, …).

## Troubleshooting

| Symptom | Check |
|---|---|
| No WiFi | `secrets.h`, 2.4 GHz SSID |
| No MQTT | broker IP, firewall, user/pass |
| OT `valid=false` | wiring IN/OUT swapped?, boiler powered, OT cable |
| S3 won't flash | hold BOOT while connecting USB-C |
| CH never starts | publish `ch_enable=on` — boots failsafe off |
