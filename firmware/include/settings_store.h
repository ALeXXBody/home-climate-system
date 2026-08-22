#pragma once
// Persistent settings (NVS Preferences on ESP32, EEPROM-emulated on ESP8266 via Preferences)

#include <Arduino.h>

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
};

class SettingsStore {
 public:
  void begin();
  bool load(HcsSettings& out);
  bool save(const HcsSettings& in);
  void clear();
};
