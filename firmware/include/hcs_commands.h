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
 *   .../dhw_enable     ON|OFF|1|0|true|false
 *   .../flow_setpoint  float °C
 *   .../max_modulation int 0..100 (clamped)
 *   .../weather_comp   ON|OFF|1|0|true|false
 *   .../weather_comp_cfg  "<ref>,<design>,<fmax>,<fmin>" (see hcs_weather_comp.h)
 *   .../reboot         payload ignored
 *   .../ota_url        payload ignored here (URL passed through)
 *   .../settings       partial JSON, same fields as POST /api/settings
 *   .../gw/set_mode    "gateway" | "master_only" (gateway builds only)
 *   .../gw/override_setpoint  float °C | off/auto/release (gateway builds only)
 */

#include <string.h>
#include <stdlib.h>
#include "hcs_gw_cfg.h"

enum HcsCommand {
  HCS_CMD_NONE = 0,
  HCS_CMD_CH_ENABLE,
  HCS_CMD_DHW_ENABLE,
  HCS_CMD_FLOW_SETPOINT,
  HCS_CMD_DHW_SETPOINT,
  HCS_CMD_FAILSAFE_CFG,
  HCS_CMD_MAX_MODULATION,
  HCS_CMD_WEATHER_COMP,
  HCS_CMD_WEATHER_COMP_CFG,
  HCS_CMD_REBOOT,
  HCS_CMD_OTA_URL,
  HCS_CMD_GW_MODE,
  HCS_CMD_GW_OVERRIDE_SETPOINT,
  HCS_CMD_SETTINGS,
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

  if (hcs_ends_with(topic, "/ch_enable")) {
    r.cmd = HCS_CMD_CH_ENABLE;
    r.bool_value = hcs_truthy(payload);
    return r;
  }
  if (hcs_ends_with(topic, "/dhw_enable")) {
    r.cmd = HCS_CMD_DHW_ENABLE;
    r.bool_value = hcs_truthy(payload);
    return r;
  }
  if (hcs_ends_with(topic, "/flow_setpoint")) {
    r.cmd = HCS_CMD_FLOW_SETPOINT;
    r.float_value = (float)atof(payload);
    return r;
  }
  if (hcs_ends_with(topic, "/dhw_setpoint") ||
      hcs_ends_with(topic, "/hotwater_setpoint")) {
    r.cmd = HCS_CMD_DHW_SETPOINT;
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
  if (hcs_ends_with(topic, "/weather_comp")) {
    r.cmd = HCS_CMD_WEATHER_COMP;
    r.bool_value = hcs_truthy(payload);
    return r;
  }
  if (hcs_ends_with(topic, "/weather_comp_cfg")) {
    r.cmd = HCS_CMD_WEATHER_COMP_CFG;
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
  if (hcs_ends_with(topic, "/settings")) {
    // JSON payload, same shape as POST /api/settings; caller applies.
    r.cmd = HCS_CMD_SETTINGS;
    return r;
  }
  if (hcs_ends_with(topic, "/failsafe_cfg")) {
    // JSON payload {"enable":b,"flow":n,"grace_min":n} handled by caller
    r.cmd = HCS_CMD_FAILSAFE_CFG;
    return r;
  }
  if (hcs_ends_with(topic, "/gw/set_mode")) {
    r.cmd = HCS_CMD_GW_MODE;
    // payload: "auto" | "master_only" | "gateway" (legacy bools still work)
    if (hcs_ieq(payload, "gateway") || hcs_ieq(payload, "gw"))
      r.int_value = hcs::HCS_GW_GATEWAY;
    else if (hcs_ieq(payload, "master_only") || hcs_ieq(payload, "master"))
      r.int_value = hcs::HCS_GW_MASTER_ONLY;
    else if (hcs_ieq(payload, "auto"))
      r.int_value = hcs::HCS_GW_AUTO;
    else
      r.int_value =
          hcs_truthy(payload) ? hcs::HCS_GW_GATEWAY : hcs::HCS_GW_MASTER_ONLY;
    return r;
  }
  if (hcs_ends_with(topic, "/gw/override_setpoint")) {
    r.cmd = HCS_CMD_GW_OVERRIDE_SETPOINT;
    r.bool_value =
        !(hcs_ieq(payload, "off") || hcs_ieq(payload, "auto") ||
          hcs_ieq(payload, "release") || strcmp(payload, "") == 0);
    r.float_value = (float)atof(payload);
    return r;
  }
  return r;
}
