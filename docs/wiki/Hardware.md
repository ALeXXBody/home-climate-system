# Hardware

## Supported boards

| Board | MCU | Form factor | OT pins | 1-Wire pin | Notes |
|---|---|---|---|---|---|
| **D1 mini** | ESP8266 | Shields directly on DIYLess | GPIO4 (D2) / GPIO5 (D1) | GPIO14 (D5) | Basic tier — master only |
| **LOLIN S2 mini** | ESP32-S2 | Shields directly on DIYLess | GPIO4 / GPIO5 | GPIO15 | Primary ESP32 target; native USB |
| **LOLIN C3 mini v2.1** | ESP32-C3 | Shields directly on DIYLess | GPIO8 / GPIO10 | GPIO1 | Direct fitment; TX capped 17 dBm |
| **ESP32 D1 mini** | ESP32 | Needs jumper wires | GPIO21 / GPIO22 | GPIO18 | Classic ESP32 form factor |
| **ESP32-S3-Zero** | ESP32-S3 | Needs jumper wires | GPIO5 / GPIO6 | GPIO1 | 240 MHz; WS2812 RGB LED |

### Gateway builds (dual OpenTherm)

For boards with a second OT interface, `*_gw` builds add **gateway mode** — speak OpenTherm to both the boiler and a wall thermostat:

| Build | Master OT (boiler) | Slave OT (thermostat) |
|---|---|---|
| `lolin_s2_mini_gw` | GPIO4 / GPIO5 | GPIO16 / GPIO17 |
| `esp32_d1_mini_gw` | GPIO21 / GPIO22 | GPIO26 / GPIO27 |
| `lolin_c3_mini_gw` | GPIO8 / GPIO10 | GPIO4 / GPIO5 |

## OpenTherm shield

The **DIYLess Master OpenTherm Shield** connects the ESP to the boiler's OT bus:

- Plug the ESP board directly onto the shield (D1 mini, S2 mini, C3 mini)
- For ESP32 D1 mini and S3-Zero: use jumper wires (see pin map above)
- The shield provides galvanic isolation and voltage regulation
- **OT bus wiring:** two wires (OT+ and OT-) to the boiler's OT terminals

**Important:** The OT bus is polarity-free — the shield handles orientation.

## 1-Wire sensors (DS18B20)

Optional temperature probes for outdoor/return water/custom sensing:

- All probes share one GPIO bus
- **4.7 kΩ pull-up resistor** between DATA and 3.3V (required)
- VCC → 3.3V, GND → GND, DATA → board 1-Wire pin

### Pin map

| Board | 1-Wire GPIO |
|---|---|
| D1 mini | GPIO14 (D5) |
| LOLIN S2 mini | GPIO15 |
| LOLIN C3 mini | GPIO1 |
| ESP32 D1 mini | GPIO18 |
| ESP32-S3-Zero | GPIO1 |

### Roles

| Role | Purpose |
|---|---|
| `outdoor` | Outdoor temperature — feeds weather compensation, overrides OT MsgID 27 |
| `return` | Return water temperature — backfills OT MsgID 25 |
| `custom` | Named sensor published to HA under `hcs/<node>/x/<name>` |

Max 8 probes. Configure roles via the web UI Sensors tab or MQTT.

## Power supply

- USB micro/USB-C depending on board
- 5V recommended (USB or external adapter)
- The OT shield draws power from the ESP board — no separate supply needed

## Status LED

| Board | LED | Behaviour |
|---|---|---|
| D1 mini | GPIO2 (D4 pad) | Active LOW — fast blink = WiFi connecting, slow = OK |
| LOLIN S2 mini | GPIO15 | Active LOW |
| LOLIN C3 mini | GPIO7 WS2812 | RGB — green heartbeat, blue WiFi, red failsafe |
| ESP32 D1 mini | GPIO2 | Active HIGH |
| ESP32-S3-Zero | GPIO48 WS2812 | RGB — same as C3 mini |

### LED patterns

| Pattern | Colour | Meaning |
|---|---|---|
| 3s pulse | Green | OK — all connected |
| 260ms fast | Blue | WiFi connecting |
| 900ms slow | Red | OT link down |
| 320ms strobe | Red + orange | Failsafe active |
