#include "ot_slave.h"

#if defined(ESP32) && defined(HCS_GW_ENABLE)

namespace hcs {

// ── IRAM decoder (no library calls; register-level pin reads) ────────────
// Replicates the library slave-side edge state machine bitwise, but in
// isolation so the whole chain is IRAM-safe and the frame handoff is a
// pure volatile latch for loop().
void IRAM_ATTR OtSlave::isr() {
  OtSlave* s = instance_;
  if (!s) return;

  bool lvl = OtSlave::fastRead_((uint8_t)s->in_pin_);
  unsigned long ts = micros();
  switch (s->isr_state_) {
    case IS_READY:  // idle-high; master pulls the start bit LOW
      if (lvl) return;   // ignore spurious rising edges while idle
      s->isr_state_ = COLLECTING;
      s->isr_ts_ = ts;
      s->isr_bits_ = 0;
      s->isr_idx_ = 0;
      break;

    case COLLECTING: {
      unsigned long dt = ts - s->isr_ts_;
      if (dt > 1500) {  // stall → drop the partial frame
        s->isr_state_ = IS_READY;
        break;
      }
      if (s->isr_idx_ < 32) {
        if (dt > 750) {  // data-bit edge (>750 µs gap)
          s->isr_bits_ = (s->isr_bits_ << 1) | (lvl ? 0u : 1u);
          s->isr_idx_++;
          s->isr_ts_ = ts;
        }
        break;
      }
      if (dt > 750) {  // stop bit → frame complete
        s->req_ = s->isr_bits_;
        s->pending_ = true;
        s->isr_state_ = IS_READY;
      }
      break;
    }
  }
}

OtSlave* OtSlave::instance_ = nullptr;

void OtSlave::begin() {
  instance_ = this;
  pinMode(in_pin_, INPUT);
  pinMode(out_pin_, OUTPUT);
  digitalWrite(out_pin_, HIGH);  // bus idle
  // All-IRAM attach — never the library's begin() (it would attach its own
  // std::function-handled interrupt chain back).
  attachInterrupt(digitalPinToInterrupt(in_pin_), isr, CHANGE);
}

void OtSlave::loop() {
  // Self-heal: a COLLECTING frame that stalls (no stop edge) ages out so
  // the decoder can't wedge on bus noise.
  if (isr_state_ == COLLECTING && millis() - last_req_ms_ > 100 && !pending_) {
    isr_state_ = IS_READY;
  }
  service();
}

void OtSlave::service() {
  if (!pending_ || !provider_) return;
  // Respond ≥20 ms after the request ended, mirroring library behaviour.
  if (millis() - last_req_ms_ < 20) return;

  uint32_t req = req_;
  pending_ = false;
  isr_state_ = IS_READY;
  seen_++;
  last_req_ms_ = millis();
  valid_++;
  req_ = req;

  uint32_t resp = provider_(req);
  if (!resp) {
    resp = OpenTherm::buildResponse(
        OpenThermMessageType::UNKNOWN_DATA_ID,
        (OpenThermMessageID)((req >> 16) & 0xFF), 0);
  }
  txAnswer_(resp);
  answered_++;
}

void OtSlave::txAnswer_(uint32_t resp) {
  txBit_(true);  // start
  for (int i = 31; i >= 0; i--) txBit_((resp >> i) & 1UL);
  txBit_(true);  // stop
  digitalWrite(out_pin_, HIGH);  // idle
}

void OtSlave::txBit_(bool high) {
  digitalWrite(out_pin_, high ? LOW : HIGH);
  delayMicroseconds(500);
  digitalWrite(out_pin_, high ? HIGH : LOW);
  delayMicroseconds(500);
}

}  // namespace hcs

#endif  // ESP32 && HCS_GW_ENABLE
