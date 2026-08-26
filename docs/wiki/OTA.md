# OTA updates

Update firmware over-the-air without USB cables.

## Method 1: From HCC panel (recommended)

1. Open **Home Climate** sidebar → **Devices** tab
2. Select the board from the dropdown
3. The firmware catalog shows available versions from GitHub releases
4. Pick a version → click **Flash**
5. Progress is shown in the panel
6. Board reboots automatically after flashing

## Method 2: From board web UI

1. Navigate to the board's IP → **System** tab
2. Paste a firmware URL into the OTA field:
   ```
   http://192.168.50.200:8000/firmware/lolin_s2_mini
   ```
   or a direct URL:
   ```
   https://github.com/ALeXXBody/home-climate-system/releases/download/v1.4.0/firmware-lolin_s2_mini.bin
   ```
3. Click **Flash**
4. Progress shows in the OT console area

## Method 3: From MQTT

```bash
mosquitto_pub -t "hcs/<node>/set/ota_url" \
  -m "http://192.168.50.200:8000/firmware/lolin_s2_mini"
```

Progress publishes to `hcs/<node>/ota` as JSON:
```json
{"state":"downloading","progress":45}
{"state":"rebooting"}
```

## Method 4: PlatformIO (USB)

```bash
cd firmware
pio run -e lolin_s2_mini -t upload
```

Always works, even if OTA is broken.

## OTA rollback protection

The firmware includes a **LittleFS-backed rollback watchdog**:

### How it works

1. **Before flash:** target URL saved to LittleFS `/otaroll.json`
2. **After boot:** waits 90 seconds for MQTT to connect
3. If MQTT connects → image promoted to **known-good** (rollback disabled)
4. If MQTT never connects within 180 seconds → **auto-reverts** to last known-good image
5. Max 3 revert attempts; then gives up

### Why it matters

If a bad firmware is flashed (compilation error, wrong board, regression), the board automatically reverts to the previous working version. This prevents bricking.

### Status

- `hcs/<node>/failsafe` shows `ON` during rollback
- Serial monitor shows rollback attempts
- Board web UI shows current firmware version

## Troubleshooting OTA

### OTA fails to start

- Board must be on the same network as the server
- URL must be accessible (no firewall blocking)
- HTTPS URLs are supported (TLS)
- Check serial monitor for error details

### OTA starts but board doesn't come back

- Rollback watchdog should auto-revert after 180 s
- If not, power-cycle and check serial monitor
- Re-flash via USB if needed

### OTA keeps reverting

- The new firmware isn't connecting to MQTT within 180 s
- Check MQTT broker is running and accessible
- Check board's WiFi signal
- Check serial monitor for MQTT reconnect attempts

## Version compatibility

- OTA works across major versions (e.g. 1.2.x → 1.4.0)
- NVS/EEPROM settings are migrated forward automatically
- ESP8266 EEPROM magic is versioned (v3–v7) for safe upgrades
- Gateway builds (`*_gw`) should only be OTA'd to gateway-capable boards
