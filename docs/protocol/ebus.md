# eBUS protocol branch (Vaillant group)

**Status:** research scaffold — no code yet.
**Target brands:** Vaillant (ecoTEC, ecoCOMPACT), Protherm, Baxi (eBUS
variants), Saunier Duval, Hermann, and any boiler with an eBUS terminals.

## Protocol facts

| | |
|---|---|
| Physical | 2-wire half-duplex bus, ~24 V idle, current-modulated |
| Speed | 2400 baud class; symbol-level arbitration (SS/RA) |
| Framing | master→slave command (QQ,PBSB,Z1..Zn) + slave ACK + response |
| Addressing | 8-bit addresses; masters 0x00–0x7F, slaves 0x80–0xFF |
| Discovery | broadcast scan (`ZZ FB 00 07 04`), devices report `MF=…;ID=…;SW=…;HW=…` |
| Clocking | some boilers need an external SYN generator (`--generatesyn`) |

Data model is CSV-driven in the ebusd ecosystem (per-device definition files)
— a huge existing knowledge base we can reuse rather than reverse-engineer.

## Hardware interface (ESP32)

Proven design: **ebusd-adapter v5** (adapter.ebusd.eu) — ESP32-C3 based,
D1-mini-shield compatible, galvanically isolated, handles eBUS arbitration in
firmware. Simple DIY version: opto/transistor pair on any UART pin.

Key reference implementations:
- john30/**ebusd** (daemon, CSV definitions)
- john30/**ebusd-esp32** (enhanced protocol firmware)
- **micro-ebusd** — full interpreter running ON the ESP32, publishing MQTT

## Integration plan for HCS

1. New `BoilerLink` interface so `ot_master.cpp` becomes one implementation of
   several (`OpenThermLink`, `EbusLink`, …). Gateway/failsafe/WC/sensors stay
   protocol-agnostic above it.
2. `EbusLink` v1 scope: passive monitoring (status, flow/return temps, DHW,
   pressure equivalents from the CSV set) — read-only, lowest risk.
3. v2: write support for flow setpoint / DHW via standard PBSBs
   (`0500`, `0B09` families), reusing our bounds-clamp + failsafe layers.
4. Reuse HCS web/MQTT/HCC surfaces unchanged — only the link layer swaps.

## Effort estimate

- Link skeleton + passive decode: ~2–3 weekends
- Write path + validation against a real Vaillant: ~2 more

## Risks

- Arbitration timing needs bit-level UART control or the proven adapter HW
- Some installations lack SYN → must implement generator mode
