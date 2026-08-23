#pragma once
// Thin wrapper around ihormelnyk OpenTherm library (MIT).

#include <Arduino.h>
#include <OpenTherm.h>
#include "hcs_weather_comp.h"
#include "hcs_sensor_logic.h"

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
  // Diagnostics (MsgID 5 ASF flags, MsgID 6 OEM code)
  bool valid_asf = false;
  bool valid_oem = false;
  uint8_t asf_flags = 0;
  uint16_t oem_diag = 0;
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

  /** Weather compensation: on/off + curve config CSV ("<ref>,<design>,<fmax>,<fmin>"). */
  void setWeatherComp(bool on) { wc_.enable = on; }
  bool setWeatherCompCfg(const char* csv);
  bool weatherComp() const { return wc_.enable; }
  const HcsWeatherComp& weatherCompCfg() const { return wc_; }
  /** Last effective (weather-compensated) target °C, NAN when inactive/unknown. */
  float wcTarget() const { return wc_target_; }

  bool chEnable() const { return ch_enable_; }
  bool dhwEnable() const { return dhw_enable_; }
  float flowSetpoint() const { return flow_setpoint_; }
  int maxModulation() const { return max_mod_; }

  const OtSnapshot& snap() const { return snap_; }

  /**
   * Register live 1-Wire readings. Assigned+fresh probes override the
   * OpenTherm-provided outdoor/return values in the snapshot (and thus
   * weather comp + telemetry). Pass nullptr to clear.
   */
  void setSensorInject(const hcs::TempValue* outdoor, const hcs::TempValue* ret) {
    inj_outdoor_ = outdoor;
    inj_return_ = ret;
  }

  /** Re-apply injections to the current snapshot (gateway path). */
  void applySensorInject() { applyInject_(); }

  /** Run one master transaction cycle (status + reads). ~1 Hz. */
  void poll();

#ifdef HCS_GW_ENABLE
  /** Gateway mode: demand-driven raw transaction; updates link health. */
  unsigned long sendRaw(unsigned long frame);
  OpenThermResponseStatus lastRawStatus() {
    return ot_.getLastResponseStatus();
  }
  /** Disable the autonomous ~1 Hz status cycle while gateway drives the bus. */
  void setAutopoll(bool on) { autopoll_ = on; }
#endif

 private:
  OpenTherm ot_;
  bool ch_enable_ = false;
  bool dhw_enable_ = true;
  float flow_setpoint_ = 45.0f;
  int max_mod_ = 100;
  HcsWeatherComp wc_;
  float wc_target_ = NAN;
  OtSnapshot snap_;
  unsigned long last_poll_ms_ = 0;
  const hcs::TempValue* inj_outdoor_ = nullptr;
  const hcs::TempValue* inj_return_ = nullptr;

  void applyInject_() {
    if (inj_outdoor_ && inj_outdoor_->valid)
      snap_.outdoor_temp = inj_outdoor_->celsius;
    if (inj_return_ && inj_return_->valid) snap_.return_temp = inj_return_->celsius;
  }
#ifdef HCS_GW_ENABLE
  bool autopoll_ = true;
#endif
};
