# Flash and first boot

## PlatformIO

```bash
cd firmware
pio run -e lolin_s2_mini -t upload
pio device monitor -b 115200
```

Envs: `d1_mini`, `lolin_s2_mini`, `lolin_c3_mini`, `esp32_d1_mini`, `esp32s3_zero`, plus `*_gw`.

## First boot

1. Join WiFi AP `HCS-Setup-XXXX`
2. Set home WiFi + MQTT broker
3. Note `node_id` (`hcs-<mac>`) and IP from serial or router
4. Add the node in HCC or subscribe to `hcs/discovery/#`

## OTA

- Device web UI `/update`
- MQTT OTA URL (HCC Devices tab mirrors GitHub assets on the LAN)
- Prefer [release binaries](https://github.com/ALeXXBody/home-climate-system/releases)

## Stuck HTTP

If the board pings but port 80 hangs: power-cycle. Firmware ≥ 1.4.0 improves this path.
