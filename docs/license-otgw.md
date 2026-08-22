# OTGW-firmware license research

Date: 2026-08-22  
Source: https://github.com/rvdbreemen/OTGW-firmware  

## Verdict

| Item | Value |
|---|---|
| License | **GNU GPL v3.0** |
| SPDX | `GPL-3.0` |
| Code reuse in Home Climate System (MIT)? | **Forbidden** |
| Studying behaviour / MQTT naming / OT IDs? | Allowed (ideas only) |

## Why this matters

Home Climate Control and Home Climate System are intended to ship under **MIT**.

GPL-3.0 is a strong copyleft license. If we:

- copy OTGW-firmware source files, or  
- link against its code, or  
- create a derivative work by substantial code reuse,

then our combined work must be distributed under GPL-3.0 (or compatible),
which conflicts with the MIT plan.

## What we may do

- Read the OTGW-firmware docs and source to understand OpenTherm gateway
  behaviour, MQTT topic layouts, and command names.
- Re-implement the same *external* behaviour (MQTT subjects, CS/MM/CH-style
  commands, outdoor temp telemetry) in original code.
- Use the official OpenTherm specification and public protocol docs.
- Use MIT/Apache/BSD libraries (e.g. some OpenTherm Arduino ports — verify
  each library’s license before adding it).

## What we must not do

- Copy `.ino` / `.cpp` / `.h` from OTGW-firmware into this tree.
- Fork OTGW-firmware and strip the GPL notice.
- Vendor their MQTT or OpenTherm stacks without a full GPL compliance plan
  (not desired for this product).

## Practical MQTT target (behavioural compatibility)

Home Climate Control’s first backend already speaks OTGW-firmware-style MQTT
so existing gateways work today. Home Climate System firmware may:

1. **Speak a compatible subset** of those topics (easier migration), and/or  
2. **Define `hcs/` topics** documented in `protocol/` and add a dedicated
   backend in Home Climate Control.

Either path is fine as long as the *implementation* is original.

## Sign-off

Any PR that adds third-party firmware code must update this file with the
dependency name, license SPDX, and why it is MIT-safe.
