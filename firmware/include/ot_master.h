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
  /** True when outdoor/return came from OpenTherm this cycle (not 1-Wire). */
  bool outdoor_from_ot = false;
  bool return_from_ot = false;
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
  uint8_t fhb_codes[8] = {0};
  uint8_t fhb_count = 0;
};

class OtMaster {
 public:
  OtMaster(int in_pin, int out_pin);

  void begin();
  /** Call very frequently from loop (handles OT response timing). */
  void loop();

  void setChEnable(bool on);
  void setDhwEnable(bool on);
  void setFlowSetpoint(float celsius);
  void setMaxModulation(int percent);

  void setWeatherComp(bool on) { wc_.enable = on; }
  bool setWeatherCompCfg(const char* csv);
  bool weatherComp() const { return wc_.enable; }
  const HcsWeatherComp& weatherCompCfg() const { return wc_; }
  float wcTarget() const { return wc_target_; }

  void setDhwSetpoint(float c) { dhw_setpoint_ = c; }
  float dhwSetpoint() const { return dhw_setpoint_; }

  void setFailsafeHeat(bool on) { failsafe_ = on; }
  bool failsafeHeat() const { return failsafe_; }

  void referencePoll();

  bool chEnable() const { return ch_enable_; }
  bool dhwEnable() const { return dhw_enable_; }
  float flowSetpoint() const { return flow_setpoint_; }
  int maxModulation() const { return max_mod_; }

  const OtSnapshot& snap() const { return snap_; }

  /**
   * 1-Wire outdoor/return backfill only when OT did not report that channel.
   * Boiler OT always wins when valid.
   */
  void setSensorInject(const hcs::TempValue* outdoor, const hcs::TempValue* ret) {
    inj_outdoor_ = outdoor;
    inj_return_ = ret;
  }

  void applySensorInject() { applyInject_(); }

  /** Run one master transaction cycle. ~1 Hz, thin (HTTP-friendly). */
  void poll();

  hcs::OtLog ot_log;

#ifdef HCS_GW_ENABLE
  unsigned long sendRaw(unsigned long frame);
  OpenThermResponseStatus lastRawStatus() {
    return last_status_;
  }
  void setAutopoll(bool on) { autopoll_ = on; }
#endif

 private:
  /**
   * Full frame exchange, all in TASK context (no ISR, by design):
   *   1. bit-bang the request (library-identical timing)
   *   2. poll-decode the response by sampling OT_IN (~25 kHz)
   *
   * Why no interrupt: on ESP32-C3 an OT edge arriving while a flash op
   * (LittleFS/NVS) has the cache disabled crashed every ISR-based variant
   * we shipped (1.4.5 PANICs, 1.4.6 brick, 1.4.8 bootloop). sendRequest()
   * already blocked for the whole exchange window — polling inside the
   * same window is electrically identical and cannot hit IRQ hazards.
   */
  void sendBit_(bool high);
  unsigned long receiveFrame_(unsigned long timeout_ms);

  unsigned long xchg_(unsigned long req);
  bool xchgOk_() const {
    return last_status_ == OpenThermResponseStatus::SUCCESS;
  }
  void noteStatusOk_(unsigned long now);
  void noteStatusFail_();

  OpenTherm ot_;  // parsing/build helpers only — its ISR is NEVER attached
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
  float dhw_setpoint_ = NAN;
  bool failsafe_ = false;
  uint32_t slow_cycle_ = 0;
  uint8_t rot_ = 0;                 // optional-read round robin
  uint8_t status_fail_streak_ = 0;  // hysteresis before clearing ot_valid
  OpenThermResponseStatus last_status_ = OpenThermResponseStatus::NONE;
  unsigned long last_dhw_write_ms_ = 0;
  unsigned long last_mm_write_ms_ = 0;
  unsigned long last_outdoor_ot_ms_ = 0;
  unsigned long last_return_ot_ms_ = 0;
  unsigned long fhb_last_fetch_ms_ = 0;
  bool fhb_fetched_once_ = false;

  void applyInject_() {
    if (!snap_.outdoor_from_ot && inj_outdoor_ && inj_outdoor_->valid)
      snap_.outdoor_temp = inj_outdoor_->celsius;
    if (!snap_.return_from_ot && inj_return_ && inj_return_->valid)
      snap_.return_temp = inj_return_->celsius;
  }
  void slowRead_();
  bool indexedRead_(OpenThermMessageID id, uint8_t index, uint16_t& value);
  void fetchFhb_();
  void doPoll_();
  void writeTSet_();
  void readFloat_(OpenThermMessageID id, float& dest, float lo, float hi,
                  bool* from_ot_flag);

#ifdef HCS_GW_ENABLE
  bool autopoll_ = true;
#endif
};
