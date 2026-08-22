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
  void setDeviceInfo(const String& name, const String& ip, const String& otgwNode);
  const String& nodeId() const { return node_id_; }

  void publishTelemetry(const OtSnapshot& s);
  void publishDiscovery();
  bool connected() { return mqtt_.connected(); }

  /** Optional: set callback when OTA URL received via MQTT */
  void onOtaUrl(void (*cb)(const String& url)) { ota_cb_ = cb; }

 private:
  PubSubClient mqtt_;
  OtMaster& ot_;
  String node_id_;
  String device_name_;
  String ip_;
  String otgw_node_ = "hcs-device";
  String user_;
  String pass_;
  unsigned long last_reconnect_ms_ = 0;
  unsigned long last_telemetry_ms_ = 0;
  unsigned long last_discovery_ms_ = 0;
  void (*ota_cb_)(const String&) = nullptr;

  void reconnect();
  void subscribeAll();
  void onMessage(char* topic, byte* payload, unsigned int length);
  static void thunk(char* topic, byte* payload, unsigned int length);
  static MqttBridge* instance_;

  void handleCommand(const String& topic, const String& payload);
  void publish(const String& topic, const String& payload, bool retain = false);
};
