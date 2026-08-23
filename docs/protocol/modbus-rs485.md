# Modbus RTU / RS485 protocol branch (Nibe, commercial boilers & heat pumps)

**Status:** research scaffold — no code yet.
**Target brands:** Nibe (MODBUS40 accessory + S-series native Modbus TCP),
many commercial/light-commercial boilers, and most heat pumps with an
RS485 service port. Also the generic fallback for anything speaking
industry-standard Modbus.

## Protocol facts

| | |
|---|---|
| Physical | RS485 differential pair (A/B), half-duplex; GND reference recommended |
| Speed | typically 9600 8N1 (Nibe MODBUS40); vendor-specific elsewhere |
| Model | standard Modbus RTU: slave id, function codes 03/04/06/16 |
| Nibe specifics | pump streams up to 20 registers per telegram and **must be ACKed** (`MODBUS40` emulation) or it raises an alarm; S-series exposes plain **Modbus TCP :502** natively; "word swap" option matters for 32-bit values |
| Register maps | vendor files: Nibe `LOG.SET` via their Modbus Manager tool |

## Hardware interface

Any MCU + RS485 transceiver. Auto-direction chips (MAX3485) avoid the DE/RE
GPIO; proven boards: LilyGo T-CAN485, M5Stamp PLC, ProDino.

Reference implementations:
- elupus/**esphome-nibe** (UDP gateway pattern, ACK handling)
- nptr/**nibegw-esp** (ESP-IDF Modbus-TCP bridge)
- yozik04/**nibe-mqtt** (register naming + HA discovery)
- HA core `nibe_heatpump` integration (TCP/RCU)

## Integration plan for HCS

1. `ModbusLink` implementing `BoilerLink`:
   - RTU mode over UART (+DE pin or auto chip) for wired units
   - TCP mode for S-series/native-Ethernet units (reuse our WiFiClient!)
2. Register map as a **data file**, not code — one JSON per model family,
   loaded at boot (keeps adding brands trivial).
3. v1 read-only telemetry → existing surfaces. v2 writes for setpoints.
4. Nibe ACK state machine is the only tricky part — port from esphome-nibe.

## Effort estimate

- RTU skeleton + one validated model: ~2 weekends
- TCP mode + second model family: +1 weekend

## Risks

- Wrong ACK behaviour can alarm a Nibe — implement carefully behind a
  "passive listen" first phase (listen without ACKing is safe)
- Register semantics differ per firmware generation — rely on LOG.SET exports
