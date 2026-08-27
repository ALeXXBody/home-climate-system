#pragma once
// Compile-time defaults. Runtime WiFi/MQTT come from captive portal (NVS).

#ifndef HCS_FW_VERSION
#define HCS_FW_VERSION "1.4.12"
#endif

#ifndef HCS_BOARD_NAME
#define HCS_BOARD_NAME "unknown"
#endif

// Optional compile-time fallbacks if portal never configured
#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASS
#define WIFI_PASS ""
#endif
#ifndef MQTT_HOST
#define MQTT_HOST ""
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_USER
#define MQTT_USER ""
#endif
#ifndef MQTT_PASS
#define MQTT_PASS ""
#endif

#ifndef MQTT_PREFIX
#define MQTT_PREFIX "hcs"
#endif


#ifndef OT_IN_PIN
#define OT_IN_PIN 4
#endif
#ifndef OT_OUT_PIN
#define OT_OUT_PIN 5
#endif

// Second (thermostat-side) interface — gateway builds only
#ifndef OT2_IN_PIN
#define OT2_IN_PIN 16
#endif
#ifndef OT2_OUT_PIN
#define OT2_OUT_PIN 17
#endif

#define OT_STATUS_INTERVAL_MS 1000
#define MQTT_RECONNECT_MS 5000
#define TELEMETRY_INTERVAL_MS 2000
#define DISCOVERY_INTERVAL_MS 30000

#define COMMAND_WATCHDOG_MS 300000
#define CH_FAILSAFE_OFF_ON_BOOT 1

#define DEFAULT_FLOW_SETPOINT_C 45.0f
#define DEFAULT_MAX_MODULATION 100
#define DEFAULT_CH_ENABLE 0
#define DEFAULT_DHW_ENABLE 1

// Captive portal AP
#define PORTAL_AP_NAME "HCS-Setup"
#define PORTAL_AP_PASS "homeclimate"  // min 8 chars; change after first boot if desired
#define WIFI_CONNECT_TIMEOUT_S 45
#define CONFIG_PORTAL_TIMEOUT_S 300

// HTTP server (status; OTA via POST /api/ota {"url":...} or ArduinoOTA)
#define HTTP_PORT 80
#define OTA_PASSWORD ""  // empty = no password; set in portal later if needed
