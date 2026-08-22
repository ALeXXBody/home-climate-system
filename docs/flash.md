# Building & flashing Home Climate System firmware

## Prerequisites

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation.html) or PlatformIO IDE
- USB cable for D1 mini, LOLIN S2 mini, or ESP32-S3-Zero
- MQTT broker reachable from the device (e.g. HA Mosquitto)
- DIYLess Master OpenTherm Shield wired per [hardware.md](hardware.md)

## 1. First boot (captive portal)

Firmware **v0.2+** opens AP **`HCS-Setup`** / password **`homeclimate`** when WiFi
is not configured. Join it, open the portal, set home WiFi + MQTT, save.

Optional compile-time seeds (gitignored):

```bash
cd firmware
cp include/secrets.example.h include/secrets.h
# edit WIFI_SSID, WIFI_PASS, MQTT_HOST, …
```

## 2. Build

```bash
cd firmware

# ESP8266 D1 mini (stacked on DIYLess Master shield)
pio run -e d1_mini

# LOLIN / Wemos S2 mini (D1-mini layout, OT GPIO4/5)
pio run -e lolin_s2_mini

# ESP32-S3-Zero (jumper wires — extra target)
pio run -e esp32s3_zero

# Classic ESP32 D1 mini form factor
pio run -e esp32_d1_mini

# LOLIN C3 mini v2.1 — direct fitment, stacks on the DIYLess shield (OT GPIO7/6)
pio run -e lolin_c3_mini
```

Gateway builds (ESP32 only — adds the thermostat-side OT front-end; see
[design-gateway.md](design-gateway.md)):

```bash
pio run -e lolin_s2_mini_gw    # S2 mini,  OT 4/5 + tstat tap 16/17
pio run -e esp32_d1_mini_gw    # ESP32 D1 mini, OT 21/22 + tap 26/27
pio run -e lolin_c3_mini_gw    # C3 mini,  OT 7/6  + tstat tap 4/5
```

## 3. Flash

```bash
# D1 mini
pio run -e d1_mini -t upload

# S2 mini (native USB CDC; hold BOOT/0 if needed)
pio run -e lolin_s2_mini -t upload

# C3 mini (native USB CDC; hold BOOT/0 if needed)
pio run -e lolin_c3_mini -t upload

# S3-Zero: hold BOOT, plug USB, then:
pio run -e esp32s3_zero -t upload
```

Serial monitor:

```bash
pio device monitor -e lolin_s2_mini -b 115200
```

You should see board name, OT pins, then portal or WiFi IP + node id (`hcs-<mac>`).
Device web UI: `http://<ip>/` (status, controls, settings, ElegantOTA).

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

## 5. Gateway mode (gateway builds only)

After flashing a `*_gw` env the device boots as an OpenTherm **master**
(thermostat replacement). Switch to gateway mode — it forwards thermostat
traffic to the boiler and answers locally only when asked:

```bash
# via MQTT (retained)
mosquitto_pub -h <broker> -t 'hcs/hcs-aabbccddeeff/gw/set_mode' -m 'gateway'

# or web UI → Gateway tab → switch mode (reboots into gateway mode)
```

Verify forwarding: `gw/tstat_online` flips to `true` once the real
thermostat's frames are seen; `gw/mode` echoes the active mode. A setpoint
override can be pushed with `gw/override_setpoint` (`null`/`{"release":true}`
releases it).

Mode is persisted and re-applied at boot; switching always reboots.

## 6. Home Climate Control

In HA, add **Home Climate Control** with backend **Real OTGW via MQTT**:

- MQTT top topic: `OTGW` (default)
- Node id: `hcs-device` (or change `OTGW_COMPAT_NODE` in `secrets.h` / `config.h`)

Telemetry subjects match OTGW-firmware names (`boilertemperature`, `flamestatus`, …).

## Troubleshooting

| Symptom | Check |
|---|---|
| No WiFi | join `HCS-Setup` / `homeclimate`, 2.4 GHz SSID only |
| No MQTT | portal MQTT host, broker IP, firewall, user/pass |
| OT `valid=false` | wiring IN/OUT swapped?, boiler powered, OT cable |
| C3 mini RGB LED flickers | normal on stacked shield — GPIO7 carries OT traffic |
| S2/S3/C3 won't flash | hold BOOT/0 while connecting USB-C |
| CH never starts | publish `ch_enable=on` — boots failsafe off |
