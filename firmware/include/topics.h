#pragma once
// MQTT topic helpers — Home Climate System native + OTGW-compat

#include "config.h"
#include <Arduino.h>

inline String hcsBase(const String& nodeId) {
  return String(MQTT_PREFIX) + "/" + nodeId;
}

inline String hcsTopic(const String& nodeId, const char* leaf) {
  return hcsBase(nodeId) + "/" + leaf;
}

inline String hcsSetTopic(const String& nodeId, const char* leaf) {
  return hcsBase(nodeId) + "/set/" + leaf;
}

// OTGW-firmware style telemetry under OTGW_COMPAT_PREFIX/
inline String otgwValue(const char* subject) {
  return String(OTGW_COMPAT_PREFIX) + "/" + subject;
}

// OTGW-firmware style command: OTGW/set/<node>/<cmd>
inline String otgwCmd(const char* cmd) {
  return String(OTGW_COMPAT_PREFIX) + "/set/" + OTGW_COMPAT_NODE + "/" + cmd;
}
