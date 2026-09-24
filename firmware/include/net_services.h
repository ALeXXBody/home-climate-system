#pragma once

#include <Arduino.h>
#include <functional>
#include "settings_store.h"
#include "ot_master.h"
#include "hcs_failsafe.h"

#if defined(ESP32) && defined(HCS_GW_ENABLE)
namespace hcs {
class OtGateway;
}
#endif

namespace hcs {
class HcsSensors;
}

/** Lowercased MAC with colons stripped — the device's stable public id. */
inline String hcs_mac_token(const String& mac) {
  String m = mac;
  m.replace(":", "");
  m.toLowerCase();
  return m;
}

/** Default admin/OTA password derived from the MAC, so the device is not
 *  wide-open by default yet remains recoverable (the MAC is the node id shown
 *  in the panel / MQTT discovery). Printed at boot; replace via the portal. */
inline String hcs_default_admin_password(const String& mac) {
  return "hcs" + hcs_mac_token(mac);
}

class NetServices {
 public:
  NetServices(OtMaster& ot);

  /** Connect WiFi via saved creds or captive portal. Blocks until associated or portal timeout. */
  bool beginWifi(HcsSettings& settings);

  /** Start HTTP status page on port 80. OTA lives at POST /api/ota {"url":...}.
   *  Manual flashes: ArduinoOTA (pio run -t upload --upload-port <ip>). Call after WiFi up. */
  void beginHttp(const HcsSettings& settings, const String& nodeId);

  /** ArduinoOTA (IDE / pio ota). */
  void beginArduinoOta(const HcsSettings& settings, const String& hostname);

  void loop();

  /** Trigger HTTP OTA from a firmware URL (HA Firmware tab). */
  bool startHttpUpdate(const String& url);

  /**
   * Install a reporter invoked with JSON progress payloads during OTA.
   * main.cpp wires this to MQTT topic hcs/<node>/ota so HA can render a
   * live progress bar and surface failure reasons.
   */
  void setOtaReporter(std::function<void(const String& json)> fn) {
    ota_report_ = std::move(fn);
  }

  /** LED command handler ("on"/"off"/brightness) — wired to main.cpp. */
  void setLedFn(std::function<void(const String& payload)> fn) {
    led_fn_ = std::move(fn);
  }

  /**
   * Reporter invoked with the masked settings snapshot whenever settings
   * change (portal or MQTT). main.cpp wires it to the retained topic
   * hcs/<node>/cfg so HA mirrors board settings both ways.
   */
  void setConfigReporter(std::function<void(const String& json)> fn) {
    cfg_report_ = std::move(fn);
  }

  /** Masked settings JSON — same shape as GET /api/settings. */
  String settingsSnapshotJson() const;

  /**
   * Apply a partial settings update (same fields as POST /api/settings),
   * persist it, publish the new snapshot and schedule a reboot.
   * Shared by the HTTP endpoint and the MQTT .../set/settings command.
   */
  bool applySettingsJson(const String& json);

  bool wifiConnected() const;
  /** Active Wi-Fi re-association with escalation; safe to call from loop. */
  void bulletproofWifiTick();
  unsigned int wifi_fail_count_ = 0;
  unsigned int wifi_force_count_ = 0;
  unsigned long wifi_down_since_ms_ = 0;  // 0 = connected
  String localIp() const;

  /** Optional: expose live 1-Wire probes to the Sensors tab. */
  void setSensors(hcs::HcsSensors* s) { sensors_ = s; }

  /** Single source of truth for settings edited without reboot (failsafe). */
  void setSharedSettings(HcsSettings* s) { shared_ = s; }
  /** Live failsafe state owned by main loop. */
  void setFailsafeStatePtr(hcs::FsState* p) { fs_state_ptr_ = p; }

  // Power-health telemetry: why the chip last rebooted and how many
  // unclean boots (brownout/panic/watchdog) happened since the last
  // clean power-on. Surfaces unstable supply situations in the UI.
  void setPowerInfo(const String& reset_reason, uint8_t unclean_boots) {
    reset_reason_ = reset_reason;
    unclean_boots_ = unclean_boots;
  }

  /** Last scheduled-reboot reason (empty if never scheduled this boot). */
  const String& lastRebootReason() const { return last_reboot_reason_; }

#if defined(ESP32) && defined(HCS_GW_ENABLE)
  /** Optional: expose gateway counters in /api/status. */
  void setGateway(hcs::OtGateway* gw) { gw_ = gw; }
#endif

  /** Wire MQTT liveness probe used by the rollback watchdog. */
  void setMqttConnectedFn(std::function<bool()> fn) { mqtt_ok_fn_ = std::move(fn); }

 private:
  OtMaster& ot_;
  std::function<bool()> mqtt_ok_fn_;
  // ---- OTA rollback watchdog state (see otaRollbackTick) ----
  bool     roll_pending_ = false;
  uint8_t  roll_attempts_ = 0;
  String   roll_target_url_;
  String   roll_good_url_;
  bool     roll_loaded_ = false;
  bool     roll_saw_health_ = false;  // MQTT or OT observed healthy this boot
  bool http_started_ = false;
  String node_id_;
  HcsSettings settings_;
  HcsSettings& liveCfg() { return shared_ ? *shared_ : settings_; }
  const HcsSettings& liveCfg() const { return shared_ ? *shared_ : settings_; }
  bool reboot_pending_ = false;
  bool ota_busy_ = false;
  unsigned long ota_last_report_ms_ = 0;
  int ota_last_progress_ = -1;
  void otaRollLoad_();
  void otaRollSave_();
  /** Mark a firmware URL as pending-rollback target (called by startHttpUpdate). */
  void otaMarkTarget(const String& url);
  void otaRollbackTick();
  std::function<void(const String& json)> ota_report_;
  std::function<void(const String& payload)> led_fn_;
  std::function<void(const String& json)> cfg_report_;

  void otaReport(const String& state, int progress, const String& error);
  unsigned long reboot_at_ms_ = 0;
  hcs::HcsSensors* sensors_ = nullptr;
  HcsSettings* shared_ = nullptr;
  hcs::FsState* fs_state_ptr_ = nullptr;
  String ap_name_;
  String reset_reason_ = "unknown";
  String last_reboot_reason_;
  uint8_t unclean_boots_ = 0;
  String session_token_;  // active web session token (empty = none)

#if defined(ESP32) && defined(HCS_GW_ENABLE)
  hcs::OtGateway* gw_ = nullptr;
#endif

  void scheduleReboot(unsigned long delayMs = 500,
                      const char* reason = "scheduled");
#if defined(ESP32)
  /** Signed, streamed firmware update: download .bin (+ .sig), hash it,
   *  verify the ECDSA signature against the baked-in key, then boot it. */
  bool signedHttpUpdate(const String& url);
#endif
};
