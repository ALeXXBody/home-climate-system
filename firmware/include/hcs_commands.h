#pragma once
/**
 * Portable MQTT command parser for Home Climate System firmware.
 *
 * Deliberately free of Arduino dependencies (plain C/C++) so the exact same
 * code that runs on ESP8266/ESP32 can be unit-tested on the host with
 * `pio test -e native`.
 *
 * Recognised topics (suffix match):
 *   .../ch_enable      ON|OFF|1|0|true|false
 *   .../chenable       alias (OTGW-compat)
 *   .../dhw_enable     ON|OFF|1|0|true|false
 *   .../flow_setpoint  float °C
 *   .../ctrlsetpt      alias (OTGW-compat)
 *   .../max_modulation int 0..100 (clamped)
 *   .../maxmodulation  alias (OTGW-compat)
 *   .../reboot         payload ignored
 *   .../ota_url        payload ignored here (URL passed through)
 */

#include <string.h>
#include <stdlib.h>

enum HcsCommand {
  HCS_CMD_NONE = 0,
  HCS_CMD_CH_ENABLE,
  HCS_CMD_DHW_ENABLE,
  HCS_CMD_FLOW_SETPOINT,
  HCS_CMD_MAX_MODULATION,
  HCS_CMD_REBOOT,
  HCS_CMD_OTA_URL,
};

struct HcsCommandResult {
  HcsCommand cmd;
  bool bool_value;
  float float_value;
  long int_value;
};

inline bool hcs_ends_with(const char* s, const char* suffix) {
  if (!s || !suffix) return false;
  size_t ls = strlen(s), lf = strlen(suffix);
  return ls >= lf && strcmp(s + ls - lf, suffix) == 0;
}

inline bool hcs_ieq(const char* a, const char* b) {
  if (!a || !b) return false;
  while (*a && *b) {
    char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
    char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
    if (ca != cb) return false;
    ++a;
    ++b;
  }
  return *a == *b;
}

inline bool hcs_truthy(const char* v) {
  return hcs_ieq(v, "on") || strcmp(v, "1") == 0 || hcs_ieq(v, "true");
}

inline HcsCommandResult hcs_parse_command(const char* topic, const char* payload) {
  HcsCommandResult r;
  r.cmd = HCS_CMD_NONE;
  r.bool_value = false;
  r.float_value = 0.0f;
  r.int_value = 0;
  if (!topic || !payload) return r;

  if (hcs_ends_with(topic, "/ch_enable") || hcs_ends_with(topic, "/chenable")) {
    r.cmd = HCS_CMD_CH_ENABLE;
    r.bool_value = hcs_truthy(payload);
    return r;
  }
  if (hcs_ends_with(topic, "/dhw_enable")) {
    r.cmd = HCS_CMD_DHW_ENABLE;
    r.bool_value = hcs_truthy(payload);
    return r;
  }
  if (hcs_ends_with(topic, "/flow_setpoint") || hcs_ends_with(topic, "/ctrlsetpt")) {
    r.cmd = HCS_CMD_FLOW_SETPOINT;
    r.float_value = (float)atof(payload);
    return r;
  }
  if (hcs_ends_with(topic, "/max_modulation") ||
      hcs_ends_with(topic, "/maxmodulation")) {
    r.cmd = HCS_CMD_MAX_MODULATION;
    r.int_value = atol(payload);
    if (r.int_value < 0) r.int_value = 0;
    if (r.int_value > 100) r.int_value = 100;
    return r;
  }
  if (hcs_ends_with(topic, "/reboot")) {
    r.cmd = HCS_CMD_REBOOT;
    return r;
  }
  if (hcs_ends_with(topic, "/ota_url")) {
    r.cmd = HCS_CMD_OTA_URL;
    return r;
  }
  return r;
}
