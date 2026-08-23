#include "settings_store.h"
#include <string.h>
#include <stdio.h>

#if defined(ESP32)
#include <Preferences.h>
static Preferences prefs;
#elif defined(ESP8266)
#include <EEPROM.h>
// Simple blob store on ESP8266
static const int kEepromSize = 1024;
// v7: + 1-Wire slot table (role + custom name per probe)
// v6: + connection-loss failsafe (enable, flow setpoint, grace minutes)
// v5: + 1-Wire sensor config (enable + two probe addresses)
// v4: gw_mode byte holds HcsGwCfg (0=auto,1=master_only,2=gateway);
// v3 blobs carried a bool there and are migrated on load.
static const uint32_t kMagic = 0x48435337;      // HCS7 (current)
// Legacy blobs accepted by load() so OTA upgrades never wipe settings:
static const uint32_t kMagicV6 = 0x48435336;    // HCS6 (+failsafe)
static const uint32_t kMagicV5 = 0x48435335;    // HCS5 (+1-wire config)
static const uint32_t kMagicV4 = 0x48435334;    // HCS4 (tri-state gw cfg)
static const uint32_t kMagicV3 = 0x48435333;    // HCS3 (bool gw_mode)
struct EepromOwSlot {
  char addr[17];
  uint8_t role;
  char name[17];
};
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
  // v5:
  uint8_t ow_enable;
  char ow_addr_outdoor[17];
  char ow_addr_return[17];
  // v6:
  uint8_t fs_enable;
  float fs_flow_c;
  uint8_t fs_grace_min;
  // v7:
  uint8_t ow_slot_count;
  EepromOwSlot ow_slots[8];
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
  out.ow_enable = prefs.getBool("ow_en", false);
  out.ow_slot_count = 0;
  for (size_t i = 0; i < hcs::kOwMaxSlots; i++) {
    out.ow_slots[i] = hcs::OwSlot{};
  }
  if (prefs.isKey("ow_n")) {
    uint8_t n = prefs.getUChar("ow_n", 0);
    if (n > hcs::kOwMaxSlots) n = hcs::kOwMaxSlots;
    out.ow_slot_count = n;
    for (uint8_t i = 0; i < n; i++) {
      char ka[8], kr[8], kn[8];
      snprintf(ka, sizeof(ka), "ow%ua", (unsigned)i);
      snprintf(kr, sizeof(kr), "ow%ur", (unsigned)i);
      snprintf(kn, sizeof(kn), "ow%un", (unsigned)i);
      String a = prefs.getString(ka, "");
      strncpy(out.ow_slots[i].addr, a.c_str(), sizeof(out.ow_slots[i].addr) - 1);
      out.ow_slots[i].role = prefs.getUChar(kr, hcs::OW_ROLE_NONE);
      String nm = prefs.getString(kn, "");
      strncpy(out.ow_slots[i].name, nm.c_str(), sizeof(out.ow_slots[i].name) - 1);
    }
  } else {
    // migrate legacy outdoor/return keys
    String o = prefs.getString("ow_out", "");
    String r = prefs.getString("ow_ret", "");
    hcs::ow_slots_from_legacy(o.c_str(), r.c_str(), out.ow_slots, hcs::kOwMaxSlots);
    out.ow_slot_count = hcs::kOwMaxSlots;
  }
  out.mqtt_host.trim();
  out.mqtt_user.trim();
  out.mqtt_prefix.trim();
  out.otgw_node.trim();
  out.fs_enable = prefs.getBool("fs_en", kFsEnableDefault);
  out.fs_flow_c = prefs.getFloat("fs_flow", kFsFlowDefaultC);
  out.fs_grace_min = prefs.getUChar("fs_grace", kFsGraceDefaultMin);
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
  // Audit F13: accept every historical blob version — rejecting an older
  // magic made OTA upgrades wipe WiFi/MQTT settings and drop the device
  // back into captive-portal mode.
  const bool v7 = b.magic == kMagic;
  const bool v6 = b.magic == kMagicV6;
  const bool v5 = b.magic == kMagicV5;
  const bool v4 = b.magic == kMagicV4;
  const bool v3 = b.magic == kMagicV3;
  if (!v7 && !v6 && !v5 && !v4 && !v3) return false;
  if (!b.configured) return false;

  if (v3) {
    // legacy v3 bool: migrate once; next save() writes the current magic
    out.gw_cfg = b.gw_mode == 1 ? HCS_GW_GATEWAY : HCS_GW_MASTER_ONLY;
  } else {  // v4 onward stores the tri-state value
    out.gw_cfg =
        b.gw_mode <= HCS_GW_GATEWAY ? b.gw_mode : HCS_GW_AUTO;
  }
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
  out.mqtt_host.trim();
  out.ota_password = String(b.ota_password);
  out.wc_enable = b.wc_enable != 0;
  out.wc_t_out_ref = b.wc_t_out_ref;
  out.wc_t_out_design = b.wc_t_out_design;
  out.wc_flow_max = b.wc_flow_max;
  out.wc_flow_min = b.wc_flow_min;
  if (v5 || v6 || v7) {
    out.ow_enable = b.ow_enable != 0;
  }
  if (v6 || v7) {
    out.fs_enable = b.fs_enable != 0;
    out.fs_flow_c = b.fs_flow_c;
    out.fs_grace_min = b.fs_grace_min;
  }
  // slots: v7 native; older → migrate two fixed addresses
  for (size_t i = 0; i < hcs::kOwMaxSlots; i++) out.ow_slots[i] = hcs::OwSlot{};
  if (v7) {
    uint8_t n = b.ow_slot_count;
    if (n > hcs::kOwMaxSlots) n = hcs::kOwMaxSlots;
    out.ow_slot_count = n;
    for (uint8_t i = 0; i < n; i++) {
      strncpy(out.ow_slots[i].addr, b.ow_slots[i].addr,
              sizeof(out.ow_slots[i].addr) - 1);
      out.ow_slots[i].role = b.ow_slots[i].role;
      strncpy(out.ow_slots[i].name, b.ow_slots[i].name,
              sizeof(out.ow_slots[i].name) - 1);
    }
  } else if (v5 || v6) {
    hcs::ow_slots_from_legacy(b.ow_addr_outdoor, b.ow_addr_return,
                              out.ow_slots, hcs::kOwMaxSlots);
    out.ow_slot_count = hcs::kOwMaxSlots;
  }
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
  prefs.putBool("ow_en", in.ow_enable);
  // write slot table; also keep legacy keys for outdoor/return channels
  prefs.putUChar("ow_n", hcs::kOwMaxSlots);
  for (uint8_t i = 0; i < hcs::kOwMaxSlots; i++) {
    char ka[8], kr[8], kn[8];
    snprintf(ka, sizeof(ka), "ow%ua", (unsigned)i);
    snprintf(kr, sizeof(kr), "ow%ur", (unsigned)i);
    snprintf(kn, sizeof(kn), "ow%un", (unsigned)i);
    prefs.putString(ka, in.ow_slots[i].addr);
    prefs.putUChar(kr, in.ow_slots[i].role);
    prefs.putString(kn, in.ow_slots[i].name);
  }
  prefs.putString("ow_out", in.ow_addr_outdoor());
  prefs.putString("ow_ret", in.ow_addr_return());
  prefs.putBool("fs_en", in.fs_enable);
  prefs.putFloat("fs_flow", in.fs_flow_c);
  prefs.putUChar("fs_grace", in.fs_grace_min);
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
  b.ow_enable = in.ow_enable ? 1 : 0;
  strncpy(b.ow_addr_outdoor, in.ow_addr_outdoor(),
          sizeof(b.ow_addr_outdoor) - 1);
  strncpy(b.ow_addr_return, in.ow_addr_return(),
          sizeof(b.ow_addr_return) - 1);
  b.fs_enable = in.fs_enable ? 1 : 0;
  b.fs_flow_c = in.fs_flow_c;
  b.fs_grace_min = in.fs_grace_min;
  b.ow_slot_count = hcs::kOwMaxSlots;
  for (uint8_t i = 0; i < hcs::kOwMaxSlots; i++) {
    strncpy(b.ow_slots[i].addr, in.ow_slots[i].addr,
            sizeof(b.ow_slots[i].addr) - 1);
    b.ow_slots[i].role = in.ow_slots[i].role;
    strncpy(b.ow_slots[i].name, in.ow_slots[i].name,
            sizeof(b.ow_slots[i].name) - 1);
  }
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
