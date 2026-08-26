# Gateway mode

For setups with an existing wall thermostat that you want to keep. The HCS board sits between the thermostat and boiler, forwarding OT traffic while also allowing HA to override.

> **Requires** a `*_gw` firmware build on an ESP32 board with two OT interfaces.

## Topology

```
Wall thermostat ──[Slave OT shield]──> HCS device ──[Master OT shield]──> Boiler
                     GPIO16/17                            GPIO4/5
```

Two independent OpenTherm interfaces:
- **Master side** (GPIO4/5): speaks to the boiler
- **Slave side** (GPIO16/17): pretends to be a boiler to the wall thermostat

## Modes

| Mode | Behaviour |
|---|---|
| `auto` | Silent listen on thermostat bus for 15 s; ≥2 valid requests → gateway, else master_only |
| `master_only` | Forced master (boiler side only); thermostat port ignored |
| `gateway` | Forced master+slave gateway; forwards thermostat requests to boiler |

### Auto-detect

On boot, the board silently listens on the thermostat OT bus:
- **≥2 valid requests** within 15 seconds → gateway mode (wall thermostat is present)
- **<2 requests** → master_only (no thermostat, standard setup)

Mode is persisted in NVS. Switching always reboots the device.

## How it works

### Request routing

When the wall thermostat sends a request:

1. **Forward** — pass (possibly modified) request to the boiler
2. **AnswerLocal** — synthesise a response locally without contacting the boiler

### Forward

Normal path — the thermostat's request goes to the boiler and the boiler's response is relayed back.

### AnswerLocal

Used when the boiler link is down:
- Uses cached boiler responses (8-entry ring buffer)
- For `READ_DATA` requests: returns cached value if available
- For `WRITE_DATA`: ACKs locally without boiler contact
- Unknown IDs get `UNKNOWN_DATA_ID` response

### Setpoint override

HCC (via MQTT) can force a flow temperature that overrides the wall thermostat:

```bash
# Force 45 °C flow
mosquitto_pub -t "hcs/<node>/set/gw/override_setpoint" -m "45.0"

# Release override
mosquitto_pub -t "hcs/<node>/set/gw/override_setpoint" -m "off"
```

When active, the board rewrites `WRITE_DATA(TSet)` payloads before forwarding to the boiler.

### Reference mode

When the wall thermostat goes silent in gateway mode, periodic reference polls keep telemetry/diagnostics fresh (every 60 s). No setpoint writes — just reads.

## MQTT additions (gateway builds)

| Topic | Direction | Payload |
|---|---|---|
| `…/gw/mode` | pub | `master_only` / `gateway` (retained) |
| `…/gw/set_mode` | sub | `auto` / `gateway` / `master_only` (saves + reboots) |
| `…/gw/tstat_online` | pub | `ON` / `OFF` |
| `…/gw/override_setpoint` | sub+pub | float °C or `off`/`auto`/`release` |

Discovery JSON includes a `gw` block with traffic counters.

## Pin map for gateway builds

| Build | Master OT (boiler) | Slave OT (thermostat) |
|---|---|---|
| `lolin_s2_mini_gw` | GPIO4 / GPIO5 | GPIO16 / GPIO17 |
| `esp32_d1_mini_gw` | GPIO21 / GPIO22 | GPIO26 / GPIO27 |
| `lolin_c3_mini_gw` | GPIO8 / GPIO10 | GPIO4 / GPIO5 |

## When to use gateway mode

- You have a wall thermostat you want to keep (e.g. Honey, Nest, Vaillant VRC)
- You want HCC to override the thermostat's flow setpoint via HA
- You want both HA automation AND manual thermostat control

## When NOT to use gateway mode

- No wall thermostat (standard setup — just use master_only)
- The wall thermostat doesn't use OpenTherm (it's a relay-only thermostat)
- You don't need to keep the wall thermostat
