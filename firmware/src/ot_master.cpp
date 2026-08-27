#include "ot_master.h"
#include "config.h"

// How many consecutive Status failures before UI shows "OT no link".
// Single timeouts are normal on a busy bus and must not flicker the badge.
static constexpr uint8_t kOtLinkFailThreshold = 3;
// Hold "linked" this long after last good Status even if later optional
// reads time out (optional IDs often T/O on boilers without those sensors).
static constexpr unsigned long kOtLinkHoldMs = 5000;

OtMaster::OtMaster(int in_pin, int out_pin) : ot_(in_pin, out_pin) {}

// NOTE: never register a custom ISR function pointer here. On ESP8266 a plain
// function lives in flash and panics ("ISR not in IRAM!") on the first bus
// edge; the library's no-arg begin() attaches its own IRAM-safe handler.
void OtMaster::begin() {
  ot_.begin();
}

// Single chokepoint for every bus exchange: OT console + yield so WiFi/HTTP
// keep running during multi-frame polls (each frame can block ≤~1.1 s).
unsigned long OtMaster::xchg_(unsigned long req) {
  unsigned long resp = ot_.sendRequest(req);
  ot_log.record(req, resp, ot_.getLastResponseStatus());
  yield();
  return resp;
}

void OtMaster::loop() {
  // Library process() is driven inside sendRequest(); nothing extra here.
}

void OtMaster::setChEnable(bool on) { ch_enable_ = on; }
void OtMaster::setDhwEnable(bool on) { dhw_enable_ = on; }

void OtMaster::setFlowSetpoint(float celsius) {
  if (isnan(celsius)) return;
  float lo = snap_.valid_maxtset_bounds ? (float)snap_.maxtset_lb : 10.0f;
  float hi = snap_.valid_maxtset_bounds ? (float)snap_.maxtset_ub : 90.0f;
  if (lo < 1) lo = 1;
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

void OtMaster::noteStatusOk_(unsigned long now) {
  status_fail_streak_ = 0;
  snap_.valid = true;
  snap_.last_ok_ms = now;
}

void OtMaster::noteStatusFail_() {
  if (status_fail_streak_ < 255) status_fail_streak_++;
  if (status_fail_streak_ >= kOtLinkFailThreshold) {
    snap_.valid = false;
  } else if (snap_.last_ok_ms != 0 &&
             (millis() - snap_.last_ok_ms) > kOtLinkHoldMs) {
    snap_.valid = false;
  }
  // else keep previous valid=true so the UI does not flicker on one blip
}

void OtMaster::writeTSet_() {
  if (!ch_enable_) return;
  float target = flow_setpoint_;
  if (!failsafe_) {
    float wc = hcs_weather_comp_target(wc_, snap_.outdoor_temp);
    if (!isnan(wc)) {
      wc_target_ = wc;
      target = wc;
    } else {
      wc_target_ = NAN;
    }
  } else {
    wc_target_ = NAN;
  }
  unsigned int data = OpenTherm::temperatureToData(target);
  xchg_(OpenTherm::buildRequest(OpenThermMessageType::WRITE_DATA,
                                OpenThermMessageID::TSet, data));
}

void OtMaster::readFloat_(OpenThermMessageID id, float& dest, float lo,
                          float hi, bool* from_ot_flag) {
  unsigned long resp = xchg_(OpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA, id, 0));
  if (!xchgOk_() || OpenTherm::getDataID(resp) != id) {
    if (from_ot_flag) *from_ot_flag = false;
    return;
  }
  if (OpenTherm::getMessageType(resp) != OpenThermMessageType::READ_ACK) {
    if (from_ot_flag) *from_ot_flag = false;
    return;
  }
  float t = OpenTherm::getFloat(resp);
  if (isnan(t) || t < lo || t > hi) {
    if (from_ot_flag) *from_ot_flag = false;
    return;
  }
  // Reject pure zero payload (library/default empty frame).
  if (t == 0.0f && (resp & 0xFFFF) == 0) {
    if (from_ot_flag) *from_ot_flag = false;
    return;
  }
  dest = t;
  if (from_ot_flag) *from_ot_flag = true;
}

#ifdef HCS_GW_ENABLE
unsigned long OtMaster::sendRaw(unsigned long frame) {
  unsigned long resp = xchg_(frame);
  if (ot_.getLastResponseStatus() == OpenThermResponseStatus::SUCCESS) {
    noteStatusOk_(millis());
    if (OpenTherm::getMessageType(resp) >= OpenThermMessageType::READ_ACK) {
      switch ((int)OpenTherm::getDataID(resp)) {
        case (int)OpenThermMessageID::Tboiler:
          snap_.flow_temp = OpenTherm::getFloat(resp);
          break;
        case (int)OpenThermMessageID::Tret: {
          float rt = OpenTherm::getFloat(resp);
          if (!isnan(rt) && rt > 0.0f && rt < 110.0f) {
            snap_.return_temp = rt;
            snap_.return_from_ot = true;
          }
          break;
        }
        case (int)OpenThermMessageID::Toutside: {
          float ot_out = OpenTherm::getFloat(resp);
          if (!isnan(ot_out) && ot_out > -40.0f && ot_out < 60.0f) {
            snap_.outdoor_temp = ot_out;
            snap_.outdoor_from_ot = true;
          }
          break;
        }
        case (int)OpenThermMessageID::RelModLevel:
          snap_.modulation = OpenTherm::getFloat(resp);
          break;
        default:
          break;
      }
    }
    if (OpenTherm::getDataID(resp) == OpenThermMessageID::Status &&
        OpenTherm::getMessageType(resp) == OpenThermMessageType::READ_ACK) {
      snap_.fault = OpenTherm::isFault(resp);
      snap_.ch_active = OpenTherm::isCentralHeatingActive(resp);
      snap_.dhw_active = OpenTherm::isHotWaterActive(resp);
      snap_.flame = OpenTherm::isFlameOn(resp);
    }
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
    noteStatusFail_();
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
#ifdef HCS_GW_ENABLE
  if (autopoll_) return;
#endif
  unsigned long now = millis();
  if (now - last_poll_ms_ < OT_STATUS_INTERVAL_MS) return;
  last_poll_ms_ = now;

  unsigned long response = xchg_(OpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA, OpenThermMessageID::Status,
      (ch_enable_ ? 0x0100 : 0) | (dhw_enable_ ? 0x0200 : 0)));
  if (!xchgOk_()) {
    noteStatusFail_();
    return;
  }
  noteStatusOk_(now);
  snap_.fault = OpenTherm::isFault(response);
  snap_.ch_active = OpenTherm::isCentralHeatingActive(response);
  snap_.dhw_active = OpenTherm::isHotWaterActive(response);
  snap_.flame = OpenTherm::isFlameOn(response);

  // One optional diagnostic per reference tick (keep bus light).
  if ((rot_++ % 2) == 0) {
    unsigned long r5 = xchg_(OpenTherm::buildRequest(
        OpenThermMessageType::READ_DATA, OpenThermMessageID::ASFflags, 0));
    if (xchgOk_() && OpenTherm::getDataID(r5) == OpenThermMessageID::ASFflags) {
      snap_.asf_flags = (uint8_t)OpenTherm::getUInt(r5);
      snap_.valid_asf = true;
    }
  } else {
    slowRead_();
  }
  applyInject_();
}

void OtMaster::slowRead_() {
  OpenThermMessageID id =
      (OpenThermMessageID)hcs::ot_slow_read_id(slow_cycle_++);
  unsigned long resp = xchg_(
      OpenTherm::buildRequest(OpenThermMessageType::READ_DATA, id, 0));
  if (!xchgOk_() || OpenTherm::getDataID(resp) != id) return;
  uint16_t d = OpenTherm::getUInt(resp);
  switch ((uint16_t)id) {
    case 18:
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
    case 48:
      snap_.dhw_ub = (int8_t)((d >> 8) & 0xFF);
      snap_.dhw_lb = (int8_t)(d & 0xFF);
      snap_.valid_dhw_bounds = true;
      break;
    case 49:
      snap_.maxtset_ub = (int8_t)((d >> 8) & 0xFF);
      snap_.maxtset_lb = (int8_t)(d & 0xFF);
      snap_.valid_maxtset_bounds = true;
      break;
    case 12:
      snap_.fhb_size = d & 0xFF;
      snap_.valid_fhb = true;
      if (!fhb_fetched_once_ ||
          millis() - fhb_last_fetch_ms_ > 3600000UL) {
        fetchFhb_();
        fhb_fetched_once_ = true;
        fhb_last_fetch_ms_ = millis();
      }
      break;
    default:
      break;
  }
}

bool OtMaster::indexedRead_(OpenThermMessageID id, uint8_t index,
                            uint16_t& value) {
  xchg_(OpenTherm::buildRequest(OpenThermMessageType::WRITE_DATA, id,
                                (unsigned int)(index << 8)));
  if (!xchgOk_()) return false;
  unsigned long resp = xchg_(
      OpenTherm::buildRequest(OpenThermMessageType::READ_DATA, id, 0));
  if (!xchgOk_() || OpenTherm::getDataID(resp) != id) return false;
  value = OpenTherm::getUInt(resp);
  return true;
}

void OtMaster::fetchFhb_() {
  if (!snap_.valid_fhb || snap_.fhb_size == 0) return;
  uint8_t want = snap_.fhb_size;
  if (want == 0 || want > sizeof(snap_.fhb_codes)) want = sizeof(snap_.fhb_codes);
  snap_.fhb_count = 0;
  for (uint8_t i = 0; i < want; i++) {
    uint16_t v;
    if (!indexedRead_(OpenThermMessageID::FHBindexFHBvalue, i, v)) break;
    snap_.fhb_codes[snap_.fhb_count++] = v & 0xFF;
  }
}

/**
 * Thin ~1 Hz master cycle.
 *
 * OLD bug: every poll did Status + ASF + OEM + Toutside + TSet + Tboiler +
 * Tret + RelMod + Tdhw + MaxRelMod + slowRead ≈ 11 blocking frames. On a
 * boiler that times out optional IDs that could block 10+ seconds, starve
 * HTTP (web UI "API err"), and clear ot_valid on a single Status blip.
 *
 * NEW: always Status + TSet + Tboiler (3 frames). One optional slot rotates
 * through return/outdoor/mod/dhw/ASF/OEM/MaxRelMod/slowRead. Worst case ~4
 * frames/s so WiFi and the web server stay responsive.
 */
void OtMaster::doPoll_() {
  unsigned long now = millis();
  if (now - last_poll_ms_ < OT_STATUS_INTERVAL_MS) return;
  last_poll_ms_ = now;

  // --- core: Status (link health) ----------------------------------------
  unsigned long response = xchg_(OpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA, OpenThermMessageID::Status,
      (ch_enable_ ? 0x0100 : 0) | (dhw_enable_ ? 0x0200 : 0)));
  if (!xchgOk_()) {
    noteStatusFail_();
    // Still allow 1-Wire backfill so outdoor/return stay live without OT.
    applyInject_();
    return;
  }
  noteStatusOk_(now);
  snap_.fault = OpenTherm::isFault(response);
  snap_.ch_active = OpenTherm::isCentralHeatingActive(response);
  snap_.dhw_active = OpenTherm::isHotWaterActive(response);
  snap_.flame = OpenTherm::isFlameOn(response);

  // Outdoor must be known before WC/TSet when available — inject first so a
  // 1-Wire outdoor can drive the curve even if OT outdoor is on the rotator.
  applyInject_();

  // --- core: CH setpoint write + flow temp -------------------------------
  writeTSet_();

  {
    float t = NAN;
    readFloat_(OpenThermMessageID::Tboiler, t, 0.5f, 110.0f, nullptr);
    if (!isnan(t)) snap_.flow_temp = t;
  }

  // --- one optional slot (round-robin) -----------------------------------
  // Keeps the bus quiet and the HTTP stack fed.
  switch (rot_++ % 8) {
    case 0: {  // return — keep last OT value between slots; expire after 60 s
      bool got = false;
      float t = NAN;
      readFloat_(OpenThermMessageID::Tret, t, 0.5f, 110.0f, &got);
      if (got) {
        snap_.return_temp = t;
        snap_.return_from_ot = true;
        last_return_ot_ms_ = now;
      } else if (snap_.return_from_ot &&
                 now - last_return_ot_ms_ > 60000UL) {
        snap_.return_from_ot = false;
        snap_.return_temp = NAN;
      }
      break;
    }
    case 1: {  // outdoor — keep last OT value between slots; expire after 60 s
      bool got = false;
      float t = NAN;
      readFloat_(OpenThermMessageID::Toutside, t, -40.0f, 60.0f, &got);
      if (got) {
        snap_.outdoor_temp = t;
        snap_.outdoor_from_ot = true;
        last_outdoor_ot_ms_ = now;
      } else if (snap_.outdoor_from_ot &&
                 now - last_outdoor_ot_ms_ > 60000UL) {
        snap_.outdoor_from_ot = false;
        snap_.outdoor_temp = NAN;
      }
      break;
    }
    case 2: {  // modulation
      float t = NAN;
      readFloat_(OpenThermMessageID::RelModLevel, t, 0.0f, 100.0f, nullptr);
      if (!isnan(t)) snap_.modulation = t;
      break;
    }
    case 3: {  // DHW temp
      float t = NAN;
      readFloat_(OpenThermMessageID::Tdhw, t, 0.5f, 100.0f, nullptr);
      if (!isnan(t)) snap_.dhw_temp = t;
      break;
    }
    case 4: {  // ASF flags
      unsigned long r5 = xchg_(OpenTherm::buildRequest(
          OpenThermMessageType::READ_DATA, OpenThermMessageID::ASFflags, 0));
      if (xchgOk_() &&
          OpenTherm::getDataID(r5) == OpenThermMessageID::ASFflags) {
        snap_.asf_flags = (uint8_t)OpenTherm::getUInt(r5);
        snap_.valid_asf = true;
      }
      break;
    }
    case 5: {  // OEM diagnostic
      unsigned long r115 = xchg_(OpenTherm::buildRequest(
          OpenThermMessageType::READ_DATA,
          OpenThermMessageID::OEMDiagnosticCode, 0));
      if (xchgOk_() && OpenTherm::getDataID(r115) ==
                           OpenThermMessageID::OEMDiagnosticCode) {
        snap_.oem_diag = (uint16_t)OpenTherm::getUInt(r115);
        snap_.valid_oem = true;
      }
      break;
    }
    case 6: {  // max relative modulation (write, throttled)
      if (millis() - last_mm_write_ms_ >= 10000UL) {
        xchg_(OpenTherm::buildRequest(
            OpenThermMessageType::WRITE_DATA,
            OpenThermMessageID::MaxRelModLevelSetting,
            (unsigned int)(max_mod_ * 256)));
        last_mm_write_ms_ = millis();
      }
      // DHW setpoint write shares this slot when due
      if (!isnan(dhw_setpoint_) && snap_.valid_dhw_bounds &&
          millis() - last_dhw_write_ms_ >= 60000UL) {
        float c = hcs::ot_clamp_with_bounds(dhw_setpoint_, true, snap_.dhw_lb,
                                            snap_.dhw_ub, 35, 60);
        xchg_(OpenTherm::buildRequest(OpenThermMessageType::WRITE_DATA,
                                      OpenThermMessageID::TdhwSet,
                                      (unsigned int)(c * 256)));
        last_dhw_write_ms_ = millis();
      }
      break;
    }
    default:
      slowRead_();
      break;
  }

  // Final inject after optional return/outdoor may have updated OT flags.
  applyInject_();
}
