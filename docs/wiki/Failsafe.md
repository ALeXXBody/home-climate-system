# Failsafe

Connection-loss protection keeps the house warm when WiFi or MQTT goes down.

## State machine

```
CONNECTED ──(link lost)──> HOLD ──(grace expired)──> FAILSAFE
    ^                         │                         │
    └──(link restored)────────┘──(link restored)────────┘
```

| State | Condition | Behaviour |
|---|---|---|
| **Connected** | WiFi up AND (no MQTT configured OR MQTT connected) | Normal — HA commands rule |
| **Hold** | Link lost, inside grace period | Runs last commanded CH/flow state unchanged |
| **Failsafe** | Link lost beyond grace period | CH forced ON at failsafe flow setpoint; WC bypassed |

## Configuration

| Setting | Default | Range | Where to set |
|---|---|---|---|
| Enable | on | — | Web UI / MQTT / HCC panel |
| Flow setpoint | 40 °C | 20–90 °C | Web UI / MQTT / HCC panel |
| Grace period | 10 min | 1–120 min | Web UI / MQTT / HCC panel |

### From web UI

Controls tab → Failsafe section: enable/disable, flow setpoint, grace period.

### From MQTT

```bash
mosquitto_pub -t "hcs/<node>/set/failsafe_cfg" \
  -m '{"enable":true,"flow":40,"grace_min":10}'
```

### From HCC panel

Devices tab → board controls → Failsafe config.

## What triggers failsafe

| Trigger | Hold? | Failsafe? |
|---|---|---|
| WiFi disconnect (router reboot) | Yes | After grace |
| MQTT broker goes down | Yes | After grace |
| HA restart | Usually no (usually <10 min) | Rarely |
| Network cable pulled from broker | Yes | After grace |
| Board reboot | No (starts in CH-off) | No |
| First-install portal mode | No | No |

## What happens in failsafe

1. CH forced **ON** (regardless of HA state)
2. Flow setpoint forced to failsafe value (40 °C default)
3. Weather compensation **bypassed** (predictable flow)
4. Board continues reading OT and publishing telemetry
5. Status LED shows **red strobe** (320 ms on/off)
6. MQTT `hcs/<node>/failsafe` → retained `ON`

## Recovery

When link is restored:
1. Board re-enters **Connected** state
2. Pre-failsafe CH/flow state is restored
3. HA resumes control
4. MQTT `hcs/<node>/failsafe` → retained `OFF`
5. Status LED returns to normal

## Important notes

- **First-install never triggers failsafe** — no MQTT configured yet = no link to lose
- **Failsafe is per-board** — each board has its own settings
- **WC bypassed in failsafe** — the curve needs outdoor data which may be stale
- **Reboot with network down** re-enters the same path (power blip = house is still warm)
- Max 3 revert attempts in the OTA rollback watchdog (separate from failsafe)
