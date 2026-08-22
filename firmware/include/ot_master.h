#pragma once
// Thin wrapper around ihormelnyk OpenTherm library (MIT).

#include <Arduino.h>
#include <OpenTherm.h>

struct OtSnapshot {
  bool valid = false;
  bool fault = false;
  bool ch_active = false;
  bool dhw_active = false;
  bool flame = false;
  float flow_temp = NAN;      // boiler water °C
  float return_temp = NAN;
  float outdoor_temp = NAN;
  float modulation = NAN;     // %
  float dhw_temp = NAN;
  unsigned long last_ok_ms = 0;
};

class OtMaster {
 public:
  OtMaster(int in_pin, int out_pin);

  void begin();
  /** Call very frequently from loop (handles OT response timing). */
  void loop();

  /** Desired commands from MQTT / app. */
  void setChEnable(bool on);
  void setDhwEnable(bool on);
  void setFlowSetpoint(float celsius);
  void setMaxModulation(int percent);

  bool chEnable() const { return ch_enable_; }
  bool dhwEnable() const { return dhw_enable_; }
  float flowSetpoint() const { return flow_setpoint_; }
  int maxModulation() const { return max_mod_; }

  const OtSnapshot& snap() const { return snap_; }

  /** Run one master transaction cycle (status + reads). ~1 Hz. */
  void poll();

 private:
  OpenTherm ot_;
  bool ch_enable_ = false;
  bool dhw_enable_ = true;
  float flow_setpoint_ = 45.0f;
  int max_mod_ = 100;
  OtSnapshot snap_;
  unsigned long last_poll_ms_ = 0;

  static void handleInterrupt();
  static OtMaster* instance_;
};
