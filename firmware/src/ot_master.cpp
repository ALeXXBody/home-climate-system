#include "ot_master.h"
#include "config.h"

OtMaster::OtMaster(int in_pin, int out_pin) : ot_(in_pin, out_pin) {}

// NOTE: never register a custom ISR function pointer here. On ESP8266 a plain
// function lives in flash and panics ("ISR not in IRAM!") on the first bus
// edge; the library's no-arg begin() attaches its own IRAM-safe handler.
void OtMaster::begin() {
  ot_.begin();
}

void OtMaster::loop() {
  // ISR-driven process() handles bit timing.
}

void OtMaster::setChEnable(bool on) { ch_enable_ = on; }
void OtMaster::setDhwEnable(bool on) { dhw_enable_ = on; }

void OtMaster::setFlowSetpoint(float celsius) {
  if (isnan(celsius)) return;
  if (celsius < 10.0f) celsius = 10.0f;
  if (celsius > 90.0f) celsius = 90.0f;
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
  unsigned long resp = ot_.sendRequest(frame);
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
  unsigned long now = millis();
  if (now - last_poll_ms_ < OT_STATUS_INTERVAL_MS) {
    return;
  }
  last_poll_ms_ = now;

  unsigned long response = ot_.setBoilerStatus(ch_enable_, dhw_enable_, false);
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

  // Outdoor temperature first (MsgID 27) so weather comp can use it this cycle
  unsigned long req = OpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA, OpenThermMessageID::Toutside, 0);
  unsigned long resp = ot_.sendRequest(req);
  if (ot_.getLastResponseStatus() == OpenThermResponseStatus::SUCCESS) {
    snap_.outdoor_temp = OpenTherm::getFloat(resp);
  }

  // Effective flow target: weather-comp curve when active, else manual
  wc_target_ = hcs_weather_comp_target(wc_, snap_.outdoor_temp);
  if (ch_enable_) {
    ot_.setBoilerTemperature(!isnan(wc_target_) ? wc_target_ : flow_setpoint_);
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
  ot_.sendRequest(req);
  (void)mm;
}
