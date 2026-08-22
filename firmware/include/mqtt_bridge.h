#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include "ot_master.h"

class MqttBridge {
 public:
  MqttBridge(Client& net, OtMaster& ot);

  void begin(const char* host, uint16_t port, const char* user, const char* pass);
  void loop();
  void setNodeId(const String& id) { node_id_ = id; }
  const String& nodeId() const { return node_id_; }

  void publishTelemetry(const OtSnapshot& s);
  bool connected() { return mqtt_.connected(); }

 private:
  PubSubClient mqtt_;
  OtMaster& ot_;
  String node_id_;
  String user_;
  String pass_;
  unsigned long last_reconnect_ms_ = 0;
  unsigned long last_telemetry_ms_ = 0;

  void reconnect();
  void subscribeAll();
  void onMessage(char* topic, byte* payload, unsigned int length);
  static void thunk(char* topic, byte* payload, unsigned int length);
  static MqttBridge* instance_;

  void handleCommand(const String& topic, const String& payload);
  void publish(const String& topic, const String& payload, bool retain = false);
};
