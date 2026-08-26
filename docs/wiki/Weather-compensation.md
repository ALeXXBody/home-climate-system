# Weather compensation

Outdoor temperature drives a linear heating curve that determines the flow setpoint automatically.

## How the curve works

The curve maps outdoor temperature to flow temperature:

```
                 flow_max ─────────────┐
                                       │
                                       │  linear
                                       │  interpolation
                                       │
                 flow_min ─────────────┘
                 ──────────────────────────
                 design     ref     outdoor
                 temp       temp    (current)
```

| Parameter | Default | Meaning |
|---|---|---|
| Reference temp (ref) | 18 °C | Outdoor temp where flow = flow_min (mild weather) |
| Design temp | -10 °C | Outdoor temp where flow = flow_max (design day) |
| Flow max | 65 °C | Maximum flow setpoint |
| Flow min | 25 °C | Minimum flow setpoint |

### Calculation

- **Outdoor ≥ ref:** flow = flow_min (mild, minimum output)
- **Outdoor ≤ design:** flow = flow_max (coldest day, maximum output)
- **Between:** linear interpolation, clamped

**Example:** With defaults (ref=18, design=-10, fmax=65, fmin=25):
- Outdoor 18 °C → flow 25 °C
- Outdoor 4 °C → flow 45 °C (midpoint)
- Outdoor -10 °C → flow 65 °C

## Configuration

### From Home Assistant (recommended)

HCC sends WC config via MQTT:

```
hcs/<node>/set/weather_comp_cfg  →  "18,-10,65,25"
hcs/<node>/set/weather_comp      →  "on"
```

HCC's auto-tune adjusts the curve coefficient over time.

### From board web UI

Controls tab → Weather compensation section:
- Enable/disable toggle
- Reference, design, flow max, flow min inputs

### From MQTT directly

```bash
mosquitto_pub -t "hcs/<node>/set/weather_comp_cfg" -m "18,-10,65,25"
mosquitto_pub -t "hcs/<node>/set/weather_comp" -m "on"
```

## Validation

The firmware rejects invalid combinations:
- `flow_max` must be ≥ `flow_min`
- `design` must be < `ref`
- `flow_max` must be ≤ 90 °C
- `flow_min` must be ≥ 10 °C
- Invalid config is rejected atomically (old config preserved)

Bad payloads publish an error to `hcs/<node>/wc_error`.

## NVS persistence

WC settings are saved to NVS (ESP32) or EEPROM (ESP8266) and survive reboots:

| NVS key | Type | Default |
|---|---|---|
| `wc_en` | bool | false |
| `wc_ref` | float | 18.0 |
| `wc_dsn` | float | -10.0 |
| `wc_fmax` | float | 65.0 |
| `wc_fmin` | float | 25.0 |

Runtime changes are synced to NVS every 5 seconds (v1.4.0+).

## Outdoor temperature source

The curve needs outdoor temperature. Priority:

1. **1-Wire outdoor sensor** (if assigned role `outdoor`)
2. **OpenTherm MsgID 27** (boiler's outdoor sensor)
3. **HA fallback sensor** (configured in HCC — boiler → HA sensor → stale → none)
4. **Stale** (>30 min old) — curve pauses, uses last known flow setpoint

## Interaction with failsafe

WC is **bypassed during failsafe** — the board uses the failsafe flow setpoint instead. This ensures predictable behaviour when the outdoor sensor is unreliable.

## Auto-tune (HCC side)

HCC adjusts the curve coefficient based on whether rooms reach their targets:
- Rooms consistently too warm → coefficient decreases (lower flow)
- Rooms consistently too cold → coefficient increases (higher flow)
- Learning is slow and conservative

The auto-tune adjusts the **HCC-side coefficient** — it sends the resulting WC config to the board.
