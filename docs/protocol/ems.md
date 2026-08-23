# EMS protocol branch (Bosch group)

**Status:** research scaffold — no code yet.
**Target brands:** Bosch, Buderus, Nefit, Junkers, Worcester Bosch, Sieger,
elm.leblanc, iVT — anything speaking **EMS, EMS+, EMS2, Heatronic 3/4,
Junkers 2-wire**.

## Protocol facts

| | |
|---|---|
| Physical | single-wire half-duplex TTL-style serial, needs level adapter |
| Speed | 9600 baud 8N1, telegrams terminated by an 11-bit **break** signal |
| Master | the boiler (UBA, ID `0x08`) polls every device ID sequentially (~1 s cycle) |
| Our role | service-key device, ID `0x0B` (reserved for service tools) |
| Read | `[src=0x0B] R [dest|0x80]` — e.g. thermostat `0x17` → dest `0x97` |
| Write | `[src=0x0B] W [dest]`, ACK `0x01` / fail `0x04` |
| Broadcast | dest `0x00`; settings telegrams often only on change → must "fetch" |

Telegram types differ per generation (`0x18` actual-values on classic EMS vs
`0xE4` on EMS+). The EMS-ESP project documents 130+ devices with their
type-ids — reuse that knowledge base.

## Hardware interface

Tiny adapter: transistor pair + pull-up on a UART pin (service-key circuit).
Pre-built: BBQKees Electronics EMS gateway modules. Works on ESP8266 and
ESP32; ESP32 preferred for dual UART + headroom.

Key reference implementations:
- emsesp/**EMS-ESP32** — mature firmware, HA-native, MQTT + API
- German EMS wiki telegram definitions
- openhab Nibe…(n/a) — see also Heatronic docs in bbqkees wiki

## Integration plan for HCS

1. Same `BoilerLink` abstraction as the eBUS branch (see protocol/ebus.md).
2. `EmsLink` v1 scope: join polling as service key `0x0B`, decode broadcast
   set (`0x18`/`0xE4` actual values, `0x0A` UBA parameters), expose flow/
   return/DHW/outdoor + burner state through existing telemetry surfaces.
3. v2 writes: flow setpoint (`UBAParameterWW`/heating temp types) with our
   bounds/failsafe layers on top.
4. Web UI: brand auto-detect from scanned device IDs (`UBAParameter` etc.).

## Effort estimate

- Poll-join + broadcast decode: ~2 weekends
- Fetch/write cycle + validation on real boiler: ~2–3 more

## Risks

- Break-signal generation needs precise UART timing (ESP32 UART has
  break support; ESP8266 needs bit-bang fallback)
- Writing to the wrong telegram type can upset the installed thermostat —
  keep read-only until validated per model
