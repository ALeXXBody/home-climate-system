#pragma once

#include <Arduino.h>
#include "settings_store.h"
#include "ot_master.h"

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

  bool wifiConnected() const;
  String localIp() const;

 private:
  OtMaster& ot_;
  bool http_started_ = false;
  String node_id_;
  HcsSettings settings_;
  bool reboot_pending_ = false;
  unsigned long reboot_at_ms_ = 0;

  void scheduleReboot(unsigned long delayMs = 500);
};
