# Changelog

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
