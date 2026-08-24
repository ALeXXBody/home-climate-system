#pragma once
// MQTT topic helpers — Home Climate System native contract

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
