#include "ot_slave.h"

#if defined(ESP32) && defined(HCS_GW_ENABLE)

namespace hcs {

OtSlave* OtSlave::instance_ = nullptr;

// Static IRAM ISR — same rationale as OtMaster (no std::function in ISR).
static void IRAM_ATTR ot_slave_isr() {
  OtSlave* s = OtSlave::instance();
  if (s) s->ot().handleInterrupt();
}

static void ot_slave_on_response(unsigned long frame,
                                 OpenThermResponseStatus st) {
  OtSlave* s = OtSlave::instance();
  if (s) s->onFrame(frame, st);
}

void OtSlave::begin() {
  instance_ = this;
  // C function ISR + C function response callback (not std::function).
  ot_.begin(ot_slave_isr, ot_slave_on_response);
}

void OtSlave::onFrame(unsigned long frame, OpenThermResponseStatus st) {
  seen_++;
  if (st != OpenThermResponseStatus::SUCCESS) return;
  valid_++;
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
