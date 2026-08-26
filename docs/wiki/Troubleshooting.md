# Troubleshooting

## Board doesn't boot / no serial output

1. Check USB cable (data cable, not charge-only)
2. Check correct COM port in PlatformIO or serial monitor
3. Try a different USB port on your computer
4. Power-cycle the board (unplug, wait 5s, plug in)
5. Check serial monitor at **115200 baud**

## Board boots but can't connect to WiFi

1. The captive portal should auto-start: connect to `HCS-Setup-XXXX` (password: `homeclimate`)
2. If portal doesn't pop up, browse to `http://192.168.4.1`
3. If the portal times out (5 min), the board reboots and retries
4. Check WiFi SSID and password are correct
5. ESP8266 only supports 2.4 GHz networks

## Board connects to WiFi but not MQTT

1. Check MQTT broker IP/port in the portal
2. Check MQTT username/password
3. Verify the broker is running: `mosquitto_sub -t "#" -v` (should see traffic)
4. Check if the broker requires TLS (HCS uses plain TCP on port 1883 by default)
5. Serial monitor shows MQTT reconnect attempts with error codes

## Board connects to MQTT but HCC can't see it

1. Check the node ID: serial at boot prints `node_id: hcs-...`
2. Node ID in HCC setup must match exactly
3. Check MQTT prefix (default `hcs`)
4. Use MQTT explorer: look for `hcs/discovery/#`
5. Publish to `hcs/discovery/ping` to force re-discovery

## OpenTherm not communicating

1. Check OT wiring (two wires, polarity-free)
2. Verify the shield is properly seated on the ESP board
3. Check OT pin assignments match your build environment
4. Serial monitor shows OT transactions — check for errors
5. Some boilers need the OT bridge to be enabled in boiler settings

### Common OT issues

| Symptom | Likely cause |
|---|---|
| All OT reads fail | Wiring issue or shield not powered |
| Some reads fail | Normal — not all boilers support all MsgIDs |
| Flow setpoint rejected | Boiler MinTSet/MaxTSet bounds — check `slowRead` rotation |
| Outdoor temp 0 °C | Boiler doesn't report it — use 1-Wire outdoor probe |

## Web UI loads but controls don't work

1. Check if OTA password is set — you need to enter it to save changes
2. Cross-origin protection blocks form posts from other origins
3. Try accessing the board directly by IP (not through a proxy)
4. Check serial monitor for HTTP request errors

## Failsafe activates unexpectedly

1. Check WiFi signal strength (RSSI in System tab)
2. Check MQTT broker stability
3. If failsafe keeps triggering, increase the grace period
4. A stable setup should rarely enter failsafe (only during real outages)

## OTA update fails

1. Board must be on the same network as the server
2. URL must be accessible (no firewall blocking)
3. HTTPS is supported (TLS) — check certificate validity
4. Check serial monitor for OTA error details
5. If OTA keeps failing, re-flash via USB

## Board keeps rebooting

1. Check serial monitor for the crash reason
2. Brownout = power supply issue (use a better USB adapter)
3. Panic = firmware bug (try re-flashing)
4. Watchdog = main loop blocked (check for infinite loops in serial output)
5. Unclean boot count in `/api/status` shows how many consecutive bad boots

## 1-Wire sensors not detected

1. Check wiring: VCC→3.3V, GND→GND, DATA→board pin
2. **4.7 kΩ pull-up resistor** is required between DATA and 3.3V
3. Enable 1-Wire in the Sensors tab (may be disabled by default)
4. Click **Test** to force a re-scan
5. Check probe health in the Sensors tab

### Common 1-Wire issues

| Health status | Fix |
|---|---|
| `disconnected` | Check wiring, pull-up resistor, probe damaged |
| `crc` | Bad connection, try shorter wires, check solder joints |
| `stuck85` | Probe needs external power (not parasite power mode) |
| `unsupported` | Wrong sensor type — must be DS18B20 (family 0x28) |

## Gateway mode issues

1. Must use a `*_gw` firmware build
2. Two OT shields needed (master + slave)
3. Auto-detect needs ≥2 requests from the wall thermostat within 15 s
4. Check `gw/mode` in MQTT to see current mode
5. Override setpoint only works in gateway mode

## Where to find logs

- **Serial monitor:** `pio device monitor -b 115200` — most detailed
- **Board web UI:** System tab → OT console (last 64 exchanges)
- **MQTT:** subscribe to `hcs/#` with MQTT explorer
- **HCC panel:** Devices tab → board controls → diagnostics
