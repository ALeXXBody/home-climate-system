#include "mqtt_bridge.h"
#include "topics.h"
#include "config.h"

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
  mqtt_.setBufferSize(512);
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
}

void MqttBridge::reconnect() {
  unsigned long now = millis();
  if (now - last_reconnect_ms_ < MQTT_RECONNECT_MS) return;
  last_reconnect_ms_ = now;

  String clientId = "hcs-" + node_id_;
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
  }
}

void MqttBridge::subscribeAll() {
  // Native HCS commands
  mqtt_.subscribe(hcsSetTopic(node_id_, "ch_enable").c_str());
  mqtt_.subscribe(hcsSetTopic(node_id_, "flow_setpoint").c_str());
  mqtt_.subscribe(hcsSetTopic(node_id_, "max_modulation").c_str());
  mqtt_.subscribe(hcsSetTopic(node_id_, "dhw_enable").c_str());

  // OTGW-firmware compatible commands
  mqtt_.subscribe(otgwCmd("chenable").c_str());
  mqtt_.subscribe(otgwCmd("ctrlsetpt").c_str());
  mqtt_.subscribe(otgwCmd("maxmodulation").c_str());
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
  String low = payload;
  low.toLowerCase();

  if (topic.endsWith("/ch_enable") || topic.endsWith("/chenable")) {
    bool on = (low == "on" || low == "1" || low == "true");
    ot_.setChEnable(on);
    return;
  }
  if (topic.endsWith("/dhw_enable")) {
    bool on = (low == "on" || low == "1" || low == "true");
    ot_.setDhwEnable(on);
    return;
  }
  if (topic.endsWith("/flow_setpoint") || topic.endsWith("/ctrlsetpt")) {
    ot_.setFlowSetpoint(payload.toFloat());
    return;
  }
  if (topic.endsWith("/max_modulation") || topic.endsWith("/maxmodulation")) {
    ot_.setMaxModulation(payload.toInt());
    return;
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

void MqttBridge::publishTelemetry(const OtSnapshot& s) {
  // --- Native HCS topics ---
  publish(hcsTopic(node_id_, "online"), "online", true);
  publish(hcsTopic(node_id_, "version"), HCS_FW_VERSION, true);
  publish(hcsTopic(node_id_, "protocol_version"), "1", true);

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

  // Commanded values (always useful for debugging)
  publish(hcsTopic(node_id_, "cmd_ch"), ot_.chEnable() ? "on" : "off");
  publish(hcsTopic(node_id_, "cmd_flow_setpoint"), f2(ot_.flowSetpoint()));
  publish(hcsTopic(node_id_, "cmd_max_modulation"), String(ot_.maxModulation()));

  // --- OTGW-firmware compatible subjects (Home Climate Control backend) ---
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
