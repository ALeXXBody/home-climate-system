# Home Climate System — firmware architecture

## Role

```
Boiler (OpenTherm)  <->  DIYLess Master Shield  <->  ESP (D1 mini / S2 mini / S3-Zero)
                                                       |
                                                      WiFi
                                                       |
                                      HTTP (UI/OTA) + MQTT
                                                       |
                                             Home Climate Control (HA)
```

Master shield = device is the **thermostat** (not a man-in-the-middle gateway).

## Modules

| File | Responsibility |
|---|---|
| `main.cpp` | Boot, failsafe CH off, wire modules, loop |
| `ot_master.*` | OpenTherm master transactions (status, TSet, reads) |
| `mqtt_bridge.*` | Subscribe commands, publish telemetry + discovery |
| `net_services.*` | WiFiManager portal, device web UI, ElegantOTA, ArduinoOTA, HTTP update |
| `settings_store.*` | NVS (ESP32) / EEPROM blob (ESP8266) for WiFi/MQTT/settings |
| `hcs_commands.h` | Portable command parser (host-tested via `pio test -e native`) |
| `hcs_weather_comp.h` | Heating-curve math + CSV config parser (host-tested) |
| `hcs_gateway.h` | Gateway router core: policies, overrides, cache (host-tested) |
| `ot_slave.*` | Thermostat-bus slave endpoint (ESP32 gateway builds) |
| `ot_gateway.h/.cpp` | Gateway glue: route/forward/local-answer state machine (ESP32) |
| `config.h` | Pins, intervals, defaults, portal AP name |
| `topics.h` | Topic string helpers |
| `secrets.h` | Optional compile-time WiFi/MQTT seeds (local only) |

## Control path

1. HA / HCC / web UI sets `flow_setpoint` + `ch_enable` (MQTT or `/api/control`)
2. Firmware stores desired state on `OtMaster`
3. ~1 Hz: `setBoilerStatus` + `setBoilerTemperature` + read sensors
4. Telemetry published every 2 s; discovery JSON every 30 s

## Boards

| Env | MCU | OT_IN | OT_OUT | Notes |
|---|---|---|---|---|
| `d1_mini` | ESP8266 | 4 (D2) | 5 (D1) | Stacks on DIYLess Master (basic tier) |
| `lolin_s2_mini` | ESP32-S2 | 4 | 5 | Primary ESP32; D1-mini layout |
| `esp32_d1_mini` | ESP32 | 21 | 22 | Classic D1 mini ESP32 |
| `esp32s3_zero` | ESP32-S3 | 5 | 6 | Extra; jumper wires |
| `lolin_s2_mini_gw` | ESP32-S2 | 4/5 + 16/17 | — | Gateway build (`HCS_GW_ENABLE`) |
| `esp32_d1_mini_gw` | ESP32 | 21/22 + 26/27 | — | Gateway build (`HCS_GW_ENABLE`) |

**Tiers:** ESP8266 = basic (master thermostat only). ESP32 = advanced (adds
weather compensation UI parity + optional gateway mode via `*_gw` envs).

## Dependencies (MIT-friendly)

- ihormelnyk OpenTherm Library — MIT
- knolleary PubSubClient — MIT
- bblanchon ArduinoJson — MIT
- tzapu WiFiManager — MIT
- ayushsharma82 ElegantOTA — MIT

No GPL sources (see docs/license-otgw.md).
