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

class NetServices {
 public:
  NetServices(OtMaster& ot);

  /** Connect WiFi via saved creds or captive portal. Blocks until associated or portal timeout. */
  bool beginWifi(HcsSettings& settings);

  /** Start HTTP status page + ElegantOTA on port 80. Call after WiFi up. */
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

#if defined(ESP32) && defined(HCS_GW_ENABLE)
  /** Optional: expose gateway counters in /api/status. */
  void setGateway(hcs::OtGateway* gw) { gw_ = gw; }
#endif

 private:
  OtMaster& ot_;
  bool http_started_ = false;
  String node_id_;
  HcsSettings settings_;
  bool reboot_pending_ = false;
  bool ota_busy_ = false;
  unsigned long ota_last_report_ms_ = 0;
  int ota_last_progress_ = -1;
  std::function<void(const String& json)> ota_report_;
  std::function<void(const String& json)> cfg_report_;

  void otaReport(const String& state, int progress, const String& error);
  unsigned long reboot_at_ms_ = 0;
  hcs::HcsSensors* sensors_ = nullptr;
  HcsSettings* shared_ = nullptr;
  hcs::FsState* fs_state_ptr_ = nullptr;
  String ap_name_;
  String reset_reason_ = "unknown";
  uint8_t unclean_boots_ = 0;

#if defined(ESP32) && defined(HCS_GW_ENABLE)
  hcs::OtGateway* gw_ = nullptr;
#endif

  void scheduleReboot(unsigned long delayMs = 500);
};
