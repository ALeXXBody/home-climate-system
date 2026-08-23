#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include "ot_master.h"
#include "hcs_failsafe.h"

#if defined(ESP32) && defined(HCS_GW_ENABLE)
namespace hcs {
class OtGateway;
}
#endif

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

#if defined(ESP32) && defined(HCS_GW_ENABLE)
  /** Gateway builds: expose live gateway + current mode string. */
  void setGateway(hcs::OtGateway* gw, const char* mode) {
    gw_ = gw;
    gw_mode_ = mode;
  }
  /** Mode switch requested via MQTT (HcsGwCfg value). Persist + reboot in cb. */
  void onGwMode(void (*cb)(uint8_t cfg)) { gw_mode_cb_ = cb; }
  /** Override setpoint (NAN releases). */
  void onGwOverride(void (*cb)(float c)) { gw_override_cb_ = cb; }
#endif

  /** Failsafe config received (JSON payload passed through for parsing). */
  void onFailsafeCfg(void (*cb)(const String& json)) { fs_cfg_cb_ = cb; }
  /** Live failsafe state owned by main loop (for retained publishing). */
  void setFailsafeStatePtr(hcs::FsState* p) { fs_state_ptr_ = p; }

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
  void (*fs_cfg_cb_)(const String&) = nullptr;
  hcs::FsState* fs_state_ptr_ = nullptr;

#if defined(ESP32) && defined(HCS_GW_ENABLE)
  hcs::OtGateway* gw_ = nullptr;
  const char* gw_mode_ = nullptr;
  void (*gw_mode_cb_)(uint8_t) = nullptr;
  void (*gw_override_cb_)(float) = nullptr;
#endif

  void reconnect();
  void subscribeAll();
  void onMessage(char* topic, byte* payload, unsigned int length);
  static void thunk(char* topic, byte* payload, unsigned int length);
  static MqttBridge* instance_;

  void handleCommand(const String& topic, const String& payload);
  void publish(const String& topic, const String& payload, bool retain = false);
};
