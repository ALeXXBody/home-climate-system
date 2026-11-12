# Changelog

## v1.5.x

The 1.5 line (2026-10 → 2026-11) hardened the board rather than adding user
features: web-UI authentication, signed firmware verification on every board
including ESP8266, an honest image-size budget, a learned CH ceiling, an MQTT
session that cannot silently die, and pull-based self-update that does not
depend on MQTT at all.

| Version | Change |
|---|---|
| [v1.5.32](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.32) | A missed outdoor or return read keeps the last temperature instead of publishing nothing |
| [v1.5.31](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.31) | ESP8266 collects Origin and Cookie through the variadic header API; the array form is ESP32-only |
| [v1.5.30](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.30) | Pull-based self-update: the board fetches the hub's `latest.json` over plain LAN HTTP and applies a newer signed release itself (6 h cadence, deferred during a burn or OTA); MQTT `ota_url` stays the instant fallback |
| [v1.5.29](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.29) | `/api/status` exposes `mqtt_connected` and `mqtt_outbound_ok`, so a one-way MQTT death is readable over HTTP |
| [v1.5.28](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.28) | Session rebinding by maximum age (6 h) instead of the 90 s inbound-decay trigger, which would have reconnect-looped on quiet nights |
| [v1.5.27](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.27) | Zombie-session guard: keepalive 30 s, rebind when no inbound frame lands for 90 s |
| [v1.5.26](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.26) | The learned CH ceiling also travels in the retained `ctl` mirror as `tset_cap` |
| [v1.5.25](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.25) | Learned boiler CH ceiling: three persistent TSet write-ACK echoes latch a real ceiling; writes clamp to it, with a ~5 min unclamped probe so a re-configured boiler lifts it |
| [v1.5.24](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.24) | OTA size budget made honest: unused exceptions/RTTI dropped (~80 KB), a per-env CI image budget, and a readable `Update.begin` failure |
| [v1.5.23](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.23) | Pinned toolchain, ESP8266 joins the signed-OTA chain, the OT slave latch becomes a tested ring, DHW setpoint clamped at the command boundary |
| [v1.5.22](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.22) | Deinit and re-init the watchdog (reconfigure exists only in newer IDF minors) |
| [v1.5.20](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.20) | Publish `boiler_diag` and `failsafe` every tick instead of change-gated |
| [v1.5.19](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.19) | Disabling authentication no longer requires a login |
| [v1.5.18](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.18) | OTA rollback watchdog: an OT link counts as healthy, plus a health latch |
| [v1.5.17](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.17) | Signed OTA: ECDSA P-256 image verification with streamed download |
| [v1.5.16](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.16) | Escape `device_name` in discovery, subscribe `set`/`led`, sanitise the node id |
| [v1.5.15](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.15) | In-page login form with a session cookie and logout |
| [v1.5.14](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.14) | Web auth flow: create password → login, with an enable/disable toggle |
| [v1.5.13](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.13) | Secure by default: a MAC-derived admin password plus recovery AP and update auth |
| [v1.5.12](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.12) | Security: exact-origin CSRF guard, OTA URL validation, `/api/ota` origin check |
| [v1.5.11](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.11) | Gateway link self-heal, retained-command dedupe, OT parity validation, portal field fix |
| [v1.5.10](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.10) | 1-Wire pins belong to their board environments; `/api/status` and OTA auth use live settings |
| [v1.5.9](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.9) | Plain-LED blink family with active-low restore, portal secrets masked and never echoed, open setup/recovery AP |
| [v1.5.8](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.8) | C3 Mini TX 8.5 dBm, no disconnect/begin thrash, HT20 and `PS_NONE` after association |
| [v1.5.5](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.5) | Status LED toggle and brightness on the Settings page (web and MQTT), persisted |
| [v1.5.4](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.4) | Active Wi-Fi re-association, TX power and driver properties |
| [v1.5.2](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.2) | Gateway slave ISR is all-IRAM, matching the master path |
| [v1.5.1](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.1) | Wi-Fi event logging, to diagnose silent drops |
| [v1.5.0](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.5.0) | Audited stable release |

Versions `v1.5.3`, `v1.5.6`, `v1.5.7` and `v1.5.21` were never tagged.
`v1.5.3` exists as a GitHub release with no matching tag; the others were
superseded inside the same day.

## v1.4.8

**Release:** [GitHub](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.4.8)

### Real 1.4.7 boot crash (addr2line)
Serial dump showed `abort()` in `rmt_driver_install` ← `neopixelWrite` ← `StatusLed::write` ← `setup()` **before Wi‑Fi**.

- **Removed WS2812/RMT status LED** on C3 (and S3-Zero). Plain GPIO blink only; RGB flag ignored.
- Boot order: OT safe-mode decision before LED
- Cap/clear unclean_boots after 90 s stable so safe-mode can exit

## v1.4.7

**Release:** [GitHub](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.4.7)

### Unbrick ESP32-C3 (1.4.6 dead-on-LAN)
- **1.4.6 bug:** `handleInterruptIsr()` was not `IRAM_ATTR` — ISR jumped to flash → immediate PANIC loop before Wi‑Fi (no ping)
- **Fix:** full IRAM ISR chain; idle bus before attach
- **OT safe-mode:** after ≥3 unclean boots, skip OpenTherm so Wi‑Fi/HTTP/OTA still work

**Recovery:** USB flash `firmware-lolin_c3_mini.bin` (OTA impossible while dead).

## v1.4.6

**Release:** [GitHub](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.4.6)

### Fix ESP32-C3 PANIC reboot loop
- **Root cause:** OpenTherm library `begin()` used a C++ `std::function` lambda ISR (`FunctionalInterrupt`). On ESP32-C3 that panics under OT bus traffic + Wi‑Fi.
- **Fix:** static `IRAM_ATTR` C ISR for master (and gateway slave) — no heap/`std::function` in interrupt context
- WS2812 status LED rate-limited + IRQs masked only during the short bit-bang
- Panic breadcrumbs in RTC: next boot Log shows `panic last mark: …` (where the crash happened)
- System tab shows panic mark next to reset reason

## v1.4.5

**Release:** [GitHub](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.4.5)

### Always publish MQTT telemetry (HCC "Boiler disconnected" fix)
- Telemetry (online, flame, temps including 1-Wire outdoor/return) is published every cycle even when OpenTherm Status is down
- New retained/live `ot_valid` topic so HCC can show "OT not linked" separately from board MQTT offline

## v1.4.4

**Release:** [GitHub](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.4.4)

### Stop reboot loop + system Log console
- **HTTP self-probe no longer reboots** — it was connecting to itself from the same superloop that serves HTTP, false-failing, and scheduling `SW_RESET` about every 1–2 minutes
- Probe is diagnostic only (logs fail/OK every 2 min)
- New **Log** tab + `GET /api/log` — boot reason, OT link up/down, scheduled reboot reason, HTTP probe, OTA rollback
- Every `scheduleReboot` records a reason shown in System + Log
- MQTT reboot ignores empty payloads (no retained-edge surprise)

## v1.4.3

**Release:** [GitHub](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.4.3)

### Priority: boiler first, 1-Wire backfill only
- **OpenTherm outdoor/return always wins** when the boiler reports a valid value
- 1-Wire probes assigned as **outdoor** / **return** are used **only** when OT did not provide that channel this cycle
- Snapshot flags `outdoor_from_ot` / `return_from_ot`; Sensors tab “effective” source shows `opentherm` vs `sensor`

## v1.4.1

**Release:** [GitHub](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.4.1)

### Fixes
- **OT console MsgID labels** were wrong (e.g. ID 25 shown as `TboilerUB` instead of `Tboiler`, ID 18 as `Tout` instead of CH pressure). Console looked full of “errors” when the boiler simply had no outdoor/return probe.
- Outdoor OT read: only accept a real READ-ACK; leave field alone on T/O / UNK-ID so **1-Wire outdoor** can fill it.
- Return OT read: leave NaN on failure so **1-Wire return** inject is not wiped by a failed OT frame.
- Payload formatting: flag/OEM IDs print hi/lo, not a fake °C float.

### Sensors (unchanged behaviour, clarified)
- 1-Wire role **outdoor** (Tout) → snapshot + MQTT `outdoor_temp` + weather compensation
- 1-Wire role **return** (Tret) → snapshot + MQTT `return_temp` (HCC gas ΔT / condense)
- HCC uses those MQTT values as the boiler outdoor/return source

## v1.4.0

**Release:** [GitHub](https://github.com/ALeXXBody/home-climate-system/releases/tag/v1.4.0)

### Fixes
- ESP32 weather-comp NVS persistence (`wc_en`, `wc_ref`, `wc_dsn`, `wc_fmax`, `wc_fmin`) — saved every 5 s at runtime
- LittleFS OTA rollback: mount before pending load; rollback watchdog (90 s confirm / 180 s revert / max 3 attempts)
- HTTP `/api/control` accepts `dhw_setpoint` (was missing)
- OT invalid 0 °C guard: only accepts temps >0 on SUCCESS frames
- `/api/reboot` requires auth (password check)
- `referencePoll` includes DHW enable bit
- `LittleFS.begin()` portable for ESP8266 (no format argument)

### Improved
- 1-Wire inject re-applied after return-temp read (prevents stale return)
- Boiler diagnostics text expanded (ASF flags + OEM code)
- HTTP self-probe: 2 consecutive failures → reboot (catches stuck web layer)
- Unique captive portal AP name per device (`HCS-Setup-XXXX`)

## v1.3.4

### Fixes
- Two-way settings sync via retained `ctl` topic
- OTA progress reporting over MQTT
- LAN HTTP OTA URL support
- DHW setpoint bounds clamped to boiler-reported limits

### Improved
- Slow-read rotation for boiler capabilities (pressure, member IDs, capacity, bounds)
- Fault history buffer: once per boot + hourly
- Status LED state machine (WiFi/OT/failsafe)

## v1.3.2

### Fixes
- OT Status must be READ_DATA (not WRITE_DATA) — regression fix
- Modulation read reliability
- WiFi reconnection stability

### Improved
- MQTT reconnect exponential backoff (5 s → 60 s max)
- 1-Wire self-test at boot (blocking conversion for immediate health)

## v1.3.0

### Features
- Weather compensation curve (outdoor → flow)
- Captive portal with WiFiManager
- ElegantOTA + ArduinoOTA
- Device web UI (single-page, dark theme)
- 1-Wire DS18B20 probe detection + role assignment
- Failsafe state machine (hold → failsafe on link loss)
- MQTT telemetry (2 s cadence) + discovery (30 s cadence)

## v1.2.0

### Features
- Basic OpenTherm master (CH/DHW enable, flow setpoint, modulation)
- WiFi + MQTT
- HTTP API (`/api/status`, `/api/control`)
- OTA via HTTP URL
- NVS settings persistence

## Earlier

- Initial firmware with OT master and MQTT bridge
- WiFi portal setup
- Basic web UI

Full release history: [GitHub Releases](https://github.com/ALeXXBody/home-climate-system/releases)
