# Changelog highlights

## v1.4.0

- ESP32 weather-comp NVS save (wc_en/ref/dsn/fmax/fmin)
- LittleFS OTA rollback mount + pending load after reboot
- HTTP `dhw_setpoint` on `/api/control`
- OT invalid 0 °C guard; 1-Wire inject after return read
- `/api/reboot` requires auth
- referencePoll DHW enable bit
- ESP8266-safe `LittleFS.begin()`

## Earlier (1.2.x – 1.3.x)

Two-way settings, OTA progress, 1-Wire roles, DHW MQTT, LAN HTTP OTA.

Full history: [GitHub Releases](https://github.com/ALeXXBody/home-climate-system/releases)
