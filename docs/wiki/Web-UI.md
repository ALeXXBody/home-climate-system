# Web UI

The HCS board runs a built-in web server on port 80. Navigate to the board's IP address in any browser.

> The web UI is embedded in firmware — no files to upload or SD card needed.

## Tabs

### Status tab (auto-refreshes every 3 s)

Live boiler state:

| Field | Source |
|---|---|
| Flame | ON/OFF (OT MsgID 5) |
| CH active | ON/OFF (OT MsgID 0) |
| Fault | ON/OFF (OT MsgID 5 ASF bits) |
| Flow temp | °C (OT MsgID 1) |
| Return temp | °C (OT MsgID 25) |
| Outdoor temp | °C (OT MsgID 27 or 1-Wire) |
| Modulation | % (OT MsgID 17) |
| Flow setpoint | °C (last commanded) |
| CH pressure | bar (OT MsgID 18) |
| Boiler diagnostics | Human-readable text (ASF flags + OEM code) |
| Boiler identity | Member IDs, config, capacity |

### Controls tab

Adjust boiler parameters in real time:

| Control | Type | Notes |
|---|---|---|
| CH on/off | Toggle | Enables/disables central heating |
| DHW on/off | Toggle | Enables/disables domestic hot water |
| DHW setpoint | Number | °C (clamped to boiler-reported bounds) |
| Flow setpoint | Number + slider | °C (clamped to MaxTSetUB/LB when known) |
| Max modulation | Slider | 0–100% |
| Weather comp | Toggle + inputs | On/off + reference, design, flow max, flow min °C |
| Failsafe | Toggle + inputs | Enable, flow setpoint, grace period |

**Weather compensation curve:**
- Reference temp (ref): outdoor temp where flow = flow_min (default 18 °C)
- Design temp: outdoor temp where flow = flow_max (default -10 °C)
- Flow max: highest flow setpoint (default 65 °C)
- Flow min: lowest flow setpoint (default 25 °C)

### Gateway tab (gateway builds only)

For `*_gw` firmware — manage the dual-OT gateway:

| Field | Description |
|---|---|
| Mode | `master_only` / `gateway` / `auto` |
| Thermostat online | ON/OFF — whether a wall thermostat is sending requests |
| Override setpoint | Force a flow temperature (bypasses wall thermostat) |
| Frame counters | forwarded / answered_local / modified / errors |
| Mode switch | Buttons to change mode (saves + reboots) |
| Override input | Set a forced flow temp or release |

### Sensors tab (auto-refreshes every 5 s)

1-Wire DS18B20 probe management:

| Column | Description |
|---|---|
| Address | 16-char hex (e.g. `28FF1234567890AB`) |
| Temperature | Current reading °C |
| Health | ok / disconnected / crc / implausible / stuck85 / unstable / unsupported |
| Role | dropdown: none / outdoor / return / custom |
| Custom name | For custom sensors — published to `hcs/<node>/x/<name>` |

**Actions:**
- **Enable/Disable** 1-Wire bus
- **Test** — force full re-scan + double conversion self-test
- **Assign role** — select from dropdown, save

### Settings tab

Device configuration (saves to NVS, reboots on save):

| Field | Description |
|---|---|
| Device name | Display name |
| MQTT host | Broker address |
| MQTT port | Default 1883 |
| MQTT username | Broker credentials |
| MQTT password | Broker credentials |
| MQTT prefix | Default `hcs` |
| OTA password | Password for OTA updates |

**Note:** WiFi SSID/password are set in the captive portal, not here.

### System tab

| Field | Description |
|---|---|
| Board | Board environment name |
| Firmware | Version string (e.g. `1.4.0`) |
| Node ID | `hcs-<mac>` |
| IP | Current IP address |
| RSSI | WiFi signal strength |
| Uptime | Since last reboot |
| MQTT status | Connected/disconnected, broker info |
| OT console | Last 64 OpenTherm exchanges (refresh/clear) |
| OTA from URL | Paste a firmware URL → Flash |
| Reboot | Reboot the device |

**OT console** shows every master↔boiler exchange with timestamps, message IDs, and payloads. Useful for debugging OT communication issues.

## Authentication

All mutating endpoints (POST) require the **OTA password** when one is set. The web UI prompts for it when you try to save changes.

Cross-origin (CSRF) protection blocks form submissions from other origins.

## Auto-refresh

| Tab | Refresh interval |
|---|---|
| Status | 3 seconds |
| Sensors | 5 seconds |
| Other | Manual only |
