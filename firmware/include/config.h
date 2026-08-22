#pragma once
// Compile-time defaults. Override via build_flags or secrets.h

#ifndef HCS_FW_VERSION
#define HCS_FW_VERSION "0.1.0"
#endif

// WiFi / MQTT — copy secrets.example.h to secrets.h and edit
#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "No secrets.h — using placeholder WiFi/MQTT (device will not connect until configured)"
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASS "YOUR_WIFI_PASSWORD"
#define MQTT_HOST "192.168.1.10"
#define MQTT_PORT 1883
#define MQTT_USER ""
#define MQTT_PASS ""
#endif

// MQTT topic root (matches protocol/mqtt.md)
#ifndef MQTT_PREFIX
#define MQTT_PREFIX "hcs"
#endif

// Also publish OTGW-firmware-compatible topics so Home Climate Control
// can use its existing OTGW MQTT backend without a new driver.
#ifndef OTGW_COMPAT_PREFIX
#define OTGW_COMPAT_PREFIX "OTGW"
#endif
#ifndef OTGW_COMPAT_NODE
#define OTGW_COMPAT_NODE "hcs-device"
#endif

// OpenTherm pins (overridden per board in platformio.ini)
#ifndef OT_IN_PIN
#define OT_IN_PIN 4
#endif
#ifndef OT_OUT_PIN
#define OT_OUT_PIN 5
#endif

// Control loop
#define OT_STATUS_INTERVAL_MS 1000
#define MQTT_RECONNECT_MS 5000
#define TELEMETRY_INTERVAL_MS 2000

// Safety: if no MQTT command for this long while CH was enabled by us, keep last
// setpoint but log; CH is only forced off if never commanded after boot with
// failsafe enabled.
#define COMMAND_WATCHDOG_MS 300000
#define CH_FAILSAFE_OFF_ON_BOOT 1

// Default setpoints when nothing received yet
#define DEFAULT_FLOW_SETPOINT_C 45.0f
#define DEFAULT_MAX_MODULATION 100
#define DEFAULT_CH_ENABLE 0
#define DEFAULT_DHW_ENABLE 1
