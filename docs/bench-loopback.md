# Gateway bench loopback — runbook

Turnkey validation of the OpenTherm gateway (`*_gw` builds). Exit criteria
match [design-gateway.md](design-gateway.md) M1/M2: **frames decoded ≥99 %**,
**boiler operated transparently**, **override <1 s / clean release**,
**CI green** (already green).

## 0. What you need

| Item | Notes |
|---|---|
| Any ESP32 board + matching `*_gw` env | see table below |
| DIYLess Master OT shield | stacked (master/boiler side) |
| DIYLess Slave OT shield | thermostat side — IN/OUT to the env's `OT2_*` pins |
| Real room thermostat + boiler | wired on the thermostat bus segment |
| MQTT broker reachable from the bench | placeholders `<broker>`, node id `hcs-<mac>` below |

Pins per env:

| Env | Master OT (stacked) | Thermostat tap (Slave OT shield → `OT2_*`) |
|---|---|---|
| `lolin_s2_mini_gw` | GPIO4/5 | GPIO16/17 |
| `esp32_d1_mini_gw` | GPIO21/22 | GPIO26/27 |
| `lolin_c3_mini_gw` | GPIO7/6 | GPIO4/5 |

Bus topology:

```
boiler ──OT bus── DIYLess Master (stacked) ── MCU ── DIYLess Slave ──OT bus── room thermostat
                  (GPIOx_in ← boiler data)          (GPIOy_in ← tstat data,
                                                     GPIOy_out → tstat data)
```

## 1. Flash + first boot

```bash
cd firmware
pio run -e <env>_gw -t upload
pio device monitor -e <env>_gw -b 115200
```

Expect serial: board name + OT pin pair, then portal/WiFi IP and node id.
Gateway builds boot in **master_only** mode unless previously persisted.

Switch to gateway (pick one):

```bash
mosquitto_pub -h <broker> -t 'hcs/<node>/gw/set_mode' -m 'gateway'
# or web UI → Gateway tab → switch (device reboots)
```

After reboot serial shows `mode=gateway`.

## 2. Pass/fail checklist

Wire the thermostat segment last (MCU powered, monitor running):

- [ ] `gw/tstat_online` → `true` within ~2 s of connecting the thermostat
      (`mosquitto_sub -h <broker> -t 'hcs/<node>/gw/#' -v`)
- [ ] Serial logs raw forwarded MsgIDs: `0` status, `TSet` writes, `17`
      rel-mod-level, … every ~1 s poll cycle
- [ ] Boiler responds to the *real* thermostat: CH demand raises flow temp;
      flame bit flips in `hcs/<node>/status` telemetry
- [ ] Decode rate ≥99 %: web UI Gateway tab `rx_ok` vs `rx_err` after 15 min
- [ ] Counter identity holds: `forwarded + local_answered == rx_ok`
- [ ] Override: publish setpoint → boiler TSet changes <1 s
      `mosquitto_pub -h <broker> -t 'hcs/<node>/gw/override_setpoint' -m '45.0'`
      Release (`{"release":true}`) returns control to thermostat cleanly
- [ ] Stability: leave 60+ min — no watchdog resets (`reset_reason` in
      `/api/status`), counters monotonic, websocket/web UI stays connected
- [ ] Brown-out: power-cycle the boiler mid-session → gateway resumes
      forwarding without manual intervention

## 3. If it fails

| Symptom | First check |
|---|---|
| `tstat_online` never true | tap IN/OUT swapped vs table above; common GND with thermostat segment |
| garbage / high `rx_err` | 5 V vs 3.3 V level shift on tap; wire length >~10 m needs termination |
| forwards but boiler ignores | OUT stage transistor polarity; measure idle bus voltage (~18–24 V) |
| override never lands | boiler must accept `TSet` write from *its* master — check MsgID 1 ACK in serial |

Record results (counters, duration, anomalies) in the PR/commit message when
closing out M1/M2 exit criteria.
