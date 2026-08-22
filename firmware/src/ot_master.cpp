#include "ot_master.h"
#include "config.h"

OtMaster* OtMaster::instance_ = nullptr;

OtMaster::OtMaster(int in_pin, int out_pin) : ot_(in_pin, out_pin) {}

void OtMaster::handleInterrupt() {
  if (instance_) {
    instance_->ot_.process();
  }
}

void OtMaster::begin() {
  instance_ = this;
  ot_.begin(handleInterrupt);
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

void OtMaster::poll() {
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

  if (ch_enable_) {
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

  // Outdoor temperature (MsgID 27) — not all boilers expose this
  unsigned long req = OpenTherm::buildRequest(
      OpenThermMessageType::READ_DATA, OpenThermMessageID::Toutside, 0);
  unsigned long resp = ot_.sendRequest(req);
  if (ot_.getLastResponseStatus() == OpenThermResponseStatus::SUCCESS) {
    snap_.outdoor_temp = OpenTherm::getFloat(resp);
  }

  // Max relative modulation setting (MsgID 14)
  unsigned int mm = OpenTherm::temperatureToData((float)max_mod_);
  // temperatureToData is f8.8 for temps; for % level same f8.8 format works
  req = OpenTherm::buildRequest(OpenThermMessageType::WRITE_DATA,
                                OpenThermMessageID::MaxRelModLevelSetting,
                                (unsigned int)(max_mod_ * 256));
  ot_.sendRequest(req);
  (void)mm;
}
