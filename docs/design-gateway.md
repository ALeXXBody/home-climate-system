# Design — Gateway mode (thermostat ⇄ HCS ⇄ boiler)

Status: **M1+M2 implemented (passthrough core, ESP32-only builds)** · Target firmware: v0.3.x
License stance: original work; OpenTherm protocol behaviour follows the
public spec (v2.2+). Ideas (not code) informed by how gateways generally
work; see [`license-otgw.md`](license-otgw.md) for why no GPL sources are used.

> Implementation status:
> - `include/hcs_gateway.h` — portable router core, 9 native tests (FORWARD /
>   local-answer / setpoint override / f8.8 helpers)
> - `ot_slave.*` + `ot_gateway.h/.cpp` — slave endpoint + glue, compiled only
>   under `HCS_GW_ENABLE` on ESP32 envs (`lolin_s2_mini_gw`, `esp32_d1_mini_gw`,
>   `lolin_c3_mini_gw`)
> - Mode persisted in settings; boot applies saved mode (deviation from the
>   "master_only after 10 s" idea below — simpler, mode switch always reboots)
> - MQTT: `gw/mode`, `gw/set_mode`, `gw/tstat_online`, `gw/override_setpoint`
> - Web UI: Gateway tab (mode switch + reboot, live counters, override)
> - Still open: bench loopback (needs hardware) — turnkey checklist in
>   [`bench-loopback.md`](bench-loopback.md)

## 1. Problem

Today HCS is the **thermostat** (master-only): it drives the boiler over the
DIYLess Master shield and takes commands from HA. A real room thermostat
cannot be used at the same time.

Gateway mode puts HCS **between** the existing wall thermostat and the
boiler so that:

- the thermostat keeps working normally (pass-through), and
- HA can observe all traffic and selectively **override** messages
  (setpoint, CH/DHW enable, …) without touching the thermostat.

## 2. Topology

```
                 +-----------------------------------+
 Wall            |              HCS device           |             Boiler
 thermostat      |                                   |
                 |  +-----------+      +-----------+ |
 OT(T) ~~~~~~~~~~~>| Slave iface|----->| Router    | |
 (thermostat     |  | ot_slave  |      | gw_router | |
  pair, 2-wire)  |  +-----------+      +-----------+ |
                 |       ^                  |        |
                 |       | answers          v        |
                 |       |             +-----------+ |
                 |       +-------------| Master    |<~~~ OT(B) ~~~~~ Boiler
                 |         locally     | iface     |          (2-wire)
                 |                     +-----------+ |
                 +-----------------------------------+
                        USB / WiFi / MQTT -> Home Assistant
```

Two independent OpenTherm interfaces:

| Interface | Role | Talks to | Hardware |
|---|---|---|---|
| **Master** | OpenTherm master | Boiler | Existing DIYLess Master shield (stacked) |
| **Slave** | OpenTherm slave | Wall thermostat | Second front-end (OT-Direct rev B piggyback) |

Both buses are polarity-free 2-wire loops, 15–18 V idle, current-modulated.
Each interface needs one RX (interrupt-capable) + one TX GPIO.

## 3. Electrical plan

### Option A — recommended first rig

- Boiler side: **DIYLess Master OpenTherm Shield** (already owned).
- Thermostat side: **OT-Direct** front-end (see
  [`hardware/ot-direct/`](../hardware/ot-direct/)) wired to the thermostat
  pair instead of the boiler pair. Same ~€5 jellybean BOM.

### Option B — fully DIY

- Two OT-Direct boards (one per bus).

### Pin map (lolin_s2_mini, proposed)

| Function | GPIO | Notes |
|---|---|---|
| Master IN (boiler RX) | 4 (D2) | unchanged |
| Master OUT (boiler TX) | 5 (D1) | unchanged |
| Slave IN (thermostat RX) | **16** | proposed |
| Slave OUT (thermostat TX) | **17** | proposed |

Avoid: GPIO19/20 (USB), GPIO0 (boot strap), GPIO33–37 (embedded flash).
**Verify GPIO16/17 against the Wemos S2 mini schematic before wiring a
permanent rig**; pins stay build-flag configurable either way.

New PlatformIO env `lolin_s2_mini_gateway` adds
`-DOT2_IN_PIN=16 -DOT2_OUT_PIN=17 -DHCS_GW_ENABLE`; all existing envs remain
master-only unless `HCS_GW_ENABLE` is defined.

## 4. Firmware architecture

### New modules

| File | Responsibility |
|---|---|
| `include/hcs_gateway.h` | Portable router core (pure C/C++): per-MsgID policy table, override value slots, cache/TTL logic. Host-tested. |
| `src/ot_slave.cpp/.h` | Slave-side OpenTherm endpoint on the thermostat bus (ihormelnyk lib in slave role, ISR-driven). Decodes requests, queues responses. |
| `src/ot_gateway.cpp/.h` | Glue: runs slave + master, executes routing decisions, maintains counters/stats, failsafe state machine. |

### Routing model

Every thermostat request frame arrives at `ot_slave`. The router looks up
its `MsgID` in the policy table:

| Policy | Behaviour |
|---|---|
| `FORWARD` | Send verbatim to boiler via master iface; relay boiler response back to thermostat. Default for all IDs. |
| `MODIFY` | Apply override function, then forward modified request; relay response. Used for forced setpoint / flags. |
| `ANSWER_LOCAL` | Do not touch boiler; synthesise response from local state/cache. Used when boiler link is down or for observation-only IDs. |

Initial override set (phase 2, each independently toggleable):

- Force CH flow setpoint (MsgID 1 write) — `override_setpoint`
- Force max modulation (MsgID 14)
- Force CH/DHW enable bits carried in Status (MsgID 0)
- Synthesise outdoor temp (MsgID 24 write from thermostat, if ever seen)

### Timing & concurrency

- Both endpoints are ISR-driven (bit timing in-library); `loop()` only moves
  completed frames → no hard realtime burden.
- Forwarding adds one transaction latency (~10–40 ms) inside the ~800 ms
  spec response window — comfortable.
- One request outstanding per bus at a time (OpenTherm is strictly
  master-initiated); simple hand-off flag suffices, no queue needed.

### Failsafe state machine

```
BOOT -> MASTER_ONLY (factory default; saved mode applied after 10 s uptime)
GATEWAY:
  boiler_ok  && tstat_ok   : normal
  boiler down >30 s        : ANSWER_LOCAL from cache; publish fault; never
                             fabricate CH-on older than COMMAND_WATCHDOG_MS
  thermostat silent >120 s : keep master link warm; nothing forwarded anyway
  any fault                : MQTT lwt-style status + red badge in web UI
CH at boot stays OFF unless thermostat commanded it within the last cycle.
```

Mode is persisted (`settings_store`, new key); switching modes at runtime
requires reboot (documented; keeps bring-up simple and safe).

## 5. External contract deltas

### MQTT (native `hcs/<node>/…`)

| Topic | Dir | Payload |
|---|---|---|
| `…/gw/mode` | pub | `master_only` \| `gateway` |
| `…/gw/set_mode` | sub | same → saves + reboots |
| `…/gw/tstat_online` | pub | `ON`/`OFF` (carrier/activity on thermostat bus) |
| `…/gw/override_setpoint` | sub | float °C, or `off` to release |
| `…/gw/fwd_total`, `…/gw/err_total` | pub | unsigned counters |

Existing `hcs/*` telemetry continues to reflect the **effective** state
(what the boiler was actually told), so HCC needs no driver change.
OTGW-compat topics unchanged.

### Web UI

New "Gateway" tab: mode switch (with confirm + reboot), thermostat-online
badge, live counters, override controls (reusing WC card patterns).

## 6. Test plan

| Layer | How |
|---|---|
| Router core | Native unit tests (`pio test -e native`): policy lookup, modify-in-place, cache TTL, counter wrap |
| Single-chip loopback (M1) | Two GPIO pairs wired through two OT-Direct boards on the bench; master endpoint talks, slave decodes — proves electrical + framing |
| Bench gateway (M3) | Real thermostat ↔ HCS ↔ boiler (or boiler sim: 18 V supply + OT-Direct + second ESP acting as boiler) |
| Regression | CI builds all envs; master-only envs must produce byte-identical behaviour (router compiled out without `HCS_GW_ENABLE`) |

## 7. Milestones

| M | Deliverable | Exit criteria |
|---|---|---|
| M0 | This doc reviewed | Agreement on pins + scope |
| M1 | `ot_slave` + bench loopback | Frames decoded from thermostat bus ≥99 %, serial log shows raw MsgIDs |
| M2 | Router core + unit tests; `FORWARD`-only gateway behind `HCS_GW_ENABLE` | Thermostat operates boiler transparently for hours; CI green |
| M3 | Overrides (setpoint first) via MQTT/web | Override applies <1 s, releases cleanly, survives boiler brown-out test |
| M4 | Docs + HA sidebar polish | flash.md/hardware.md updated; OTA'd to test rig |

## 8. Explicit non-goals (v0.3)

- PIC/OTGW hex emulation (never — GPL wall).
- Multi-boiler / zone-valve logic.
- Wireless thermostat pairing tricks.
