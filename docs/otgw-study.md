# OTGW study — what a production PIC-gateway exchanges with this boiler

Field observation of an existing **NodoShop OpenTherm Gateway WiFi**
(ESP8266 + PIC second processor running [OTGW-firmware](https://github.com/rvdbreemen/OTGW-firmware)),
taken as input for HCS feature planning. Study date: 2026-08-23.
The gateway was left in its original configuration; the legacy TCP bridge was
temporarily enabled for a 5-minute capture and switched off again.

## 1. The rig under study

| | |
|---|---|
| Device | `192.168.50.21`, hostname `OTGW`, unique-id `otgw-18fe34cca876` |
| Firmware | OTGW-firmware (web UI "OTGW firmware"), MQTT → HA broker `.20` |
| Topics | top topic `OTGW`, HA discovery prefix `homeassistant`, raw OT messages on (`mqttotmessage`), publish-on-change + 60 s interval |
| Legacy bridge | port 25238 normally **disabled** |

## 2. Method

1. Read-only API: `GET /api/v2/settings` (config, masked secrets).
2. Temporarily enabled `legacyport25238enabled` via `POST /api/v2/settings`
   (`{"name":…,"value":"true"}`) and tapped the raw serial bridge.
3. Passive capture of 300 s of real thermostat↔boiler traffic
   (645 lines, incl. PIC-generated reference polls).
4. Decoded per-MsgID statistics (script preserved in PR notes).
5. Restored `legacyport25238enabled=false`; verified port closed.

## 3. What actually flows on THIS installation (5 min window)

| MsgID | Name | Observed | Notes |
|---|---|---|---|
| 0 | Status | 74× | CH on/off transitions seen; no flame, no fault during window |
| 1 | TSet | 74× | f8.8; ranged 10.0–40.0 °C (thermostat dropped setpoint mid-capture) |
| 14 | MaxRelModLevelSetting | 38× | thermostat writes **100 %** |
| 15 | MaxCapacityMinModLevel | 120× | **15 kW** capacity, **17 %** min-modulation |
| 17 | RelModLevel | 38× | modulation mostly ~0–3 % during idle period |
| 18 | CHPressure | 38× | steady **1.80 bar** |
| 25 | Tboiler | 104× | flow temp 0–45.8 °C (cool-down visible) |
| 27 | Toutside | 38× | **10.6–10.7 °C** — boiler reports outdoor temp itself |
| 56 | TdhwSetUB/LB | 38× | DHW setpoint bounds **0…51 °C** |
| 57 | MaxTSetUB/LB | 19× | CH setpoint bounds **0…55 °C** |

Not observed from their wall thermostat in-window: Tr/TrSet (IDs 24/16),
remote-parameter flags (6), fault-history buffer size (13), ASF/diag reads
(5/115) — the boiler was healthy and idle-ish. Member-ID/config exchanges
(2/3) happen at boot only.

PIC-side behaviour worth noting: OTGW-firmware runs the PIC in **gateway
reference mode** (`PR:M=G`) — it injects its own polling cycle so monitoring
stays live even when the room thermostat goes quiet (that's why Tboiler is
answered 104× while the thermostat only asked ~29 status rounds). It also
emits helper lines (`CS:` current setpoint, `PM:`, `CH:`, `MM:`).

## 4. What this means for HCS

Already covered by HCS v0.6: Status/TSet/Tboiler/Tret/Toutside/RelModLevel
telemetry, ASF + OEM-diagnostic decoding, weather comp, gateway mode with
auto-detect, OTGW-compat MQTT subjects.

### Plan — adopt (priority order)

| P | Feature | Why | Where |
|---|---|---|---|
| 1 | **CHPressure read (ID 18)** in master poll cycle + telemetry/MQTT/UI card | trivial add; valuable leak detection | `ot_master.cpp` poll, `mqtt_bridge`, web Status tab |
| 1 | **Respect setpoint bounds** from IDs 56/57 at boot: clamp UI/API flow-setpoint & DHW ranges | prevents rejected writes | `ot_master.cpp`, `net_services` controls |
| 2 | **Reference polling in gateway mode**: when thermostat silent >10 s, inject one lightweight read cycle (e.g. Tboiler/pressure/diag) per ~60 s | keeps telemetry fresh like PIC `M=G` without stealing the bus | `ot_gateway.cpp` |
| 2 | **DHW setpoint control** via remote parameter write (TdhwSet ID 56 path) with bounds check | HA-side DHW control | `ot_master`, MQTT cmd + web Controls card |
| 3 | **Member-ID/config discovery** (IDs 2/3) at boot → log + expose boiler identity/capabilities | nice diagnostics context | boot sequence |

### Not applicable (PIC-specific — deliberately skipped)

- PIC firmware flashing/version management (`PR:A/B/C/D…` upgrade flows)
- otmonitor-style persistent hex stream service
- GPIO/S0 sensor features on the ESP8266 side (HCS has its own 1-Wire stack)
- Webhook-on-flame, night-restart, graphing backend — presentation-layer,
  HA/HCC covers these better

## 5. Open questions for next bench session

- Does this boiler answer FHBsize (13)/fault-history reads? If yes, port the
  fault-history dump as part of diagnostics page.
- Remote-parameter write-enable handshake (RBPflags ID 6) needed before DHW
  setpoint writes are accepted — verify on bench before shipping P2-DHW.

## 6. Postscript — corrections & the boiler in question

**Corrections to §3 (verified against the ihormelnyk lib enum):**
ID 56 is `TdhwSet` and ID 57 is `MaxTSet` — *remote-parameter values*, not
bounds (real bounds live at IDs 48/49). So the observed 51 °C / 55 °C frames
mean the DHW setpoint is being **actively written remotely** on this
installation (by OTGW/HA), proving the boiler accepts remote-parameter
writes without an explicit RBPflags handshake. HCS therefore writes TdhwSet
(ID 56) directly, clamped by bounds read from ID 48.

**The boiler:** Viessmann **Vitodens 100-W, model B1KF**, fitted with the
optional low-power radio module (Zigbee-based OpenTherm adapter). Owner
reports ~30 kW unit configured at 15 kW max capacity — consistent with the
observed `MaxCapacity=15 kW / min-modulation 17 %` from ID 15. No return-temp
(ID 28) value reported → use a DS18B20 probe assigned as `return` in the
Sensors tab (HCS v0.5 feature) for return telemetry.
