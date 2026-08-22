#pragma once
// Persistent settings (NVS Preferences on ESP32, EEPROM blob on ESP8266)

#include <Arduino.h>
#include "hcs_weather_comp.h"

struct HcsSettings {
  String wifi_ssid;
  String wifi_pass;
  String mqtt_host;
  uint16_t mqtt_port = 1883;
  String mqtt_user;
  String mqtt_pass;
  String mqtt_prefix = "hcs";
  String otgw_node = "hcs-device";
  String device_name = "Home Climate System";
  String ota_password = "";
  bool configured = false;

  // Weather compensation curve
  bool wc_enable = false;
  float wc_t_out_ref = 18.0f;
  float wc_t_out_design = -10.0f;
  float wc_flow_max = 65.0f;
  float wc_flow_min = 25.0f;
};

class SettingsStore {
 public:
  void begin();
  bool load(HcsSettings& out);
  bool save(const HcsSettings& in);
  void clear();
};
