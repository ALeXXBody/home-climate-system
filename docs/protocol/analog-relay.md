# Analog / relay fallback branch (universal boiler support)

**Status:** research scaffold — no code yet.
**Target:** every boiler, including the dumbest ones — via the two analog
interfaces that have existed for decades: **on/off thermostat contacts** and
**0–10 V / PWM flow-setpoint inputs**.

## Interface facts

| Mode | Wiring | Behaviour |
|---|---|---|
| On/off (dry contact) | relay across TT terminals | CH demand binary; boiler runs its own setpoint |
| 0–10 V | analog output + GND | continuous flow-setpoint command; most combis with "0-10V" jumper scale 0–80 °C |
| PWM | some Viessmann/Elco | duty-cycle encodes target; vendor-specific curves |

Telemetry in these modes is minimal to none — pair with our **1-Wire DS18B20
stack** for flow/return/outdoor readings, exactly the Sensors-tab roles we
already ship.

## Hardware interface

- Relay: any opto-isolated MOSFET/relay shield
- 0–10 V: PWM through RC filter + op-amp buffer (or MCP4725 DAC), or a small
  boost from 3.3 V PWM since many boilers accept ≥6 V logic as "10 V"

## Integration plan for HCS

1. `AnalogLink` implementing `BoilerLink`:
   - mode `onoff`: maps CH demand → relay; flow setpoint meaningless (hide in UI)
   - mode `analog`: maps effective flow target (incl. weather comp!) to volts,
     with per-boiler min/max calibration values in settings
2. Failsafe layer works unchanged — it already forces CH + fixed flow.
3. Telemetry comes from DS18B20 probes; diagnostics limited to what sensors see.

## Effort estimate

- onoff: ~1 day · analog: ~2–3 days incl. calibration UI

## Risks

- Lowest data richness of all links — set expectations in UI copy
- 0-10V scaling must be calibrated per boiler model (settings wizard step)
