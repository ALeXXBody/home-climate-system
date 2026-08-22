/**
 * Home Climate System — OpenTherm master firmware
 *
 * - Captive portal (WiFiManager) for WiFi + MQTT
 * - HTTP status + ElegantOTA + ArduinoOTA
 * - MQTT native hcs/ + OTGW-compat for Home Climate Control
 */

#include <Arduino.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

#include "config.h"
#include "ot_master.h"
#include "mqtt_bridge.h"
#include "settings_store.h"
#include "net_services.h"

static OtMaster ot(OT_IN_PIN, OT_OUT_PIN);
static WiFiClient wifiClient;
static MqttBridge mqtt(wifiClient, ot);
static NetServices net(ot);
static SettingsStore store;
static HcsSettings settings;
static String nodeId;

static String makeNodeId() {
  String m = WiFi.macAddress();
  m.replace(":", "");
  m.toLowerCase();
  return "hcs-" + m;
}

static void onOtaUrl(const String& url) {
  Serial.printf("[mqtt] OTA URL received: %s\n", url.c_str());
  net.startHttpUpdate(url);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println(F("=== Home Climate System ==="));
  Serial.printf("FW %s  board %s  OT_IN=%d OT_OUT=%d\n", HCS_FW_VERSION,
                HCS_BOARD_NAME, OT_IN_PIN, OT_OUT_PIN);

#if CH_FAILSAFE_OFF_ON_BOOT
  ot.setChEnable(false);
#else
  ot.setChEnable(DEFAULT_CH_ENABLE);
#endif
  ot.setDhwEnable(DEFAULT_DHW_ENABLE);
  ot.setFlowSetpoint(DEFAULT_FLOW_SETPOINT_C);
  ot.setMaxModulation(DEFAULT_MAX_MODULATION);
#ifdef HCS_TEST_BOOT
  // Simulated OT bus is an unterminated floating wire — bias it stable
  pinMode(OT_IN_PIN, INPUT_PULLUP);
  pinMode(OT_OUT_PIN, OUTPUT);
  Serial.println(F("[dbg] pre ot.begin"));
#endif
  ot.begin();
#ifdef HCS_TEST_BOOT
  Serial.println(F("[dbg] post ot.begin"));
#endif

  store.begin();
  if (!store.load(settings)) {
    // Optional compile-time seeds
    settings.wifi_ssid = WIFI_SSID;
    settings.wifi_pass = WIFI_PASS;
    settings.mqtt_host = MQTT_HOST;
    settings.mqtt_port = MQTT_PORT;
    settings.mqtt_user = MQTT_USER;
    settings.mqtt_pass = MQTT_PASS;
    settings.mqtt_prefix = MQTT_PREFIX;
    settings.otgw_node = OTGW_COMPAT_NODE;
    settings.configured = settings.wifi_ssid.length() > 0;
  }
#ifdef HCS_TEST_BOOT
  Serial.println(F("[dbg] post store"));
#endif

#ifdef HCS_TEST_BOOT
  // Headless simulation (Wokwi/CI): skip blocking portal + network services
  Serial.println(F("[wifi] TEST_BOOT: skipping WiFi portal"));
  Serial.println(F("[http] TEST_BOOT: skipping HTTP/MQTT services"));
  nodeId = makeNodeId();
  Serial.printf("[boot] node %s ready (test mode)\n", nodeId.c_str());
#else
  if (!net.beginWifi(settings)) {
    Serial.println(F("[boot] no WiFi — restarting in 10s"));
    delay(10000);
    ESP.restart();
  }

  nodeId = makeNodeId();
  net.beginHttp(settings, nodeId);
  net.beginArduinoOta(settings, nodeId);

  mqtt.setNodeId(nodeId);
  mqtt.setDeviceInfo(settings.device_name, net.localIp(), settings.otgw_node);
  mqtt.onOtaUrl(onOtaUrl);

  if (settings.mqtt_host.length()) {
    mqtt.begin(settings.mqtt_host.c_str(), settings.mqtt_port,
               settings.mqtt_user.c_str(), settings.mqtt_pass.c_str());
    Serial.printf("[mqtt] broker %s:%u node %s\n", settings.mqtt_host.c_str(),
                  settings.mqtt_port, nodeId.c_str());
  } else {
    Serial.println(F("[mqtt] no broker configured — portal MQTT host empty"));
  }

  Serial.println(F("[boot] ready — open http://device-ip/ for status & OTA"));
#endif
}

void loop() {
#ifdef HCS_TEST_BOOT
  static unsigned long last_tick = 0;
#endif
  ot.loop();

#ifndef HCS_TEST_BOOT
  net.loop();

  if (WiFi.status() != WL_CONNECTED) {
    // brief reconnect attempt; portal not re-opened automatically
    static unsigned long last = 0;
    if (millis() - last > 15000) {
      last = millis();
      WiFi.reconnect();
    }
  } else if (settings.mqtt_host.length()) {
    // keep IP fresh for discovery
    mqtt.setDeviceInfo(settings.device_name, net.localIp(), settings.otgw_node);
    mqtt.loop();
  }
#endif

  ot.poll();

#ifdef HCS_TEST_BOOT
  if (millis() - last_tick >= 2000) {
    last_tick = millis();
    Serial.printf("[tick] uptime=%lus\n", millis() / 1000);
  }
#endif
  delay(1);
}
