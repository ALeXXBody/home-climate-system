# Flash & first boot

## Method 1: PlatformIO (recommended)

### Prerequisites

- [PlatformIO CLI](https://platformio.org/install/cli) or VS Code + PlatformIO extension
- USB cable connected to the ESP board

### Build and flash

```bash
cd firmware

# Build only (check compilation)
pio run -e lolin_s2_mini

# Build + flash + monitor
pio run -e lolin_s2_mini -t upload
pio device monitor -b 115200
```

### Available build environments

| Environment | Board | Notes |
|---|---|---|
| `d1_mini` | ESP8266 D1 mini | Basic master |
| `lolin_s2_mini` | ESP32-S2 | Primary ESP32 |
| `lolin_c3_mini` | ESP32-C3 | Direct fitment |
| `esp32_d1_mini` | ESP32 | Classic ESP32 |
| `esp32s3_zero` | ESP32-S3 | Jumper wires |
| `lolin_s2_mini_gw` | ESP32-S2 gateway | Dual OT |
| `esp32_d1_mini_gw` | ESP32 gateway | Dual OT |
| `lolin_c3_mini_gw` | ESP32-C3 gateway | Dual OT |

See [Boards & builds](Boards-and-builds.md) for full pin maps.

## Method 2: Web flasher (Chrome/Edge only)

1. Open [docs/flasher/index.html](https://github.com/ALeXXBody/home-climate-system/blob/main/docs/flasher/index.html) from the repo (or host locally)
2. Select your board type
3. Connect the ESP via USB
4. Click **Flash** — uses ESP Web Tools (Web Serial API)
5. Works with **D1 mini** and **Lolin C3 mini** out of the box

**Browser requirement:** Chrome or Edge on desktop (Web Serial API). Does not work on mobile or Firefox.

## Method 3: OTA (from HCC or board web UI)

After the first flash, future updates can be done over-the-air:

- **HCC Devices tab** — select board, pick firmware version, flash
- **Board web UI** — System tab → paste OTA URL → Flash

See [OTA updates](OTA.md) for details.

## First boot

After flashing, the board boots into setup mode:

### 1. Join the captive portal

On your phone or laptop, connect to WiFi:

| SSID | Password |
|---|---|
| `HCS-Setup-XXXX` | `homeclimate` |

(`XXXX` = last 4 hex chars of the MAC address)

If the portal doesn't pop up automatically, browse to `http://192.168.4.1` or `http://captive.apple.com`.

### 2. Configure WiFi + MQTT

In the portal:

| Field | What to enter |
|---|---|
| WiFi SSID | Your home WiFi network name |
| WiFi password | Your home WiFi password |
| MQTT broker host | e.g. `192.168.50.100` or `mqtt.local` |
| MQTT port | Default `1883` |
| MQTT username | Your MQTT broker username (if any) |
| MQTT password | Your MQTT broker password (if any) |
| MQTT prefix | Default `hcs` (matches HCC default) |
| Device name | e.g. "Boiler Gateway" (optional) |
| OTA password | Password for OTA updates (optional) |

### 3. Note the node ID

After connecting to MQTT, the board prints its identity on serial:

```
node_id: hcs-aabbccddeeff
```

Use this node ID when setting up the HCC integration in Home Assistant.

You can also find it in:
- MQTT explorer: `hcs/discovery/#`
- Router DHCP table
- Board web UI: System tab

### 4. Verify in Home Assistant

1. Open **Home Climate** sidebar in HA
2. Go to **Devices** tab
3. The board should appear in the board selector
4. Check the **Home** tab — boiler link pill should be green

## Serial monitor

For debugging, connect USB and run:

```bash
pio device monitor -b 115200
```

You'll see:
- Boot sequence and reset reason
- WiFi connection status
- MQTT connect/reconnect events
- OpenTherm transactions
- Node ID and IP address
- OTA progress
- Failsafe state changes
