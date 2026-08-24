#pragma once
// Thin wrapper around ihormelnyk OpenTherm library (MIT).

#include <Arduino.h>
#include <OpenTherm.h>
#include "hcs_ot_log.h"
#include "hcs_weather_comp.h"
#include "hcs_sensor_logic.h"
#include "hcs_ot_caps.h"

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
  // Diagnostics (MsgID 5 ASF flags, MsgID 115 OEM code)
  bool valid_asf = false;
  bool valid_oem = false;
  uint8_t asf_flags = 0;
  uint16_t oem_diag = 0;

  // Capabilities / identity (slow-read rotation)
  bool valid_pressure = false;
  float pressure_bar = NAN;
  bool valid_slave_cfg = false;
  uint8_t slave_config = 0, slave_member_id = 0;
  bool valid_master_cfg = false;
  uint8_t master_config = 0, master_member_id = 0;
  bool valid_capacity = false;
  uint8_t capacity_kw = 0, min_mod_pct = 0;
  bool valid_dhw_bounds = false;
  int8_t dhw_ub = -1, dhw_lb = -1;
  bool valid_maxtset_bounds = false;
  int8_t maxtset_ub = -1, maxtset_lb = -1;
  bool valid_fhb = false;
  uint8_t fhb_size = 0;
  uint8_t fhb_codes[8] = {0};   // first entries of the fault-history buffer
  uint8_t fhb_count = 0;        // how many entries were actually fetched
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

  /**
   * Desired DHW setpoint (°C). Clamped to boiler-reported bounds once
   * known; written as remote parameter TdhwSet (ID 56) in poll().
   * NAN disables the write (thermostat keeps control).
   */
  void setDhwSetpoint(float c) { dhw_setpoint_ = c; }
  float dhwSetpoint() const { return dhw_setpoint_; }

  /**
   * Failsafe mode: bypass weather compensation so the manual flow setpoint
   * (owner's connection-loss value) is used verbatim.
   */
  void setFailsafeHeat(bool on) { failsafe_ = on; }
  bool failsafeHeat() const { return failsafe_; }

  /** One lightweight read cycle for gateway reference mode (bus idle). */
  void referencePoll();

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

  // OT console: last master↔boiler exchanges (web UI + /api/otlog).
  hcs::OtLog ot_log;

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
  unsigned long xchg_(unsigned long req);
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
  float dhw_setpoint_ = NAN;   // NAN = thermostat controls DHW
  bool failsafe_ = false;
  uint32_t slow_cycle_ = 0;
  uint8_t poll_div_ = 0;            // slow-read throttle (every 3rd poll)
  unsigned long last_dhw_write_ms_ = 0;
  unsigned long fhb_last_fetch_ms_ = 0;
  bool fhb_fetched_once_ = false;

  void applyInject_() {
    if (inj_outdoor_ && inj_outdoor_->valid)
      snap_.outdoor_temp = inj_outdoor_->celsius;
    if (inj_return_ && inj_return_->valid) snap_.return_temp = inj_return_->celsius;
  }
  void slowRead_();      // one capability read per call
  bool indexedRead_(OpenThermMessageID id, uint8_t index, uint16_t& value);
  void fetchFhb_();
  void doPoll_();        // full ~1 Hz master cycle

#ifdef HCS_GW_ENABLE
  bool autopoll_ = true;
#endif
};
