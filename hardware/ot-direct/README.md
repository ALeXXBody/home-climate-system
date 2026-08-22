# HCS OT-Direct — ESP32/ESP8266 → OpenTherm front-end (proposal v0.1)

**Status:** proposed, not yet fabricated. Functionally equivalent to the
well-known direct-drive OpenTherm adapter topology popularised by
[ihormelnyk/opentherm_library](https://github.com/ihormelnyk/opentherm_library)
(MIT) — the same library this firmware already uses. Original redraw, own
reference designators; no GPL OTGW-firmware material used.

## What "OTDirect" means

In [OTGW-firmware](https://github.com/rvdbreemen/OTGW-firmware) (GPL-3.0,
ideas only — see `docs/license-otgw.md`), **OTDirect** is their name for
driving the OpenTherm bus straight from ESP32 GPIOs through a small
level-shifting front-end, with **no PIC gateway and no MAX232**. This repo
already does master-mode OpenTherm in software (`ot_master.cpp`), so all that
is missing is this ~€5 front-end between MCU and boiler.

OpenTherm bus electricals (spec v2.2+): polarity-free 2-wire current loop.
Master side sits at **15–18 V idle**, bus pulled **low (< 7 V)** by modulating
current (~20 mA step) to signal bits. MCU is 3.3 V — hence an opto-coupled
front-end.

## Schematic

See [`schematic.svg`](schematic.svg). Netlist summary:

| From | To | Note |
|---|---|---|
| J1-1 | +3V3 | from MCU board |
| J1-2 | GND | common ground, MCU side = bus side |
| J1-3 | RX | MCU `OT_IN` pin (interrupt-capable); pulled up by R3 10 k |
| J1-4 | TX | MCU `OT_OUT` pin |
| TX → R4 330R → U1 LED anode; U1 K → GND | | TX opto drive |
| U1 C → R5 1k5 → Q1 base; Q1 E → VBUS+; Q1 C → GND; U1 E → GND | | PNP shunts extra loop current = bus pulled low |
| OT-A / OT-B → D1..D4 bridge → BR+ ; BR+ → R1 100R → VBUS+ | | polarity-free bus input, series limit |
| VBUS+ → D2 (15 V zener) → GND | | rail clamp |
| VBUS+ → R2 220R → D4 (4V7) → D3 (4V3) → U2 LED A; U2 K → GND | | RX threshold chain: bus high ⇒ opto on ⇒ RX low |
| U2 C → RX; U2 E → GND; R3 10 k from RX to +3V3 | | RX open-collector readback |

Polarity sense on both paths matches ihormelnyk sample wiring for his
library (`OpenTherm(inPin, outPin)` as used by `ot_master.cpp`). If you port
to another stack, verify TX/RX sense first.

Firmware pin maps already shipped in `platformio.ini`:

| Env | OT_IN | OT_OUT |
|---|---|---|
| `d1_mini` (ESP8266) | GPIO4 (D2) | GPIO5 (D1) |
| `esp32_d1_mini` | GPIO21 | GPIO22 |
| `esp32s3_zero` | GPIO5 | GPIO6 |

## BOM

See [`bom.csv`](bom.csv). All jellybean parts: PC817 ×2, BC858A, 1N4148 ×4,
zeners 15 V / 4V7 / 4V3, resistors 100R / 220R / 330R / 1k5 / 10k.

## Safety

- Boiler safeties stay authoritative; firmware boots CH-off (`CH_FAILSAFE_OFF_ON_BOOT=1`).
- Bus is SELV but keep wiring away from mains; do not hot-plug the bus.
- Test first with the boiler's service/monitor mode or a bench 18 V source +
  series resistor before attaching a live boiler.

## Roadmap

1. Breadboard validation against bench supply (18 V, 330 Ω series).
2. KiCad capture of this netlist, single-sided-friendly layout target.
3. Optional rev B: add DS18B20 outdoor-sensor port + status LED.
