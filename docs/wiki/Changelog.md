# Changelog

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
