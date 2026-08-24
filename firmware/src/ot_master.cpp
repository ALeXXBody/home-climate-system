#include "ot_master.h"
#include "config.h"

OtMaster::OtMaster(int in_pin, int out_pin) : ot_(in_pin, out_pin) {}

// NOTE: never register a custom ISR function pointer here. On ESP8266 a plain
// function lives in flash and panics ("ISR not in IRAM!") on the first bus
// edge; the library's no-arg begin() attaches its own IRAM-safe handler.
void OtMaster::begin() {
  ot_.begin();
}

// Single chokepoint for every bus exchange: records the frame pair in the
// OT console ring buffer so the web UI can replay the conversation.
unsigned long OtMaster::xchg_(unsigned long req) {
  unsigned long resp = ot_.sendRequest(req);
  ot_log.record(req, resp, ot_.getLastResponseStatus());
  return resp;
}

void OtMaster::loop() {
  // ISR-driven process() handles bit timing.
}

void OtMaster::setChEnable(bool on) { ch_enable_ = on; }
void OtMaster::setDhwEnable(bool on) { dhw_enable_ = on; }

void OtMaster::setFlowSetpoint(float celsius) {
  if (isnan(celsius)) return;
  // Clamp to boiler-reported bounds when known (MaxTSetUB/LB), else default
  float lo = snap_.valid_maxtset_bounds ? (float)snap_.maxtset_lb : 10.0f;
  float hi = snap_.valid_maxtset_bounds ? (float)snap_.maxtset_ub : 90.0f;
  if (lo < 1) lo = 1;  // never fully off via clamp
  celsius = hcs::ot_clamp_with_bounds(celsius, snap_.valid_maxtset_bounds,
                                      lo, hi, 10.0f, 90.0f);
  flow_setpoint_ = roundf(celsius * 2.0f) / 2.0f;
}

void OtMaster::setMaxModulation(int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  max_mod_ = percent;
}

bool OtMaster::setWeatherCompCfg(const char* csv) {
  HcsWeatherComp tmp = wc_;
  if (!hcs_weather_comp_parse_cfg(csv, tmp)) return false;
  wc_ = tmp;
  return true;
}

#ifdef HCS_GW_ENABLE
unsigned long OtMaster::sendRaw(unsigned long frame) {
  unsigned long resp = xchg_(frame);
  if (ot_.getLastResponseStatus() == OpenThermResponseStatus::SUCCESS) {
    snap_.valid = true;
    snap_.last_ok_ms = millis();
    if (OpenTherm::getMessageType(resp) >= OpenThermMessageType::READ_ACK) {
      // keep telemetry fields fresh from forwarded traffic where possible
      switch ((int)OpenTherm::getDataID(resp)) {
        case (int)OpenThermMessageID::Tboiler:
          snap_.flow_temp = OpenTherm::getFloat(resp);
          break;
        case (int)OpenThermMessageID::Tret:
          snap_.return_temp = OpenTherm::getFloat(resp);
          break;
        case (int)OpenThermMessageID::RelModLevel:
          snap_.modulation = OpenTherm::getFloat(resp);
          break;
        default:
          break;
      }
    }
    // Status responses carry flame/fault/ch flags
    if (OpenTherm::getDataID(resp) == OpenThermMessageID::Status &&
        OpenTherm::getMessageType(resp) == OpenThermMessageType::READ_ACK) {
      snap_.fault = OpenTherm::isFault(resp);
      snap_.ch_active = OpenTherm::isCentralHeatingActive(resp);
      snap_.dhw_active = OpenTherm::isHotWaterActive(resp);
      snap_.flame = OpenTherm::isFlameOn(resp);
    }
    // Harvest diagnostics from forwarded traffic
    if (OpenTherm::getMessageType(resp) >= OpenThermMessageType::READ_ACK) {
      if (OpenTherm::getDataID(resp) == OpenThermMessageID::ASFflags) {
        snap_.asf_flags = (uint8_t)OpenTherm::getUInt(resp);
        snap_.valid_asf = true;
      } else if (OpenTherm::getDataID(resp) ==
                 OpenThermMessageID::OEMDiagnosticCode) {
        snap_.oem_diag = (uint16_t)OpenTherm::getUInt(resp);
        snap_.valid_oem = true;
      }
    }
  } else {
    snap_.valid = false;
  }
  return resp;
}
#endif

void OtMaster::poll() {
#ifdef HCS_GW_ENABLE
  if (!autopoll_) return;
#endif
  doPoll_();
}

void OtMaster::referencePoll() {
  // Gateway reference mode: reduced read cycle, never writes setpoints —
  // the thermostat owns those. Keeps telemetry/diagnostics fresh while the
  // wall thermostat is silent.
#ifdef HCS_GW_ENABLE
  if (autopoll_) return;  // master-only mode already polls full cycles
#endif
  unsigned long now = millis();
  if (now - last_poll_ms_ < OT_STATUS_INTERVAL_MS) return;
  last_poll_ms_ = now;

  unsigned long response = xchg_(OpenTherm::buildRequest(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::Status, (ch_enable_ ? 0x0100 : 0) | 0x0000));
  if (ot_.getLastResponseStatus() != OpenThermResponseStatus::SUCCESS) {
    snap_.valid = false;
    return;
  }
  snap_.valid = true;
  snap_.last_ok_ms = now;
  snap_.fault = OpenTherm::isFault(response);
  snap_.ch_active = OpenTherm::isCentralHeatingActive(response);
  snap_.dhw_active = OpenTherm::isHotWaterActive(response);
  snap_.flame = OpenTherm::isFlameOn(response);

  // Diagnostics refresh only (ASF + OEM code)
  unsigned long r5 = xchg_(OpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA, OpenThermMessageID::ASFflags, 0));
  if (ot_.getLastResponseStatus() == OpenThermResponseStatus::SUCCESS &&
      OpenTherm::getDataID(r5) == OpenThermMessageID::ASFflags) {
    snap_.asf_flags = (uint8_t)OpenTherm::getUInt(r5);
    snap_.valid_asf = true;
  }
  unsigned long r115 = xchg_(OpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA, OpenThermMessageID::OEMDiagnosticCode,
      0));
  if (ot_.getLastResponseStatus() == OpenThermResponseStatus::SUCCESS &&
      OpenTherm::getDataID(r115) == OpenThermMessageID::OEMDiagnosticCode) {
    snap_.oem_diag = (uint16_t)OpenTherm::getUInt(r115);
    snap_.valid_oem = true;
  }

  if ((++poll_div_ % 3) == 0) slowRead_();
  applyInject_();
}

/** One capability read per call (round robin). */
void OtMaster::slowRead_() {
  OpenThermMessageID id =
      (OpenThermMessageID)hcs::ot_slow_read_id(slow_cycle_++);
  unsigned long resp = xchg_(
      OpenTherm::buildRequest(OpenThermMessageType::READ_DATA, id, 0));
  if (ot_.getLastResponseStatus() != OpenThermResponseStatus::SUCCESS ||
      OpenTherm::getDataID(resp) != id)
    return;
  uint16_t d = OpenTherm::getUInt(resp);
  switch ((uint16_t)id) {
    case 18:  // CHPressure f8.8 bar
      snap_.pressure_bar = d / 256.0f;
      snap_.valid_pressure = true;
      break;
    case 3:
      snap_.slave_config = (d >> 8) & 0xFF;
      snap_.slave_member_id = d & 0xFF;
      snap_.valid_slave_cfg = true;
      break;
    case 2:
      snap_.master_config = (d >> 8) & 0xFF;
      snap_.master_member_id = d & 0xFF;
      snap_.valid_master_cfg = true;
      break;
    case 15:
      snap_.capacity_kw = (d >> 8) & 0xFF;
      snap_.min_mod_pct = d & 0xFF;
      snap_.valid_capacity = true;
      break;
    case 48:  // TdhwSetUB/LB s8/s8
      snap_.dhw_ub = (int8_t)((d >> 8) & 0xFF);
      snap_.dhw_lb = (int8_t)(d & 0xFF);
      snap_.valid_dhw_bounds = true;
      break;
    case 49:  // MaxTSetUB/LB s8/s8
      snap_.maxtset_ub = (int8_t)((d >> 8) & 0xFF);
      snap_.maxtset_lb = (int8_t)(d & 0xFF);
      snap_.valid_maxtset_bounds = true;
      break;
    case 13:  // FHBsize u8 in low byte
      snap_.fhb_size = d & 0xFF;
      snap_.valid_fhb = true;
      // Audit F6: indexed reads used to run every rotation (~7 s) — bus spam.
      // Now: once per boot, then hourly.
      if (!fhb_fetched_once_ ||
          millis() - fhb_last_fetch_ms_ > 3600000UL) {
        fetchFhb_();
        fhb_last_fetch_ms_ = millis();
        fhb_fetched_once_ = true;
      }
      break;
    default:
      break;
  }
}

/**
 * Indexed fault-history entry: write index (ID 14 low byte), then read the
 * value back. Same handshake as transparent slave parameters. Returns the
 * entry value or -1 when the boiler declines.
 */
bool OtMaster::indexedRead_(OpenThermMessageID id, uint8_t index,
                            uint16_t& value) {
  unsigned long w = xchg_(OpenTherm::buildRequest(
      OpenThermMessageType::WRITE_DATA, id, index & 0xFF));
  if (ot_.getLastResponseStatus() != OpenThermResponseStatus::SUCCESS)
    return false;
  (void)w;
  unsigned long r = xchg_(OpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA, id, index & 0xFF));
  if (ot_.getLastResponseStatus() != OpenThermResponseStatus::SUCCESS ||
      OpenTherm::getDataID(r) != id)
    return false;
  value = OpenTherm::getUInt(r);
  return true;
}

void OtMaster::fetchFhb_() {
  // Cap at our storage size; entries are OEM-defined bytes.
  uint8_t want = snap_.fhb_size;
  if (want == 0 || want > sizeof(snap_.fhb_codes)) want = sizeof(snap_.fhb_codes);
  snap_.fhb_count = 0;
  for (uint8_t i = 0; i < want; i++) {
    uint16_t v;
    if (!indexedRead_(OpenThermMessageID::FHBindexFHBvalue, i, v)) break;
    snap_.fhb_codes[snap_.fhb_count++] = v & 0xFF;
  }
}

/** Full master cycle (~1 Hz): status, temps, WC, setpoint writes. */
void OtMaster::doPoll_() {
  unsigned long now = millis();
  if (now - last_poll_ms_ < OT_STATUS_INTERVAL_MS) {
    return;
  }
  last_poll_ms_ = now;

  unsigned long response = xchg_(OpenTherm::buildRequest(OpenThermMessageType::WRITE_DATA, OpenThermMessageID::Status, (ch_enable_ ? 0x0100 : 0) | (dhw_enable_ ? 0x0200 : 0)));
  OpenThermResponseStatus st = ot_.getLastResponseStatus();
  if (st != OpenThermResponseStatus::SUCCESS) {
    snap_.valid = false;
    return;
  }

  snap_.valid = true;
  snap_.last_ok_ms = now;
  snap_.fault = OpenTherm::isFault(response);
  snap_.ch_active = OpenTherm::isCentralHeatingActive(response);
  snap_.dhw_active = OpenTherm::isHotWaterActive(response);
  snap_.flame = OpenTherm::isFlameOn(response);

  // Diagnostics: ASF fault flags (ID 5) + OEM diagnostic code (ID 115)
  unsigned long r5 = xchg_(OpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA, OpenThermMessageID::ASFflags, 0));
  if (ot_.getLastResponseStatus() == OpenThermResponseStatus::SUCCESS &&
      OpenTherm::getDataID(r5) == OpenThermMessageID::ASFflags) {
    snap_.asf_flags = (uint8_t)OpenTherm::getUInt(r5);
    snap_.valid_asf = true;
  }
  unsigned long r115 = xchg_(OpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA, OpenThermMessageID::OEMDiagnosticCode,
      0));
  if (ot_.getLastResponseStatus() == OpenThermResponseStatus::SUCCESS &&
      OpenTherm::getDataID(r115) == OpenThermMessageID::OEMDiagnosticCode) {
    snap_.oem_diag = (uint16_t)OpenTherm::getUInt(r115);
    snap_.valid_oem = true;
  }

  // Outdoor temperature first (MsgID 27) so weather comp can use it this cycle
  unsigned long req = OpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA, OpenThermMessageID::Toutside, 0);
  unsigned long resp = xchg_(req);
  if (ot_.getLastResponseStatus() == OpenThermResponseStatus::SUCCESS) {
    snap_.outdoor_temp = OpenTherm::getFloat(resp);
  }

  applyInject_();  // 1-Wire probes may override outdoor/return before WC

  // Effective flow target. Failsafe bypasses weather compensation so the
  // owner's manual connection-loss setpoint is used verbatim.
  if (failsafe_) {
    wc_target_ = NAN;
  } else {
    wc_target_ = hcs_weather_comp_target(wc_, snap_.outdoor_temp);
    if (!isnan(wc_target_)) {
      // WC active: skip the manual write below, TSet comes from the curve
      if (ch_enable_) {
        ot_.setBoilerTemperature(wc_target_);
      }
    }
  }
  bool wc_used = !isnan(wc_target_) && !failsafe_;
  if (!wc_used && ch_enable_) {
    ot_.setBoilerTemperature(flow_setpoint_);
  }

  float t = ot_.getBoilerTemperature();
  if (!isnan(t)) snap_.flow_temp = t;

  t = ot_.getReturnTemperature();
  if (!isnan(t)) snap_.return_temp = t;

  t = ot_.getModulation();
  if (!isnan(t)) snap_.modulation = t;

  t = ot_.getDHWTemperature();
  if (!isnan(t)) snap_.dhw_temp = t;

  // Max relative modulation setting (MsgID 14)
  unsigned int mm = OpenTherm::temperatureToData((float)max_mod_);
  // temperatureToData is f8.8 for temps; for % level same f8.8 format works
  req = OpenTherm::buildRequest(OpenThermMessageType::WRITE_DATA,
                                OpenThermMessageID::MaxRelModLevelSetting,
                                (unsigned int)(max_mod_ * 256));
  xchg_(req);
  (void)mm;

  // DHW setpoint as remote parameter (ID 56). Audit F8: was written every
  // 1 Hz cycle; thermostats reaffirm ~once a minute — match that.
  if (!isnan(dhw_setpoint_) && snap_.valid_dhw_bounds &&
      millis() - last_dhw_write_ms_ >= 60000UL) {
    float c = hcs::ot_clamp_with_bounds(dhw_setpoint_, true, snap_.dhw_lb,
                                        snap_.dhw_ub, 35, 60);
    unsigned long dhw_req = OpenTherm::buildRequest(
        OpenThermMessageType::WRITE_DATA, OpenThermMessageID::TdhwSet,
        (unsigned int)(c * 256));
    xchg_(dhw_req);
    last_dhw_write_ms_ = millis();
  }

  slowRead_();
}
