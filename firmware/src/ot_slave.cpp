#include "ot_slave.h"

#if defined(ESP32) && defined(HCS_GW_ENABLE)

namespace hcs {

OtSlave* OtSlave::instance_ = nullptr;
void OtSlave::begin() {
  instance_ = this;
  ot_.begin(
      [](unsigned long frame, OpenThermResponseStatus st) {
        if (instance_) instance_->onFrame(frame, st);
      });
}

void OtSlave::onFrame(unsigned long frame, OpenThermResponseStatus st) {
  seen_++;
  if (st != OpenThermResponseStatus::SUCCESS) return;
  req_ = frame;
  last_req_ms_ = millis();
  pending_ = true;
}

void OtSlave::loop() {
  ot_.process();  // advances RESPONSE_READY -> DELAY -> READY (~20 ms)

  if (pending_ && provider_ && ot_.isReady() &&
      millis() - last_req_ms_ >= 20) {
    uint32_t resp = provider_(req_);
    if (!resp) {
      resp = OpenTherm::buildResponse(
          OpenThermMessageType::UNKNOWN_DATA_ID,
          (OpenThermMessageID)((req_ >> 16) & 0xFF), 0);
    }
    ot_.sendResponse(resp);
    pending_ = false;
  }
}

}  // namespace hcs

#endif  // ESP32 && HCS_GW_ENABLE
