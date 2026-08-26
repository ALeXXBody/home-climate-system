# 1-Wire sensors

Optional DS18B20 temperature probes for outdoor, return water, or custom sensing.

## Hardware

- **Sensor:** DS18B20 digital thermometer (waterproof or TO-92 package)
- **Wiring:** All probes share one GPIO bus
  - VCC → 3.3V
  - GND → GND
  - DATA → board 1-Wire pin
- **Pull-up:** 4.7 kΩ resistor between DATA and 3.3V (one per bus, required)

### Default 1-Wire pins

| Board | GPIO |
|---|---|
| D1 mini | GPIO14 (D5) |
| LOLIN S2 mini | GPIO15 |
| LOLIN C3 mini | GPIO1 |
| ESP32 D1 mini | GPIO18 |
| ESP32-S3-Zero | GPIO1 |

## Detection

The firmware runs a non-blocking scan every 15 seconds:
1. Send reset pulse → detect presence
2. Read ROM address (64-bit, includes family code `0x28`)
3. Start temperature conversion (800 ms at 12-bit resolution)
4. Read scratchpad → CRC check → temperature

Boot does one blocking conversion for immediate health status.

## Health classification

| Status | Meaning |
|---|---|
| `ok` | Reading valid, stable |
| `disconnected` | No presence pulse / DEVICE_DISCONNECTED_C |
| `crc` | Scratchpad CRC8 failure |
| `implausible` | Outside -55..125 °C |
| `stuck85` | Two consecutive exact 85.0 °C readings (power-on default) |
| `unstable` | Step change >15 °C between polls |
| `unsupported` | Non-DS18B20 family code (not `0x28`) |

## Roles

| Role | Purpose | Effect |
|---|---|---|
| `none` | Unassigned | No OT override |
| `outdoor` | Outdoor temperature | Overrides OT MsgID 27; feeds weather compensation |
| `return` | Return water temp | Backfills OT MsgID 25 |
| `custom` | Named sensor | Published to `hcs/<node>/x/<name>` for HA |

### Assignment rules
- `outdoor` and `return` are unique — assigning to a new probe removes it from the old one
- Custom sensors require a sanitized name: 2–16 chars, `[a-z0-9_]`, unique among customs
- Max 8 slots total

### Override logic
If a probe is assigned + fresh (<90 s) + healthy → it overrides the OT value. Otherwise the OT value passes through.

## Configuration

### Web UI Sensors tab

1. Navigate to the board's IP → Sensors tab
2. Enable 1-Wire if disabled
3. For each probe: select a Role from the dropdown
4. For custom sensors: enter a name (lowercase, underscores)
5. Click Save

### MQTT

```bash
# View sensors
mosquitto_sub -t "hcs/<node>/sensors"

# Custom sensor publishes to:
# hcs/<node>/x/<sensor_name>  →  "21.3"
```

### Self-test

From the Sensors tab, click **Test** — forces a full re-scan + double conversion to verify probe health. Takes ~2 seconds.

## NVS storage

| Key | Type | Description |
|---|---|---|
| `ow_en` | bool | 1-Wire enabled |
| `ow_n` | UChar | Number of configured slots |
| `ow0a`..`ow7a` | String | Probe addresses (16-char hex) |
| `ow0r`..`ow0r` | UChar | Probe roles (0=none, 1=outdoor, 2=return, 3=custom) |
| `ow0n`..`ow7n` | String | Custom sensor names |

Legacy keys `ow_out`/`ow_ret` are maintained for OTA migration from older firmware.

## Tips

- Use **waterproof** DS18B20 probes for outdoor and return water
- Keep probe wires short (<5 m) or use shielded cable for longer runs
- Multiple probes can share the bus — just connect them in parallel
- The 85 °C "stuck" detection catches the common DS18B20 power-on default issue
