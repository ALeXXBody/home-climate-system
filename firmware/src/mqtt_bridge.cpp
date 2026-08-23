#include "mqtt_bridge.h"
#include "topics.h"
#include "config.h"
#include "hcs_commands.h"
#include "hcs_boiler_text.h"
#include "hcs_sensors.h"
#if defined(ESP32) && defined(HCS_GW_ENABLE)
#include "ot_gateway.h"
#endif

MqttBridge* MqttBridge::instance_ = nullptr;

MqttBridge::MqttBridge(Client& net, OtMaster& ot) : mqtt_(net), ot_(ot) {
  instance_ = this;
}

void MqttBridge::thunk(char* topic, byte* payload, unsigned int length) {
  if (instance_) instance_->onMessage(topic, payload, length);
}

void MqttBridge::begin(const char* host, uint16_t port, const char* user,
                       const char* pass) {
  user_ = hcs_trim(user ? user : "");
  pass_ = pass ? pass : "";
  host_ = hcs_trim(host ? host : "");
  if (!port) port = 1883;
  port_ = port;
  // Hosts/IPs never contain whitespace. Strip it (self-heals values saved
  // dirty by portal forms) so PubSubClient cannot fail DNS silently.
  while (host_.indexOf(' ') >= 0) host_.replace(" ", "");
  while (host_.indexOf('\t') >= 0) host_.replace("\t", "");
  while (host_.indexOf('\r') >= 0) host_.replace("\r", "");
  while (host_.indexOf('\n') >= 0) host_.replace("\n", "");
  host_ok_ = false;
  if (!host_.length() || hcs_host_is_placeholder(host_)) {
    Serial.printf("[mqtt] no broker configured (%s)\n",
                  host_.length() ? "template placeholder" : "empty");
    return;
  }
  if (host_.length() > 63) {
    Serial.println("[mqtt] broker host too long (>63)");
    return;
  }
  host_ok_ = true;
  mqtt_.setServer(host_.c_str(), port_);
  mqtt_.setCallback(thunk);
  mqtt_.setBufferSize(1024);
  // Audit F7: default 15 s socket timeout stalled the whole loop during
  // every reconnect attempt against an unreachable broker.
  mqtt_.setSocketTimeout(4);
  Serial.printf("[mqtt] broker %s:%u user='%s'\n", host_.c_str(), port_,
                user_.length() ? user_.c_str() : "(anonymous)");
}

void MqttBridge::setDeviceInfo(const String& name, const String& ip) {
  device_name_ = name;
  ip_ = ip;
}

void MqttBridge::loop() {
  if (!host_ok_) return;  // rejected/absent config: stay silent but visible
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
  static unsigned long delay_ms = MQTT_RECONNECT_MS;
  unsigned long now = millis();
  if (now - last_reconnect_ms_ < delay_ms) return;
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
  if (!ok) {
    // PubSubClient state codes: -4 timeout, -3 lost, -2 failed(TCP/DNS),
    // -1 disconnected, 5 bad user/pass — log EVERY failure so a dead
    // broker link is never silent again.
    Serial.printf("[mqtt] connect to %s:%u failed (state=%d, retry in %lus)\n",
                  host_.c_str(), port_, mqtt_.state(),
                  (delay_ms = min(2 * delay_ms, 60000UL)) / 1000);
    return;
  }
  Serial.println("[mqtt] connected");
  delay_ms = MQTT_RECONNECT_MS;
  publish(lwt, "online", true);
  subscribeAll();
  publishDiscovery();
  last_discovery_ms_ = millis();
}

void MqttBridge::subscribeAll() {
  mqtt_.subscribe(hcsSetTopic(node_id_, "ch_enable").c_str());
  mqtt_.subscribe(hcsSetTopic(node_id_, "flow_setpoint").c_str());
  mqtt_.subscribe(hcsSetTopic(node_id_, "max_modulation").c_str());
  mqtt_.subscribe(hcsSetTopic(node_id_, "dhw_enable").c_str());
  mqtt_.subscribe(hcsSetTopic(node_id_, "weather_comp").c_str());
  mqtt_.subscribe(hcsSetTopic(node_id_, "weather_comp_cfg").c_str());
  mqtt_.subscribe(hcsSetTopic(node_id_, "failsafe_cfg").c_str());
  mqtt_.subscribe(hcsSetTopic(node_id_, "ota_url").c_str());
  mqtt_.subscribe(hcsSetTopic(node_id_, "reboot").c_str());
#if defined(ESP32) && defined(HCS_GW_ENABLE)
  mqtt_.subscribe(hcsSetTopic(node_id_, "gw/set_mode").c_str());
  mqtt_.subscribe(hcsSetTopic(node_id_, "gw/override_setpoint").c_str());
#endif

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
    case HCS_CMD_DHW_SETPOINT:
      // "off"/"auto" releases control back to the boiler/thermostat
      ot_.setDhwSetpoint(hcs_ieq(payload.c_str(), "off") ||
                                 hcs_ieq(payload.c_str(), "auto")
                             ? (float)NAN
                             : r.float_value);
      break;
    case HCS_CMD_MAX_MODULATION:
      ot_.setMaxModulation((int)r.int_value);
      break;
    case HCS_CMD_WEATHER_COMP:
      ot_.setWeatherComp(r.bool_value);
      break;
    case HCS_CMD_WEATHER_COMP_CFG:
      if (!ot_.setWeatherCompCfg(payload.c_str())) {
        publish(hcsTopic(node_id_, "wc_error"),
                "bad weather_comp_cfg (want ref,design,fmax,fmin)");
      }
      break;
    case HCS_CMD_REBOOT:
      delay(100);
      ESP.restart();
      break;
    case HCS_CMD_OTA_URL:
      if (ota_cb_ && payload.length()) ota_cb_(payload);
      break;
#if defined(ESP32) && defined(HCS_GW_ENABLE)
    case HCS_CMD_GW_MODE:
      if (gw_mode_cb_) gw_mode_cb_((uint8_t)r.int_value);
      break;
    case HCS_CMD_GW_OVERRIDE_SETPOINT:
      if (gw_override_cb_)
        gw_override_cb_(r.bool_value ? r.float_value : (float)NAN);
      break;
#endif
    case HCS_CMD_FAILSAFE_CFG:
      if (fs_cfg_cb_) fs_cfg_cb_(payload);
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
#if defined(ESP32) && defined(HCS_GW_ENABLE)
  if (gw_) {
    const hcs::GwCounters& c = gw_->counters();
    j += ",\"gw\":{";
    j += "\"requests\":" + String(c.requests) + ",";
    j += "\"forwarded\":" + String(c.forwarded) + ",";
    j += "\"answered_local\":" + String(c.answered_local) + ",";
    j += "\"modified\":" + String(c.modified) + ",";
    j += "\"errors\":" + String(c.errors) + "}";
  }
#endif
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
    if (s.valid_pressure) publish(hcsTopic(node_id_, "ch_pressure"), f2(s.pressure_bar));
    if (!isnan(ot_.dhwSetpoint()))
      publish(hcsTopic(node_id_, "dhw_setpoint"), f2(ot_.dhwSetpoint()), true);
  }

  // Boiler diagnostics -> retained clean text + raw numbers (change-gated)
  {
    hcs::BoilerDiag bd;
    bd.valid_asf = s.valid_asf;
    bd.valid_oem = s.valid_oem;
    bd.asf = s.asf_flags;
    bd.oem = s.oem_diag;
    char txt[160];
    hcs::boiler_diag_text(bd, txt, sizeof(txt));
    static String last_txt, last_state;
    String state = hcs::boiler_diag_state(bd);
    String txt_s = String(txt);
    if (txt_s != last_txt) {
      publish(hcsTopic(node_id_, "boiler_diag"), txt, true);
      publish(hcsTopic(node_id_, "boiler_state"), state, true);
      last_txt = txt_s;
      last_state = state;
    }
  }

  // Failsafe live state (retained + OTGW-compat mirror)
  if (fs_state_ptr_) {
    static String last_fs;
    String v =
        (*fs_state_ptr_ == hcs::FsState::FAILSAFE)
            ? "ON"
            : (*fs_state_ptr_ == hcs::FsState::HOLD ? "HOLD" : "OFF");
    if (v != last_fs) {
      publish(hcsTopic(node_id_, "failsafe"), v, true);
      last_fs = v;
    }
  }

  // Boiler identity + fault history (retained, change-gated)
  {
    static String last_ident, last_fhb;
    char fhb[64] = "";
    hcs::ot_fhb_format(s.fhb_codes, s.fhb_count, fhb, sizeof(fhb));
    String ident = "";
    if (s.valid_slave_cfg)
      ident += "slave member " + String(s.slave_member_id) + " config 0x" +
               String(s.slave_config, HEX);
    if (s.valid_master_cfg) {
      if (ident.length()) ident += "; ";
      ident += "master member " + String(s.master_member_id) + " config 0x" +
               String(s.master_config, HEX);
    }
    if (s.valid_capacity && ident.length())
      ident += "; " + String(s.capacity_kw) + " kW min-mod " +
               String(s.min_mod_pct) + "%";
    if (s.valid_fhb && ident.length())
      ident += "; FHB size " + String(s.fhb_size);
    if (ident != last_ident) {
      publish(hcsTopic(node_id_, "boiler_identity"), ident, true);
      if (s.valid_slave_cfg)
        publish(hcsTopic(node_id_, "boiler_member"),
                String(s.slave_member_id), true);
      last_ident = ident;
    }
    if (String(fhb) != last_fhb) {
      publish(hcsTopic(node_id_, "fault_history"), fhb, true);
      last_fhb = String(fhb);
    }
  }

  publish(hcsTopic(node_id_, "cmd_ch"), ot_.chEnable() ? "on" : "off");
  publish(hcsTopic(node_id_, "cmd_flow_setpoint"), f2(ot_.flowSetpoint()));
  publish(hcsTopic(node_id_, "cmd_max_modulation"), String(ot_.maxModulation()));
  publish(hcsTopic(node_id_, "weather_comp"), ot_.weatherComp() ? "on" : "off");
  if (!isnan(ot_.wcTarget()))
    publish(hcsTopic(node_id_, "wc_target"), f2(ot_.wcTarget()));

#if defined(ESP32) && defined(HCS_GW_ENABLE)
  if (gw_) {
    publish(hcsTopic(node_id_, "gw/mode"),
            gw_mode_ ? gw_mode_ : "gateway", true);
    publish(hcsTopic(node_id_, "gw/tstat_online"),
            gw_->thermostatOnline() ? "ON" : "OFF");
    float ov = gw_->overrideSetpointC();
    publish(hcsTopic(node_id_, "gw/override_setpoint"),
            isnan(ov) ? "" : f2(ov), true);
  } else {
    // gateway compiled in but inactive: keep retained state accurate
    publish(hcsTopic(node_id_, "gw/mode"), "master_only", true);
    publish(hcsTopic(node_id_, "gw/tstat_online"), "OFF");
    publish(hcsTopic(node_id_, "gw/override_setpoint"), "", true);
  }
#endif

  // 1-Wire probes: retained snapshot + custom leaves under x/<name>
  if (sensors_ && sensors_->enabled()) {
    unsigned long now = millis();
    String j = "{\"enabled\":true,\"devices\":[";
    for (size_t i = 0; i < sensors_->count(); i++) {
      if (i) j += ',';
      const hcs::OwDevice& d = sensors_->device(i);
      hcs::OwSlot sl = sensors_->slotForDevice(i);
      char hex[17];
      hcs::ow_addr_to_hex(d.addr, hex);
      j += "{\"addr\":\"";
      j += hex;
      j += "\",\"health\":\"";
      j += hcs::ow_health_name(d.health);
      j += "\",\"role\":\"";
      j += hcs::ow_role_name((hcs::OwRole)sl.role);
      j += "\"";
      if (sl.role == hcs::OW_ROLE_CUSTOM && sl.name[0]) {
        j += ",\"name\":\"";
        j += sl.name;
        j += "\"";
      }
      if (d.valid && d.health == hcs::OW_HEALTH_OK &&
          (now - d.ts_ms) <= hcs::kOwStaleMs) {
        j += ",\"temp_c\":";
        j += f2(d.celsius);
      } else {
        j += ",\"temp_c\":null";
      }
      j += '}';
    }
    j += "]}";
    publish(hcsTopic(node_id_, "sensors"), j, true);

    char name[hcs::kOwNameMax + 1];
    float c = NAN;
    for (size_t i = 0; sensors_->customAt(i, name, &c, now); i++) {
      if (name[0] && !isnan(c)) {
        String leaf = String("x/") + name;
        publish(hcsTopic(node_id_, leaf.c_str()), f2(c), true);
      }
    }
  }
}
