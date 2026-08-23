# OptoLink / KM-BUS protocol branch (Viessmann)

**Status:** research scaffold — no code yet.
**Target brands:** Viessmann Vitodens / Vitotronic family — **including the
owner's Vitodens 100-W B1KF**, which exposes exactly this interface.

## Protocol facts

| | |
|---|---|
| Physical | **optical** IR link (SFH487-2 emitter + SFH309FA receiver) over the service jack |
| Speed | 4800 baud, 8E2 (some ESP32-S3 boards need 5040 to keep timing) |
| Protocols | **KW (VS1)** legacy, **P300 (VS2)** modern default, GWG old |
| Sync | device sends `0x05` every ~1 s; telegrams answered only right after |
| Read frame | `01 F7 addrH addrL len` → response `01 F7 …payload…` |
| Write frame | `01 F4 addrH addrL len data…` → echo + return code |
| Identity | addresses `0x00F8..0x00FB` = device id/type/hw/sw version |

Known datapoints for the B1KF-class units (from vcontrold/vitohome configs):
kesseltemp `0x0810`, ruecklauf `0x0814`, aussentemp `0x0800`, HW temp
`0x0804`, brennerstarts `0x088A`, heizkurve slope `0x27D3`, betriebsmodus
`0x2323`, error history `0x7507+`.

## Hardware interface

~€5 of parts: IR diode pair + two resistors on any UART (circuit in the
VitoWiFi README). No bus electrical risk at all — purely optical, read-only
by nature unless we choose to emit.

Reference implementations:
- bertmelis/**VitoWiFi** — ESP8266/ESP32 Arduino library, VS1/VS2/GWG,
  non-blocking, PlatformIO registry ✓ drop-in candidate for our link layer
- openv/**vcontrold** + wiki — datapoint database per Vitotronic model
- SoulSolistice/esphome_vitohome & dannerph/esphome_vitoconnect — ESPHome proofs

## Integration plan for HCS

1. `OptolinkLink` implementing `BoilerLink` on top of **VitoWiFi** (MIT).
2. v1: passive reads of the table above → existing telemetry/MQTT/HCC.
   Weather comp gets a real outdoor source on Viessmanns that lack one.
   Return-temp gap (our B1KF) can also be filled from `0x0814` if present!
3. v2: writes (flow setpoint `0x2700-ish`, heating curve, DHW) behind the
   same bounds/failsafe layers as OpenTherm.
4. Bonus: error-history datapoints map straight into our diagnostics page.

## Effort estimate

- Passive monitoring: ~1–2 weekends (library does the heavy lifting)
- Writes + model validation: +1–2 weekends with hardware in hand

## Risks

- Optical coupling alignment is finicky; tape jig recommended
- Protocol variant must match controller generation (detect via `0xF8`)
