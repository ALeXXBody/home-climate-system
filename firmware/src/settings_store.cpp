#include "settings_store.h"

#if defined(ESP32)
#include <Preferences.h>
static Preferences prefs;
#elif defined(ESP8266)
#include <EEPROM.h>
// Simple blob store on ESP8266
static const int kEepromSize = 1024;
static const uint32_t kMagic = 0x48435331;  // HCS1
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
  return out.wifi_ssid.length() > 0;
#elif defined(ESP8266)
  EepromBlob b{};
  EEPROM.get(0, b);
  if (b.magic != kMagic || !b.configured) return false;
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
