/**
 * Home Climate System — OpenTherm master firmware
 *
 * Hardware:
 *  - DIYLess Master OpenTherm Shield + boiler OT wires
 *  - ESP8266 D1 mini (stacked) OR ESP32-S3-Zero (jumper wires)
 *
 * Talks MQTT to Home Climate Control (native hcs/ + OTGW-compat topics).
 * OpenTherm stack: ihormelnyk/opentherm_library (MIT).
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

static OtMaster ot(OT_IN_PIN, OT_OUT_PIN);
static WiFiClient wifiClient;
static MqttBridge mqtt(wifiClient, ot);

static String makeNodeId() {
#if defined(ESP8266)
  String m = WiFi.macAddress();
#elif defined(ESP32)
  String m = WiFi.macAddress();
#else
  String m = "000000000000";
#endif
  m.replace(":", "");
  m.toLowerCase();
  return "hcs-" + m;
}

static void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.printf("[wifi] connecting to %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] ok %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[wifi] failed — will retry");
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("=== Home Climate System ==="));
  Serial.printf("FW %s  OT_IN=%d OT_OUT=%d\n", HCS_FW_VERSION, OT_IN_PIN, OT_OUT_PIN);

#if CH_FAILSAFE_OFF_ON_BOOT
  ot.setChEnable(false);
#else
  ot.setChEnable(DEFAULT_CH_ENABLE);
#endif
  ot.setDhwEnable(DEFAULT_DHW_ENABLE);
  ot.setFlowSetpoint(DEFAULT_FLOW_SETPOINT_C);
  ot.setMaxModulation(DEFAULT_MAX_MODULATION);
  ot.begin();

  connectWifi();

  String node = makeNodeId();
  mqtt.setNodeId(node);
  mqtt.begin(MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASS);
  Serial.printf("[mqtt] node=%s broker=%s\n", node.c_str(), MQTT_HOST);
  Serial.println(F("[mqtt] commands: hcs/<node>/set/{ch_enable,flow_setpoint,max_modulation}"));
  Serial.printf("[mqtt] OTGW-compat: %s/set/%s/{chenable,ctrlsetpt,maxmodulation}\n",
                OTGW_COMPAT_PREFIX, OTGW_COMPAT_NODE);
}

void loop() {
  ot.loop();

  if (WiFi.status() != WL_CONNECTED) {
    connectWifi();
  }

  mqtt.loop();
  ot.poll();

  delay(1);
}
