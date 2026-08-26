# Boards & builds

## Build matrix

| Environment | MCU | Platform | Role | OT in/out | 1-Wire | LED |
|---|---|---|---|---|---|---|
| `d1_mini` | ESP8266 | espressif8266 | Master | 4/5 | 14 | GPIO2 (LOW) |
| `lolin_s2_mini` | ESP32-S2 | espressif32 | Master | 4/5 | 15 | GPIO15 (LOW) |
| `lolin_c3_mini` | ESP32-C3 | espressif32 | Master | 8/10 | 1 | GPIO7 (RGB) |
| `esp32_d1_mini` | ESP32 | espressif32 | Master | 21/22 | 18 | GPIO2 (HIGH) |
| `esp32s3_zero` | ESP32-S3 | espressif32 | Master | 5/6 | 1 | GPIO48 (RGB) |
| `lolin_s2_mini_gw` | ESP32-S2 | espressif32 | Gateway | 4/5 + 16/17 | 15 | GPIO15 |
| `esp32_d1_mini_gw` | ESP32 | espressif32 | Gateway | 21/22 + 26/27 | 18 | GPIO2 |
| `lolin_c3_mini_gw` | ESP32-C3 | espressif32 | Gateway | 8/10 + 4/5 | 1 | GPIO7 |
| `esp32_test` | ESP32 | espressif32 | Test | — | — | — |
| `native` | host | native | Test | — | — | — |

## Choosing a board

### Best value: LOLIN C3 mini

- Direct fitment on DIYLess shield (no wires)
- ESP32-C3 with WiFi + BLE
- Built-in WS2812 RGB status LED
- Low power, small form factor
- **Caveat:** TX power capped at 17 dBm (shares LDO with shield PIC)

### Most capable: LOLIN S2 mini

- ESP32-S2 with native USB CDC
- Direct fitment on DIYLess shield
- Full WiFi tx power
- Gateway builds available
- Best for gateway mode

### Classic choice: ESP32 D1 mini

- Well-known form factor
- Needs jumper wires to shield
- Gateway builds available
- Wider community support

### Budget: D1 mini (ESP8266)

- Cheapest option
- Master only (no gateway)
- Basic tier — no weather compensation UI parity
- Good for simple setups

### Premium: ESP32-S3-Zero

- 240 MHz, QIO flash
- WS2812 RGB LED
- Needs jumper wires
- Overkill for most setups

## Gateway builds

If you have a wall thermostat and want to keep it, use a `*_gw` build. These add a second OT interface (slave side) for the thermostat.

See [Gateway mode](Gateway-mode.md) for details.

## Release assets

Each release provides pre-built binaries:

```
firmware-d1_mini.bin
firmware-lolin_s2_mini.bin
firmware-lolin_c3_mini.bin
firmware-esp32_d1_mini.bin
firmware-esp32s3_zero.bin
firmware-lolin_s2_mini_gw.bin
firmware-esp32_d1_mini_gw.bin
firmware-lolin_c3_mini_gw.bin
```

## Build from source

```bash
cd firmware
pio run -e <environment>    # build
pio run -e <environment> -t upload  # build + flash
```

### Dependencies (auto-installed by PlatformIO)

| Library | Version | Purpose |
|---|---|---|
| PubSubClient | ^2.8 | MQTT |
| ArduinoJson | ^7.2.1 | JSON |
| OpenTherm Library | ^1.1.5 | OT master/slave |
| WiFiManager | ^2.0.17 | Captive portal |
| OneWire | ^2.3.8 | 1-Wire bus |
| DallasTemperature | ^4.0.3 | DS18B20 probes |
