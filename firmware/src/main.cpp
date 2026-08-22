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

#ifdef HCS_GW_ENABLE
#if !defined(ESP32)
#error "HCS_GW_ENABLE requires ESP32 (gateway is ESP32-only)"
#endif
#include "ot_gateway.h"
#endif

static OtMaster ot(OT_IN_PIN, OT_OUT_PIN);
static WiFiClient wifiClient;
static MqttBridge mqtt(wifiClient, ot);
static NetServices net(ot);
static SettingsStore store;
static HcsSettings settings;
static String nodeId;

#ifdef HCS_GW_ENABLE
static hcs::OtGateway gw(ot, OT2_IN_PIN, OT2_OUT_PIN);
#endif

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

static void applyWcSettings(const HcsSettings& s) {
  ot.setWeatherComp(s.wc_enable);
  char cfg[48];
  snprintf(cfg, sizeof(cfg), "%.1f,%.1f,%.1f,%.1f", s.wc_t_out_ref,
           s.wc_t_out_design, s.wc_flow_max, s.wc_flow_min);
  ot.setWeatherCompCfg(cfg);
}

/** Persist WC changes made at runtime via MQTT/web (only when they differ). */
static void syncWcFromDevice() {
  static unsigned long last = 0;
  unsigned long now = millis();
  if (now - last < 5000) return;
  last = now;

  const HcsWeatherComp& c = ot.weatherCompCfg();
  if (ot.weatherComp() == settings.wc_enable &&
      c.t_out_ref == settings.wc_t_out_ref &&
      c.t_out_design == settings.wc_t_out_design &&
      c.flow_max == settings.wc_flow_max &&
      c.flow_min == settings.wc_flow_min) return;

  settings.wc_enable = ot.weatherComp();
  settings.wc_t_out_ref = c.t_out_ref;
  settings.wc_t_out_design = c.t_out_design;
  settings.wc_flow_max = c.flow_max;
  settings.wc_flow_min = c.flow_min;
  SettingsStore store;
  store.begin();
  store.save(settings);
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
#else
  applyWcSettings(settings);
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

#ifdef HCS_GW_ENABLE
  ot.setAutopoll(false);  // master bus is driven by gateway forwarding now
  gw.begin();
  net.setGateway(&gw);
  Serial.printf("[gw] gateway up — thermostat bus %d/%d\n", OT2_IN_PIN,
                OT2_OUT_PIN);
#endif

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
  syncWcFromDevice();

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

#ifdef HCS_GW_ENABLE
  gw.loop();     // answers thermostat + forwards to boiler on demand
#else
  ot.poll();     // autonomous ~1 Hz master cycle (master-only builds)
#endif

#ifdef HCS_TEST_BOOT
  if (millis() - last_tick >= 2000) {
    last_tick = millis();
    Serial.printf("[tick] uptime=%lus\n", millis() / 1000);
  }
#endif
  delay(1);
}
