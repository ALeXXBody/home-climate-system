# DIYLess Master OpenTherm Shield + your MCUs

## Hardware you have

| Piece | Role |
|---|---|
| **DIYLess Master OpenTherm Shield** | Level-shifts MCU GPIO ↔ boiler OpenTherm bus (master / thermostat side) |
| **ESP8266 D1 mini** | Stacks on the shield (DIYLESS designed for this) |
| **LOLIN / Wemos S2 mini** | Same D1-mini layout family; OT pins match ESP8266 (GPIO4/5) |
| **LOLIN C3 mini v2.1** | Direct fitment — stacks; OT lands on GPIO8 (in) / GPIO10 (out) |
| **ESP32-S3-Zero** | Extra target; does **not** stack — jumper OT_IN / OT_OUT |

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

### B) LOLIN / Wemos S2 mini (primary ESP32 target)

Same D1-mini footprint / pin labeling family as the ESP8266 shield stack.
Use the same OT pin numbers as the D1 mini:

| Function | S2 mini label | GPIO | Shield |
|---|---|---|---|
| OT receive (in) | **D2** | GPIO4 | OT_IN |
| OT transmit (out) | **D1** | GPIO5 | OT_OUT |
| GND | GND | — | GND |
| 3V3 | 3V3 | — | 3V3 |

Do **not** use GPIO33–37 (embedded flash on S2FH4).  
USB-C is native CDC serial (no FTDI). Hold **0/BOOT** if download mode is needed.

Firmware env: `lolin_s2_mini` (`OT_IN=4`, `OT_OUT=5`).

### C) ESP32-S3-Zero (extra — jumper wires)

S3-Zero is too small / different pinout to stack. Power the shield from 3V3+GND
and jump OT lines:

| Function | ESP32-S3-Zero | Shield |
|---|---|---|
| OT receive (in) | **GPIO8** | OT_IN |
| OT transmit (out) | **GPIO10** | OT_OUT |
| GND | GND | GND |
| 3V3 | 3V3 | 3V3 |

Do **not** cover the ceramic antenna. USB-C for power/flash  
(hold **BOOT** then plug USB to enter download mode on S3-Zero).

Firmware env: `esp32s3_zero` (`OT_IN=5`, `OT_OUT=6`).

### D) LOLIN C3 mini v2.1 (direct fitment — stacks)

The C3 mini is D1-mini footprint and Wemos lists it as shield-compatible.
Its right header is TX, RX, IO10, IO8, IO7, IO6, GND, 5V (Wemos C3 mini
v2.1.0 schematic) — so stacked on the DIYLess Master shield, the shield's
`OT_IN` pad (D1-mini D2 position) lands on C3 **GPIO8**, and `OT_OUT`
(D1 position) on **GPIO10** (verified against both Wemos schematics).
Power pads (5V/GND/3V3) line up as on the ESP8266.

Note: GPIO7 (the D3-position pad) drives the onboard WS2812 RGB LED and is
free for other uses — it is NOT part of the OpenTherm interface.

Gateway build (`lolin_c3_mini_gw`) adds the thermostat-side interface on
free **GPIO4** (in) / **GPIO5** (out) — wire these to a DIYLess **Slave OT**
shield's IN/OUT (see docs/design-gateway.md).

Firmware env: `lolin_c3_mini` (`OT_IN=7`, `OT_OUT=6`).

Pins are build flags — change in `firmware/platformio.ini` if you rewire.

### E) Classic ESP32 D1 mini form-factor

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

### E) 1-Wire DS18B20 probes (Sensors tab)

One GPIO runs the whole 1-Wire bus — many probes share the same pair.
Wire every probe: `VCC → 3.3 V`, `GND → GND`, `DATA → board pin`, plus a
**4.7 kΩ resistor between DATA and 3.3 V** (one per bus, anywhere).

Default pin per env (`-DHCS_ONEWIRE_PIN`, change in `platformio.ini`):

| Env | 1-Wire pin | OT pins in use |
|---|---|---|
| `d1_mini` | **GPIO14** (D5) | 4/5 |
| `lolin_s2_mini(_gw)` | **GPIO15** | 4/5 (+16/17 gw) |
| `lolin_c3_mini(_gw)` | **GPIO5** (D7 pad) | 8/10 (+4/5 gw) |
| `esp32_d1_mini(_gw)` | **GPIO18** | 21/22 (+26/27 gw) |
| `esp32s3_zero` | **GPIO1** | 5/6 |

Roles are assigned in the device web UI (**Sensors** tab): an assigned probe
overrides the boiler's value for that channel while fresh (<90 s), else the
OpenTherm value passes through. `outdoor` feeds weather compensation;
`return` backfills return-water telemetry for boilers that don't report it.

## Library license

OpenTherm bit-level stack: [ihormelnyk/opentherm_library](https://github.com/ihormelnyk/opentherm_library) **MIT** — safe to use with our MIT firmware.  
No third-party gateway firmware is used.
