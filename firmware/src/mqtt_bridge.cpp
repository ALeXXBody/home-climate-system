#include "mqtt_bridge.h"
#include "topics.h"
#include "config.h"
#include "hcs_commands.h"

MqttBridge* MqttBridge::instance_ = nullptr;

MqttBridge::MqttBridge(Client& net, OtMaster& ot) : mqtt_(net), ot_(ot) {
  instance_ = this;
}

void MqttBridge::thunk(char* topic, byte* payload, unsigned int length) {
  if (instance_) instance_->onMessage(topic, payload, length);
}

void MqttBridge::begin(const char* host, uint16_t port, const char* user,
                       const char* pass) {
  user_ = user ? user : "";
  pass_ = pass ? pass : "";
  mqtt_.setServer(host, port);
  mqtt_.setCallback(thunk);
  mqtt_.setBufferSize(1024);
}

void MqttBridge::setDeviceInfo(const String& name, const String& ip,
                               const String& otgwNode) {
  device_name_ = name;
  ip_ = ip;
  if (otgwNode.length()) otgw_node_ = otgwNode;
}

void MqttBridge::loop() {
  if (!mqtt_.connected()) {
    reconnect();
  } else {
    mqtt_.loop();
  }

  unsigned long now = millis();
  if (mqtt_.connected() && now - last_telemetry_ms_ >= TELEMETRY_INTERVAL_MS) {
    last_telemetry_ms_ = now;
    publishTelemetry(ot_.snap());
  }
  if (mqtt_.connected() && now - last_discovery_ms_ >= DISCOVERY_INTERVAL_MS) {
    last_discovery_ms_ = now;
    publishDiscovery();
  }
}

void MqttBridge::reconnect() {
  unsigned long now = millis();
  if (now - last_reconnect_ms_ < MQTT_RECONNECT_MS) return;
  last_reconnect_ms_ = now;

  String clientId = String("hcs-") + node_id_;
  String lwt = hcsTopic(node_id_, "online");
  bool ok;
  if (user_.length()) {
    ok = mqtt_.connect(clientId.c_str(), user_.c_str(), pass_.c_str(),
                       lwt.c_str(), 0, true, "offline");
  } else {
    ok = mqtt_.connect(clientId.c_str(), lwt.c_str(), 0, true, "offline");
  }
  if (ok) {
    publish(lwt, "online", true);
    subscribeAll();
    publishDiscovery();
    last_discovery_ms_ = millis();
  }
}

void MqttBridge::subscribeAll() {
  mqtt_.subscribe(hcsSetTopic(node_id_, "ch_enable").c_str());
  mqtt_.subscribe(hcsSetTopic(node_id_, "flow_setpoint").c_str());
  mqtt_.subscribe(hcsSetTopic(node_id_, "max_modulation").c_str());
  mqtt_.subscribe(hcsSetTopic(node_id_, "dhw_enable").c_str());
  mqtt_.subscribe(hcsSetTopic(node_id_, "ota_url").c_str());
  mqtt_.subscribe(hcsSetTopic(node_id_, "reboot").c_str());

  // OTGW-compat (node from settings)
  String base = String(OTGW_COMPAT_PREFIX) + "/set/" + otgw_node_ + "/";
  mqtt_.subscribe((base + "chenable").c_str());
  mqtt_.subscribe((base + "ctrlsetpt").c_str());
  mqtt_.subscribe((base + "maxmodulation").c_str());

  // Global discovery ping
  mqtt_.subscribe("hcs/discovery/ping");
}

void MqttBridge::onMessage(char* topic, byte* payload, unsigned int length) {
  String t(topic);
  String p;
  p.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++) p += (char)payload[i];
  p.trim();
  handleCommand(t, p);
}

void MqttBridge::handleCommand(const String& topic, const String& payload) {
  if (topic == "hcs/discovery/ping") {
    publishDiscovery();
    return;
  }

  HcsCommandResult r = hcs_parse_command(topic.c_str(), payload.c_str());
  switch (r.cmd) {
    case HCS_CMD_CH_ENABLE:
      ot_.setChEnable(r.bool_value);
      break;
    case HCS_CMD_DHW_ENABLE:
      ot_.setDhwEnable(r.bool_value);
      break;
    case HCS_CMD_FLOW_SETPOINT:
      ot_.setFlowSetpoint(r.float_value);
      break;
    case HCS_CMD_MAX_MODULATION:
      ot_.setMaxModulation((int)r.int_value);
      break;
    case HCS_CMD_REBOOT:
      delay(100);
      ESP.restart();
      break;
    case HCS_CMD_OTA_URL:
      if (ota_cb_ && payload.length()) ota_cb_(payload);
      break;
    default:
      break;
  }
}

void MqttBridge::publish(const String& topic, const String& payload, bool retain) {
  mqtt_.publish(topic.c_str(), payload.c_str(), retain);
}

static String f2(float v) {
  if (isnan(v)) return "";
  char buf[16];
  dtostrf(v, 0, 1, buf);
  return String(buf);
}

void MqttBridge::publishDiscovery() {
  // Retained discovery JSON for HA Firmware tab
  String j = "{";
  j += "\"node_id\":\"" + node_id_ + "\",";
  j += "\"name\":\"" + (device_name_.length() ? device_name_ : node_id_) + "\",";
  j += "\"board\":\"" + String(HCS_BOARD_NAME) + "\",";
  j += "\"version\":\"" + String(HCS_FW_VERSION) + "\",";
  j += "\"ip\":\"" + ip_ + "\",";
  j += "\"ota_http\":\"http://" + ip_ + "/update\",";
  j += "\"api_status\":\"http://" + ip_ + "/api/status\",";
  j += "\"api_ota\":\"http://" + ip_ + "/api/ota\"";
  j += "}";

  publish(String("hcs/discovery/") + node_id_, j, true);
  publish(hcsTopic(node_id_, "ip"), ip_, true);
  publish(hcsTopic(node_id_, "board"), HCS_BOARD_NAME, true);
  publish(hcsTopic(node_id_, "version"), HCS_FW_VERSION, true);
}

void MqttBridge::publishTelemetry(const OtSnapshot& s) {
  publish(hcsTopic(node_id_, "online"), "online", true);

  if (s.valid) {
    publish(hcsTopic(node_id_, "flame"), s.flame ? "ON" : "OFF");
    publish(hcsTopic(node_id_, "ch_active"), s.ch_active ? "ON" : "OFF");
    publish(hcsTopic(node_id_, "fault"), s.fault ? "ON" : "OFF");
    if (!isnan(s.flow_temp))
      publish(hcsTopic(node_id_, "flow_temp"), f2(s.flow_temp));
    if (!isnan(s.return_temp))
      publish(hcsTopic(node_id_, "return_temp"), f2(s.return_temp));
    if (!isnan(s.outdoor_temp))
      publish(hcsTopic(node_id_, "outdoor_temp"), f2(s.outdoor_temp));
    if (!isnan(s.modulation))
      publish(hcsTopic(node_id_, "modulation"), f2(s.modulation));
  }

  publish(hcsTopic(node_id_, "cmd_ch"), ot_.chEnable() ? "on" : "off");
  publish(hcsTopic(node_id_, "cmd_flow_setpoint"), f2(ot_.flowSetpoint()));
  publish(hcsTopic(node_id_, "cmd_max_modulation"), String(ot_.maxModulation()));

  // OTGW-compat
  if (s.valid) {
    publish(otgwValue("flamestatus"), s.flame ? "ON" : "OFF");
    publish(otgwValue("chmodus"), s.ch_active ? "ON" : "OFF");
    if (!isnan(s.flow_temp))
      publish(otgwValue("boilertemperature"), f2(s.flow_temp));
    if (!isnan(s.return_temp))
      publish(otgwValue("returnwatertemperature"), f2(s.return_temp));
    if (!isnan(s.outdoor_temp))
      publish(otgwValue("outsidetemperature"), f2(s.outdoor_temp));
    if (!isnan(s.modulation))
      publish(otgwValue("relmodlvl"), f2(s.modulation));
    publish(otgwValue("controlsetpoint"), f2(ot_.flowSetpoint()));
  }
}
