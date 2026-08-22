# Home Climate System — firmware architecture

## Role

```
Boiler (OpenTherm)  <->  DIYLess Master Shield  <->  ESP (D1 mini / S3-Zero)
                                                      |
                                                     WiFi
                                                      |
                                                    MQTT
                                                      |
                                            Home Climate Control (HA)
```

Master shield = device is the **thermostat** (not a man-in-the-middle gateway).

## Modules

| File | Responsibility |
|---|---|
| `main.cpp` | Boot, WiFi, failsafe CH off, loop |
| `ot_master.*` | OpenTherm master transactions (status, TSet, reads) |
| `mqtt_bridge.*` | Subscribe commands, publish telemetry (HCS + OTGW-compat) |
| `config.h` | Pins, intervals, defaults |
| `topics.h` | Topic string helpers |
| `secrets.h` | WiFi/MQTT credentials (local only) |

## Control path

1. HA / HCC publishes `flow_setpoint` + `ch_enable`
2. Firmware stores desired state
3. ~1 Hz: `setBoilerStatus` + `setBoilerTemperature` + read sensors
4. Telemetry published every 2 s

## Boards

| Env | MCU | OT_IN | OT_OUT |
|---|---|---|---|
| `d1_mini` | ESP8266 | 4 (D2) | 5 (D1) |
| `esp32_d1_mini` | ESP32 | 21 | 22 |
| `esp32s3_zero` | ESP32-S3 | 5 | 6 |

## Dependencies (MIT-friendly)

- ihormelnyk OpenTherm Library — MIT
- knolleary PubSubClient — MIT
- bblanchon ArduinoJson — MIT (reserved for future config JSON)

No GPL sources (see docs/license-otgw.md).
