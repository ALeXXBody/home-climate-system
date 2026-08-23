#pragma once
// Persistent settings (NVS Preferences on ESP32, EEPROM blob on ESP8266)

#include <Arduino.h>
#include "hcs_gw_cfg.h"
#include "hcs_failsafe.h"
#include "hcs_weather_comp.h"

// Re-export gateway-role names for unqualified use across firmware sources
using hcs::HcsGwCfg;
using hcs::HCS_GW_AUTO;
using hcs::HCS_GW_MASTER_ONLY;
using hcs::HCS_GW_GATEWAY;
using hcs::hcs_gw_cfg_name;
using hcs::kFsEnableDefault;
using hcs::kFsFlowDefaultC;
using hcs::kFsGraceDefaultMin;

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

  // Gateway role (meaningful on ESP32 gateway builds only; see HcsGwCfg)
  uint8_t gw_cfg = HCS_GW_AUTO;

  // 1-Wire DS18B20 probes (see hcs_sensor_logic.h); addrs as 16 hex chars
  bool ow_enable = false;
  String ow_addr_outdoor = "";
  String ow_addr_return = "";

  // Connection-loss failsafe (see hcs_failsafe.h)
  bool fs_enable = kFsEnableDefault;
  float fs_flow_c = kFsFlowDefaultC;
  uint8_t fs_grace_min = kFsGraceDefaultMin;
};

class SettingsStore {
 public:
  void begin();
  bool load(HcsSettings& out);
  bool save(const HcsSettings& in);
  void clear();
};
