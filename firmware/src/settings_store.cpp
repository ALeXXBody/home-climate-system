#include "settings_store.h"

#if defined(ESP32)
#include <Preferences.h>
static Preferences prefs;
#elif defined(ESP8266)
#include <EEPROM.h>
// Simple blob store on ESP8266
static const int kEepromSize = 1024;
// v4: gw_mode byte now holds HcsGwCfg (0=auto,1=master_only,2=gateway);
// v3 blobs carried a bool there and are migrated on load.
static const uint32_t kMagic = 0x48435334;      // HCS4
static const uint32_t kMagicV3 = 0x48435333;    // HCS3 (legacy bool gw_mode)
struct EepromBlob {
  uint32_t magic;
  uint16_t mqtt_port;
  char wifi_ssid[64];
  char wifi_pass[64];
  char mqtt_host[64];
  char mqtt_user[32];
  char mqtt_pass[32];
  char mqtt_prefix[16];
  char otgw_node[32];
  char device_name[32];
  char ota_password[32];
  uint8_t configured;
  uint8_t wc_enable;
  float wc_t_out_ref;
  float wc_t_out_design;
  float wc_flow_max;
  float wc_flow_min;
  uint8_t gw_mode;
};
#endif

void SettingsStore::begin() {
#if defined(ESP32)
  prefs.begin("hcs", false);
#elif defined(ESP8266)
  EEPROM.begin(kEepromSize);
#endif
}

bool SettingsStore::load(HcsSettings& out) {
#if defined(ESP32)
  out.configured = prefs.getBool("cfg", false);
  if (!out.configured) return false;
  out.wifi_ssid = prefs.getString("wifi_ssid", "");
  out.wifi_pass = prefs.getString("wifi_pass", "");
  out.mqtt_host = prefs.getString("mqtt_host", "");
  out.mqtt_port = prefs.getUShort("mqtt_port", 1883);
  out.mqtt_user = prefs.getString("mqtt_user", "");
  out.mqtt_pass = prefs.getString("mqtt_pass", "");
  out.mqtt_prefix = prefs.getString("mqtt_prefix", "hcs");
  out.otgw_node = prefs.getString("otgw_node", "hcs-device");
  out.device_name = prefs.getString("dev_name", "Home Climate System");
  out.ota_password = prefs.getString("ota_pass", "");
  out.wc_enable = prefs.getBool("wc_en", false);
  out.wc_t_out_ref = prefs.getFloat("wc_ref", 18.0f);
  out.wc_t_out_design = prefs.getFloat("wc_dsn", -10.0f);
  out.wc_flow_max = prefs.getFloat("wc_fmax", 65.0f);
  out.wc_flow_min = prefs.getFloat("wc_fmin", 25.0f);
  if (prefs.isKey("gw_cfg")) {
    uint8_t cfg = prefs.getUChar("gw_cfg", HCS_GW_AUTO);
    out.gw_cfg = (cfg <= HCS_GW_GATEWAY) ? cfg : HCS_GW_AUTO;
  } else if (prefs.isKey("gw_mode")) {
    // legacy bool key: preserve the explicit choice it represented
    out.gw_cfg =
        prefs.getBool("gw_mode", false) ? HCS_GW_GATEWAY : HCS_GW_MASTER_ONLY;
  } else {
    out.gw_cfg = HCS_GW_AUTO;  // fresh install: auto-detect
  }
  return out.wifi_ssid.length() > 0;
#elif defined(ESP8266)
  EepromBlob b{};
  EEPROM.get(0, b);
  if (b.configured && b.magic == kMagicV3) {
    // legacy v3 bool: migrate once; next save() writes the v4 magic
    if (b.gw_mode == 1) {
      out.gw_cfg = HCS_GW_GATEWAY;
    } else {
      out.gw_cfg = HCS_GW_MASTER_ONLY;
    }
  } else if (b.magic == kMagic && b.gw_mode <= HCS_GW_GATEWAY) {
    out.gw_cfg = b.gw_mode;
  }
  if (b.magic != kMagic && b.magic != kMagicV3) return false;
  if (!b.configured) return false;
  out.wifi_ssid = String(b.wifi_ssid);
  out.wifi_pass = String(b.wifi_pass);
  out.mqtt_host = String(b.mqtt_host);
  out.mqtt_port = b.mqtt_port ? b.mqtt_port : 1883;
  out.mqtt_user = String(b.mqtt_user);
  out.mqtt_pass = String(b.mqtt_pass);
  out.mqtt_prefix = String(b.mqtt_prefix);
  if (!out.mqtt_prefix.length()) out.mqtt_prefix = "hcs";
  out.otgw_node = String(b.otgw_node);
  if (!out.otgw_node.length()) out.otgw_node = "hcs-device";
  out.device_name = String(b.device_name);
  out.ota_password = String(b.ota_password);
  out.wc_enable = b.wc_enable != 0;
  out.wc_t_out_ref = b.wc_t_out_ref;
  out.wc_t_out_design = b.wc_t_out_design;
  out.wc_flow_max = b.wc_flow_max;
  out.wc_flow_min = b.wc_flow_min;
  out.configured = true;
  return out.wifi_ssid.length() > 0;
#endif
}

bool SettingsStore::save(const HcsSettings& in) {
#if defined(ESP32)
  prefs.putBool("cfg", true);
  prefs.putString("wifi_ssid", in.wifi_ssid);
  prefs.putString("wifi_pass", in.wifi_pass);
  prefs.putString("mqtt_host", in.mqtt_host);
  prefs.putUShort("mqtt_port", in.mqtt_port);
  prefs.putString("mqtt_user", in.mqtt_user);
  prefs.putString("mqtt_pass", in.mqtt_pass);
  prefs.putString("mqtt_prefix", in.mqtt_prefix);
  prefs.putString("otgw_node", in.otgw_node);
  prefs.putString("dev_name", in.device_name);
  prefs.putString("ota_pass", in.ota_password);
  prefs.putUChar("gw_cfg", in.gw_cfg);
  return true;
#elif defined(ESP8266)
  EepromBlob b{};
  b.magic = kMagic;
  b.configured = 1;
  b.mqtt_port = in.mqtt_port;
  strncpy(b.wifi_ssid, in.wifi_ssid.c_str(), sizeof(b.wifi_ssid) - 1);
  strncpy(b.wifi_pass, in.wifi_pass.c_str(), sizeof(b.wifi_pass) - 1);
  strncpy(b.mqtt_host, in.mqtt_host.c_str(), sizeof(b.mqtt_host) - 1);
  strncpy(b.mqtt_user, in.mqtt_user.c_str(), sizeof(b.mqtt_user) - 1);
  strncpy(b.mqtt_pass, in.mqtt_pass.c_str(), sizeof(b.mqtt_pass) - 1);
  strncpy(b.mqtt_prefix, in.mqtt_prefix.c_str(), sizeof(b.mqtt_prefix) - 1);
  strncpy(b.otgw_node, in.otgw_node.c_str(), sizeof(b.otgw_node) - 1);
  strncpy(b.device_name, in.device_name.c_str(), sizeof(b.device_name) - 1);
  strncpy(b.ota_password, in.ota_password.c_str(), sizeof(b.ota_password) - 1);
  b.wc_enable = in.wc_enable ? 1 : 0;
  b.wc_t_out_ref = in.wc_t_out_ref;
  b.wc_t_out_design = in.wc_t_out_design;
  b.wc_flow_max = in.wc_flow_max;
  b.wc_flow_min = in.wc_flow_min;
  b.gw_mode = (in.gw_cfg <= HCS_GW_GATEWAY) ? in.gw_cfg : HCS_GW_AUTO;
  EEPROM.put(0, b);
  return EEPROM.commit();
#endif
}

void SettingsStore::clear() {
#if defined(ESP32)
  prefs.clear();
#elif defined(ESP8266)
  EepromBlob b{};
  EEPROM.put(0, b);
  EEPROM.commit();
#endif
}
