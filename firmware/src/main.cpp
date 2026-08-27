/**
 * Home Climate System — OpenTherm master firmware
 *
 * - Captive portal (WiFiManager) for WiFi + MQTT
 * - HTTP status + ElegantOTA + ArduinoOTA
 * - MQTT native hcs/ topics for Home Climate Control
 */

#include <Arduino.h>
#include <ArduinoJson.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

#include "config.h"
#include "ot_master.h"
#include "mqtt_bridge.h"
#include "settings_store.h"
#include "net_services.h"
#if defined(ESP32)
#include <esp_system.h>
#include <Preferences.h>
#endif
#include "hcs_status_led.h"
#include "hcs_sys_log.h"

#ifdef HCS_GW_ENABLE
#if !defined(ESP32)
#error "HCS_GW_ENABLE requires ESP32 (gateway is ESP32-only)"
#endif
#include "ot_gateway.h"
#endif
#include "hcs_sensors.h"

static OtMaster ot(OT_IN_PIN, OT_OUT_PIN);
static WiFiClient wifiClient;
static MqttBridge mqtt(wifiClient, ot);
static NetServices net(ot);
static SettingsStore store;
static HcsSettings settings;
static String nodeId;
static hcs::HcsSensors sensors;
static hcs::TempValue sens_outdoor, sens_return;

// ---- connection-loss failsafe -------------------------------------------
static hcs::FsState fs_state = hcs::FsState::CONNECTED;
static bool fs_prev_ch = false;
static float fs_prev_flow = NAN;

/** Apply (or undo) the failsafe heat demand. */
static void applyFailsafe(bool active) {
  if (active) {
    fs_prev_ch = ot.chEnable();
    fs_prev_flow = ot.flowSetpoint();
    ot.setFailsafeHeat(true);
    if (settings.fs_enable) {
      ot.setChEnable(true);
      ot.setFlowSetpoint(settings.fs_flow_c);
    }
    Serial.printf("[fs] FAILSAFE engaged: CH on @ %.1f C\n",
                  settings.fs_flow_c);
  } else {
    ot.setFailsafeHeat(false);
    if (ot.chEnable() && !fs_prev_ch) ot.setChEnable(fs_prev_ch);
    if (!isnan(fs_prev_flow)) ot.setFlowSetpoint(fs_prev_flow);
    Serial.println("[fs] failsafe released - head-end back in control");
  }
}

static void failsafeLoop(bool link_up) {
  static unsigned long lost_since_ms = 0;
  unsigned long now = millis();

  if (!settings.mqtt_host.length()) return;  // standalone device: n/a

  if (link_up) {
    lost_since_ms = 0;
    if (fs_state != hcs::FsState::CONNECTED) {
      applyFailsafe(false);
      fs_state = hcs::FsState::CONNECTED;
    }
    return;
  }

  if (lost_since_ms == 0) lost_since_ms = now;
  unsigned long grace_ms = (unsigned long)settings.fs_grace_min * 60000UL;
  hcs::FsState st =
      hcs::fs_evaluate(settings.fs_enable, false, now - lost_since_ms,
                       grace_ms);
  if (st != fs_state) {
    if (st == hcs::FsState::FAILSAFE) applyFailsafe(true);
    fs_state = st;
  }
}

/** Refresh the live sensor readings OtMaster injects into its snapshot. */
static void syncSensorInject() {
  unsigned long now = millis();
  hcs::TempValue o = sensors.roleValue(hcs::OW_ROLE_OUTDOOR, now);
  hcs::TempValue r = sensors.roleValue(hcs::OW_ROLE_RETURN, now);
  sens_outdoor = o;
  sens_return = r;
  ot.setSensorInject(&sens_outdoor, &sens_return);
}

#ifdef HCS_GW_ENABLE
static hcs::OtGateway gw(ot, OT2_IN_PIN, OT2_OUT_PIN);
#endif

static String makeNodeId() {
  String m = WiFi.macAddress();
  m.replace(":", "");
  m.toLowerCase();
  return "hcs-" + m;
}

static void onOtaUrl(const String& url) {
  Serial.printf("[mqtt] OTA URL received: %s\n", url.c_str());
  net.startHttpUpdate(url);
}

/** Live OTA progress -> MQTT (HA Firmware tab progress bar / failures). */
static void onOtaProgress(const String& json) {
  if (nodeId.length()) {
    mqtt.publish("hcs/" + nodeId + "/ota", json, false);
    Serial.printf("[ota] %s\n", json.c_str());
  }
}

/** Retained settings snapshot -> MQTT so HA mirrors board settings. */
static void publishCfgSnapshot() {
  if (nodeId.length()) {
    mqtt.publish("hcs/" + nodeId + "/cfg", net.settingsSnapshotJson(), true);
  }
}

static bool cfg_announced = false;

/** Settings pushed over MQTT by HCC: same JSON as POST /api/settings. */
static void onSettingsJson(const String& json) {
  Serial.println("[mqtt] settings update received");
  net.applySettingsJson(json);  // persists + republishes + reboots
}

/** Failsafe config from MQTT (HCC) or web UI: apply + persist immediately. */
static void onFailsafeCfg(const String& json) {
  JsonDocument d;
  DeserializationError e = deserializeJson(d, json);
  if (e) {
    Serial.printf("[fs] bad cfg payload: %s\n", e.c_str());
    return;
  }
  if (d["enable"].is<bool>()) settings.fs_enable = d["enable"].as<bool>();
  if (d["flow"].is<float>()) {
    float f = d["flow"].as<float>();
    if (f < 20) f = 20;
    if (f > 90) f = 90;
    settings.fs_flow_c = f;
  }
  if (d["grace_min"].is<int>()) {
    int g = d["grace_min"].as<int>();
    settings.fs_grace_min = (uint8_t)constrain(g, 1, 120);
  }
  SettingsStore st;
  st.begin();
  st.save(settings);
  if (fs_state == hcs::FsState::FAILSAFE && settings.fs_enable) {
    ot.setChEnable(true);
    ot.setFlowSetpoint(settings.fs_flow_c);
  }
  Serial.printf("[fs] cfg: enable=%d flow=%.1f grace=%u min\n",
                settings.fs_enable, settings.fs_flow_c, settings.fs_grace_min);
}

static void applyWcSettings(const HcsSettings& s) {
  ot.setWeatherComp(s.wc_enable);
  char cfg[48];
  snprintf(cfg, sizeof(cfg), "%.1f,%.1f,%.1f,%.1f", s.wc_t_out_ref,
           s.wc_t_out_design, s.wc_flow_max, s.wc_flow_min);
  ot.setWeatherCompCfg(cfg);
}

/** Persist WC changes made at runtime via MQTT/web (only when they differ). */static void syncWcFromDevice() {
  static unsigned long last = 0;
  unsigned long now = millis();
  if (now - last < 5000) return;
  last = now;

  const HcsWeatherComp& c = ot.weatherCompCfg();
  if (ot.weatherComp() == settings.wc_enable &&
      c.t_out_ref == settings.wc_t_out_ref &&
      c.t_out_design == settings.wc_t_out_design &&
      c.flow_max == settings.wc_flow_max &&
      c.flow_min == settings.wc_flow_min) return;

  settings.wc_enable = ot.weatherComp();
  settings.wc_t_out_ref = c.t_out_ref;
  settings.wc_t_out_design = c.t_out_design;
  settings.wc_flow_max = c.flow_max;
  settings.wc_flow_min = c.flow_min;
  SettingsStore store;
  store.begin();
  store.save(settings);
}

#ifdef HCS_GW_ENABLE
static void requestGwMode(uint8_t cfg) {
  if (cfg > HCS_GW_GATEWAY) return;
  settings.gw_cfg = cfg;
  SettingsStore st;
  st.begin();
  st.save(settings);
  Serial.printf("[gw] role -> %s, rebooting\n", hcs_gw_cfg_name(cfg));
  delay(200);
  ESP.restart();
}

static bool gw_active = false;
static bool gw_probing = false;
static unsigned long gw_probe_start_ms = 0;
static const unsigned long kGwProbeMs = hcs::kGwAutoWindowMs;

/** Wire up (or tear down) runtime gateway services for the chosen role. */
static void applyGwRole(bool gateway) {
  if (gateway) {
    ot.setAutopoll(false);  // master bus is driven by gateway forwarding now
    gw.activate();          // slave RX already running during auto-probe
    net.setGateway(&gw);
    Serial.println("[gw] gateway mode ACTIVE");
  } else {
    net.setGateway(nullptr);
    Serial.println("[gw] master-only mode");
  }
  gw_active = gateway;
  mqtt.setGateway(gateway ? &gw : nullptr, gateway ? "gateway" : "master_only");
}
#endif

StatusLed status_led;  // diagnostic patterns — see hcs_status_led.h

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println(F("=== Home Climate System ==="));
  Serial.printf("FW %s  board %s  OT_IN=%d OT_OUT=%d\n", HCS_FW_VERSION,
                HCS_BOARD_NAME, OT_IN_PIN, OT_OUT_PIN);

  // ---- Power health: classify this boot and count unclean ones. -------
  // A supply that browns out under radio load shows up here as a growing
  // counter with reset_reason "BROWNOUT" — visible in /api/status and MQTT
  // instead of a board that silently vanishes from the network.
  String reset_reason;
  uint8_t unclean_boots = 0;
  bool clean_boot = true;
#if defined(ESP32)
  {
    int rr = esp_reset_reason();
    switch (rr) {
      case ESP_RST_BROWNOUT: reset_reason = "BROWNOUT"; clean_boot = false; break;
      case ESP_RST_PANIC:    reset_reason = "PANIC";    clean_boot = false; break;
      case ESP_RST_TASK_WDT: reset_reason = "TASK_WDT"; clean_boot = false; break;
      case ESP_RST_INT_WDT:  reset_reason = "INT_WDT";  clean_boot = false; break;
      case ESP_RST_WDT:      reset_reason = "WDT";      clean_boot = false; break;
      case ESP_RST_POWERON:  reset_reason = "POWERON";  break;
      case ESP_RST_SW:       reset_reason = "SW_RESET"; break;
      case ESP_RST_DEEPSLEEP:reset_reason = "DEEPSLEEP";break;
      default:               reset_reason = "UNKNOWN";  break;
    }
  }
#elif defined(ESP8266)
  {
    String r = ESP.getResetReason();
    if (r == "Power on" || r == "Software/System restart" ||
        r == "Deep-Sleep Wake") {
      reset_reason = r;
    } else {
      reset_reason = r.length() ? r : String("UNKNOWN");
      clean_boot = false;
    }
  }
#else
  reset_reason = "UNKNOWN";
#endif
  {
    // Count consecutive unclean boots in NVS so a brownout loop is a
    // number you can read, not a mystery.
#ifdef ESP32
    Preferences pp;
    pp.begin("hcs", false);
    unclean_boots = pp.getUChar("unclean_boots", 0);
    if (clean_boot) {
      unclean_boots = 0;
    } else if (unclean_boots < 250) {
      unclean_boots += 1;
    }
    pp.putUChar("unclean_boots", unclean_boots);
    pp.end();
#else
    // ESP8266: the reset-reason string above already tells the story;
    // a persistent unclean-boot counter would need an EEPROM byte slot.
    unclean_boots = clean_boot ? 0 : 1;
#endif
    Serial.printf("[power] boot reason: %s · unclean boots: %u\n",
                  reset_reason.c_str(), unclean_boots);
    HCS_LOG("boot", "reason=%s unclean=%u fw=%s board=%s",
            reset_reason.c_str(), unclean_boots, HCS_FW_VERSION, HCS_BOARD_NAME);
  }
  net.setPowerInfo(reset_reason, unclean_boots);
  status_led.begin();


#if CH_FAILSAFE_OFF_ON_BOOT
  ot.setChEnable(false);
#else
  ot.setChEnable(DEFAULT_CH_ENABLE);
#endif
  ot.setDhwEnable(DEFAULT_DHW_ENABLE);
  ot.setFlowSetpoint(DEFAULT_FLOW_SETPOINT_C);
  ot.setMaxModulation(DEFAULT_MAX_MODULATION);
#ifdef HCS_TEST_BOOT
  // Simulated OT bus is an unterminated floating wire — bias it stable
  pinMode(OT_IN_PIN, INPUT_PULLUP);
  pinMode(OT_OUT_PIN, OUTPUT);
  Serial.println(F("[dbg] pre ot.begin"));
#endif
  ot.begin();
#ifdef HCS_TEST_BOOT
  Serial.println(F("[dbg] post ot.begin"));
#endif

  store.begin();
  if (!store.load(settings)) {
    // Optional compile-time seeds
    settings.wifi_ssid = WIFI_SSID;
    settings.wifi_pass = WIFI_PASS;
    settings.mqtt_host = MQTT_HOST;
    settings.mqtt_port = MQTT_PORT;
    settings.mqtt_user = MQTT_USER;
    settings.mqtt_pass = MQTT_PASS;
    settings.mqtt_prefix = MQTT_PREFIX;
    settings.configured = settings.wifi_ssid.length() > 0;
  }
#ifdef HCS_TEST_BOOT
  Serial.println(F("[dbg] post store"));
#else
  applyWcSettings(settings);
  sensors.configure(settings.ow_enable, settings.ow_slots, hcs::kOwMaxSlots);
  if (HCS_ONEWIRE_PIN >= 0) sensors.begin();
  net.setSensors(&sensors);
  mqtt.setSensors(&sensors);
#endif

#ifdef HCS_TEST_BOOT
  // Headless simulation (Wokwi/CI): skip blocking portal + network services
  Serial.println(F("[wifi] TEST_BOOT: skipping WiFi portal"));
  Serial.println(F("[http] TEST_BOOT: skipping HTTP/MQTT services"));
  nodeId = makeNodeId();
  Serial.printf("[boot] node %s ready (test mode)\n", nodeId.c_str());
#else
  // Unique default device name: "Home Climate System" alone made two
  // devices indistinguishable in router lists and HCC. (Runs on real
  // hardware only — inside the #else branch.)
  {
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    String uniq = mac.substring(mac.length() - 4);
    uniq.toUpperCase();
    String defName = "Home Climate " + uniq;
    if (settings.device_name.length() == 0 ||
        settings.device_name == "Home Climate System") {
      settings.device_name = defName;
      SettingsStore st;
      st.begin();
      st.save(settings);
    }
  }

  if (!net.beginWifi(settings)) {
    HCS_LOG("boot", "no WiFi — restarting in 10s");
    delay(10000);
    ESP.restart();
  }
  HCS_LOG("wifi", "up ip=%s rssi=%d",
          WiFi.localIP().toString().c_str(), WiFi.RSSI());

  nodeId = makeNodeId();
  HCS_LOG("boot", "node=%s starting HTTP", nodeId.c_str());
  net.beginHttp(settings, nodeId);
  net.beginArduinoOta(settings, nodeId);
  net.setOtaReporter(onOtaProgress);
  net.setMqttConnectedFn([] { return mqtt.connected(); });
  net.setSharedSettings(&settings);
  net.setFailsafeStatePtr(&fs_state);

  mqtt.setNodeId(nodeId);
  mqtt.setDeviceInfo(settings.device_name, net.localIp());
  mqtt.onOtaUrl(onOtaUrl);
  mqtt.onSettings(onSettingsJson);
  net.setConfigReporter([](const String& j) { publishCfgSnapshot(); });
  // ensure the lambda above can reach the retained publisher even before
  // nodeId-dependent paths run: it calls publishCfgSnapshot directly.
  mqtt.onFailsafeCfg(onFailsafeCfg);
  mqtt.setFailsafeStatePtr(&fs_state);

  if (settings.mqtt_host.length()) {
    mqtt.begin(settings.mqtt_host.c_str(), settings.mqtt_port,
               settings.mqtt_user.c_str(), settings.mqtt_pass.c_str());
    Serial.printf("[mqtt] broker %s:%u node %s\n", settings.mqtt_host.c_str(),
                  settings.mqtt_port, nodeId.c_str());
  } else {
    Serial.println(F("[mqtt] no broker configured — portal MQTT host empty"));
  }

#ifdef HCS_GW_ENABLE
  if (settings.gw_cfg == HCS_GW_GATEWAY) {
    ot.setAutopoll(false);
    gw.begin();
    applyGwRole(true);
    Serial.printf("[gw] forced gateway — thermostat bus %d/%d\n", OT2_IN_PIN,
                  OT2_OUT_PIN);
  } else if (settings.gw_cfg == HCS_GW_MASTER_ONLY) {
    applyGwRole(false);
  } else {
    // AUTO: silent listen on the thermostat bus; decide after kGwProbeMs
    gw.beginProbe();
    gw_probing = true;
    gw_probe_start_ms = millis();
    Serial.printf("[gw] auto-detect: listening on %d/%d for %lus\n",
                  OT2_IN_PIN, OT2_OUT_PIN, kGwProbeMs / 1000);
  }
  mqtt.onGwMode(requestGwMode);
  mqtt.onGwOverride([](float c) { gw.setOverrideSetpointC(c); });
#endif

  Serial.println(F("[boot] ready — open http://device-ip/ for status & OTA"));
#endif
}

void loop() {
  status_led.update(WiFi.status() == WL_CONNECTED, ot.snap().valid,
                    fs_state == hcs::FsState::FAILSAFE);
#ifdef HCS_TEST_BOOT
  static unsigned long last_tick = 0;
#endif
  ot.loop();
#ifndef HCS_TEST_BOOT
  sensors.loop();
  syncSensorInject();
#endif

#ifndef HCS_TEST_BOOT
  net.loop();
  syncWcFromDevice();

  if (WiFi.status() != WL_CONNECTED) {
    // brief reconnect attempt; portal not re-opened automatically
    static unsigned long last = 0;
    if (millis() - last > 15000) {
      last = millis();
      WiFi.reconnect();
    }
    failsafeLoop(false);
  } else if (settings.mqtt_host.length()) {
    // keep IP fresh for discovery
    mqtt.setDeviceInfo(settings.device_name, net.localIp());
    mqtt.loop();
    // Announce the retained settings snapshot once per broker session so
    // HA always has fresh board settings (also republished on every change).
    if (mqtt.connected()) {
      if (!cfg_announced) {
        publishCfgSnapshot();
        cfg_announced = true;
      }
    } else {
      cfg_announced = false;
    }
    failsafeLoop(mqtt.connected());
  } else {
    failsafeLoop(true);  // no broker configured = standalone, never failsafe
  }
#endif

#ifdef HCS_GW_ENABLE
  if (gw_probing) {
    // AUTO phase: keep boiler autonomy, listen on thermostat bus silently
    ot.poll();
    gw.probeLoop();
    int d = hcs::gw_autodetect_decide(gw.probeValidRequests(),
                                      millis() - gw_probe_start_ms);
    if (d) {
      Serial.printf("[gw] auto-detect -> %s (%lu valid requests in %lus)\n",
                    d == HCS_GW_GATEWAY ? "gateway" : "master_only",
                    gw.probeValidRequests(),
                    (millis() - gw_probe_start_ms) / 1000);
      gw_probing = false;
      applyGwRole(d == HCS_GW_GATEWAY);
    }
  } else if (gw_active) {
    gw.loop();  // answers thermostat + forwards to boiler on demand
    ot.applySensorInject();  // keep telemetry sensor-correct between forwards
    // Reference mode: when the wall thermostat goes silent, keep light
    // monitoring alive (status + diagnostics + capabilities) like the PIC
    // gateway's M=G does — never writing setpoints the thermostat owns.
    static unsigned long last_ref_ms = 0;
    if (!gw.thermostatOnline(10000) && millis() - last_ref_ms > 60000) {
      last_ref_ms = millis();
      ot.referencePoll();
      ot.applySensorInject();
      mqtt.publishTelemetry(ot.snap());  // refresh retained topics
    }
  } else {
    ot.poll();  // autonomous ~1 Hz master cycle (injects sensors inside poll)
  }
#else
  ot.poll();  // autonomous ~1 Hz master cycle (master-only builds)
#endif

#ifdef HCS_TEST_BOOT
  if (millis() - last_tick >= 2000) {
    last_tick = millis();
    Serial.printf("[tick] uptime=%lus\n", millis() / 1000);
  }
#endif
  delay(1);
}
