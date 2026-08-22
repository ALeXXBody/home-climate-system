# DIYLess Master OpenTherm Shield + your MCUs

## Hardware you have

| Piece | Role |
|---|---|
| **DIYLess Master OpenTherm Shield** | Level-shifts MCU GPIO ↔ boiler OpenTherm bus (master / thermostat side) |
| **ESP8266 D1 mini** | Stacks on the shield (DIYLESS designed for this) |
| **ESP32-S3-Zero** | Does **not** stack; wire OT_IN / OT_OUT with jumpers |

The Master shield talks to the **boiler only** (replaces wall thermostat).  
It is **not** a pass-through gateway (that needs Master+Slave shields).  
Home Climate Control becomes the thermostat brain over MQTT.

## Wiring

### A) ESP8266 D1 mini (recommended first flash)

Stack D1 mini on the Master OpenTherm Shield (headers as DIYLESS docs).

| Function | D1 mini | GPIO | Shield |
|---|---|---|---|
| OT receive (in) | **D2** | GPIO4 | OT_IN |
| OT transmit (out) | **D1** | GPIO5 | OT_OUT |
| GND | GND | — | GND |
| 3V3 | 3V3 | — | 3V3 |

Boiler: two-wire OpenTherm to the shield screw terminal (**polarity free**).

Firmware env: `d1_mini` (`OT_IN=4`, `OT_OUT=5`).

### B) ESP32-S3-Zero (jumper wires)

S3-Zero is too small / different pinout to stack. Power the shield from 3V3+GND
and jump OT lines:

| Function | ESP32-S3-Zero | Shield |
|---|---|---|
| OT receive (in) | **GPIO5** | OT_IN |
| OT transmit (out) | **GPIO6** | OT_OUT |
| GND | GND | GND |
| 3V3 | 3V3 | 3V3 |

Do **not** cover the ceramic antenna. USB-C for power/flash  
(hold **BOOT** then plug USB to enter download mode on S3-Zero).

Firmware env: `esp32s3_zero` (`OT_IN=5`, `OT_OUT=6`).

Pins are build flags — change in `firmware/platformio.ini` if you rewire.

### C) Classic ESP32 D1 mini form-factor

DIYLESS sample uses **GPIO21 = IN**, **GPIO22 = OUT**.  
Env: `esp32_d1_mini`.

## Power

Shield is powered from the MCU 3.3 V rail. USB power on the MCU is enough for
bench testing. Keep OT cable reasonably short; OpenTherm is low-voltage but
noisy near mains.

## Safety

- Boiler hardware safeties must work (pressure, over-temp, flue).
- Firmware boots with **CH off** until MQTT enables it.
- Never leave CH on with all zone valves closed for long periods.

## Library license

OpenTherm bit-level stack: [ihormelnyk/opentherm_library](https://github.com/ihormelnyk/opentherm_library) **MIT** — safe to use with our MIT firmware.  
OTGW-firmware (GPL) is **not** used.
