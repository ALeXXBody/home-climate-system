# Connection-loss failsafe

What happens when the HCS device loses WiFi or MQTT — and how the house
stays warm while it does.

## State machine (evaluated every loop)

| State | When | Heating behaviour |
|---|---|---|
| `connected` | WiFi up **and** MQTT connected (or no broker configured) | Normal: HA/HCC commands rule |
| `hold` | link lost, inside grace period | Runs the last commanded state unchanged |
| `failsafe` | link lost longer than grace period | **CH forced ON** at the failsafe flow setpoint; weather compensation bypassed so behaviour is predictable |

Grace period default: **10 minutes**. On reconnect the device restores its
pre-failsafe CH/flow state; HA resumes control as soon as it talks again.

A reboot with the network still down re-enters the same path: after boot +
grace without a link, failsafe heating starts — a power blip during freezing
weather no longer means a cold house.

First-install portal mode never triggers the failsafe.

## Owner values (persisted on the device)

| Value | Default | Range |
|---|---|---|
| Enable | on | — |
| Flow setpoint | 40 °C | 20…90 °C |
| Grace period | 10 min | 1…120 min |

Editable from three places — all converge on the same saved values:

1. Device web UI → Controls → *Connection-loss failsafe* card
2. MQTT: publish JSON to `hcs/<node>/set/failsafe_cfg`
   `{"enable":true,"flow":40,"grace_min":10}`
3. HCC sidebar panel → Settings → *Connection-loss failsafe* card
   ("Save to device" pushes via MQTT)

Live state is visible everywhere: web badge (`FAILSAFE`),
`/api/status` → `failsafe{}`, retained MQTT `hcs/<node>/failsafe`
(`OFF/HOLD/ON`), and in HA as the
**Heating failsafe** sensor from Home Climate Control.
